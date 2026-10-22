#include "lim/blob.hpp"
#include "lim/capacity.hpp"
#include "onyx/mesh.hpp"
#include "onyx/resources.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/container/stack_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/preprocessor/system.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/utils/limits.hpp"

namespace Lim
{
using Onyx::D3;

namespace
{
constexpr u32 PADDING_CELLS = 1;
constexpr u32 SAMPLE_CANDIDATES = 512;
constexpr f32 MIN_CELL_SIZE = 1e-3f;
constexpr f32 MIN_EXTENT = 1e-6f;
constexpr f32 MIN_TRIANGLE_NORMAL = 1e-12f;
constexpr f32 MIN_GRADIENT = 1e-12f;
constexpr f32 GRADIENT_STEP = 1e-3f;
constexpr f32 VERTEX_BIAS = 0.05f;
constexpr f32 HEX_COSINE = 0.8660254f;
constexpr f32 HEX_SINE = 0.5f;
constexpr f32 HEX_HALF_EDGE = 0.57735027f;
constexpr f32 HEX_CIRCUMRADIUS = 1.1547005f;
constexpr f32v3 FALLBACK_NORMAL{0.f, 1.f, 0.f};
constexpr TKit::FixedArray<f32v3, 4> TETRAHEDRON{f32v3{1.f, -1.f, -1.f}, f32v3{-1.f, -1.f, 1.f}, f32v3{-1.f, 1.f, -1.f},
                                                 f32v3{1.f, 1.f, 1.f}};

struct SampleGrid
{
    f32v3 Origin;
    f32 CellSize;
    u32v3 Cells;
    TKit::StackArray<f32> Samples{};

    [[nodiscard]] u32 PointIndex(const u32v3 &point) const
    {
        return point[0] + (Cells[0] + 1) * (point[1] + (Cells[1] + 1) * point[2]);
    }
    [[nodiscard]] u32 CellIndex(const u32v3 &cell) const
    {
        return cell[0] + Cells[0] * (cell[1] + Cells[1] * cell[2]);
    }
    [[nodiscard]] f32v3 Position(const u32v3 &point) const
    {
        return Origin + f32v3(point) * CellSize;
    }
};

struct Crossing
{
    f32v3 Position;
    f32v3 Normal;
};

[[nodiscard]] f32 PrimitiveReach(const BlobPrimitive &primitive)
{
    const f32v3 &extent = primitive.Extent;
    switch (primitive.Kind)
    {
    case BlobPrimitive::Shape::Sphere:
        return extent[0];
    case BlobPrimitive::Shape::Box:
    case BlobPrimitive::Shape::Wedge:
    case BlobPrimitive::Shape::Frame:
    case BlobPrimitive::Shape::Lattice:
        return TKit::Math::Norm(extent);
    case BlobPrimitive::Shape::Capsule:
    case BlobPrimitive::Shape::Torus:
        return extent[0] + extent[1];
    case BlobPrimitive::Shape::Cylinder:
    case BlobPrimitive::Shape::Ring:
        return TKit::Math::Norm(f32v2{extent[0], extent[1]});
    case BlobPrimitive::Shape::Prism:
        return TKit::Math::Norm(f32v2{HEX_CIRCUMRADIUS * extent[0], extent[1]});
    }
    TKIT_UNREACHABLE();
}

[[nodiscard]] f32 ExcessDistance(const f32v3 &excess)
{
    const f32 inside = TKit::Math::Min(TKit::Math::Max(excess[0], TKit::Math::Max(excess[1], excess[2])), 0.f);
    return TKit::Math::Norm(TKit::Math::Max(excess, f32v3{0.f})) + inside;
}

[[nodiscard]] f32 PlanarDistance(const f32v2 &excess)
{
    const f32 inside = TKit::Math::Min(TKit::Math::Max(excess[0], excess[1]), 0.f);
    return TKit::Math::Norm(TKit::Math::Max(excess, f32v2{0.f})) + inside;
}

[[nodiscard]] f32 PrimitiveDistance(const BlobPrimitive &primitive, const f32v3 &point)
{
    const f32v3 local = TKit::Math::Conjugate(primitive.Rotation) * (point - primitive.Center);
    const f32v3 &extent = primitive.Extent;
    const f32 wall = primitive.Wall;
    switch (primitive.Kind)
    {
    case BlobPrimitive::Shape::Sphere:
        return TKit::Math::Norm(local) - extent[0];
    case BlobPrimitive::Shape::Box:
        return ExcessDistance(TKit::Math::Absolute(local) - extent);
    case BlobPrimitive::Shape::Capsule: {
        const f32v3 spine{0.f, TKit::Math::Clamp(local[1], -extent[1], extent[1]), 0.f};
        return TKit::Math::Norm(local - spine) - extent[0];
    }
    case BlobPrimitive::Shape::Torus: {
        const f32 ring = TKit::Math::Norm(f32v2{local[0], local[2]}) - extent[0];
        return TKit::Math::Norm(f32v2{ring, local[1]}) - extent[1];
    }
    case BlobPrimitive::Shape::Cylinder: {
        const f32 radial = TKit::Math::Norm(f32v2{local[0], local[2]}) - extent[0];
        return PlanarDistance(f32v2{radial, TKit::Math::Absolute(local[1]) - extent[1]});
    }
    case BlobPrimitive::Shape::Prism: {
        f32 across = TKit::Math::Absolute(local[2]);
        f32 flat = TKit::Math::Absolute(local[0]);
        const f32 fold = 2.f * TKit::Math::Min(HEX_SINE * flat - HEX_COSINE * across, 0.f);
        across += fold * HEX_COSINE;
        flat -= fold * HEX_SINE;
        const f32 edge = HEX_HALF_EDGE * extent[0];
        const f32 beyond = flat - extent[0];
        const f32 side = TKit::Math::Norm(f32v2{across - TKit::Math::Clamp(across, -edge, edge), beyond});
        return PlanarDistance(f32v2{beyond < 0.f ? -side : side, TKit::Math::Absolute(local[1]) - extent[1]});
    }
    case BlobPrimitive::Shape::Wedge: {
        const f32 slope = (local[0] * extent[2] + local[2] * extent[0]) /
                          TKit::Math::Max(TKit::Math::Norm(f32v2{extent[2], extent[0]}), MIN_EXTENT);
        return TKit::Math::Max(ExcessDistance(TKit::Math::Absolute(local) - extent), slope);
    }
    case BlobPrimitive::Shape::Frame: {
        const f32v2 window{TKit::Math::Absolute(local[0]) - (extent[0] - wall),
                           TKit::Math::Absolute(local[2]) - (extent[2] - wall)};
        return TKit::Math::Max(ExcessDistance(TKit::Math::Absolute(local) - extent), -PlanarDistance(window));
    }
    case BlobPrimitive::Shape::Ring: {
        const f32 radial = TKit::Math::Norm(f32v2{local[0], local[2]}) - (extent[0] - 0.5f * wall);
        return PlanarDistance(
            f32v2{TKit::Math::Absolute(radial) - 0.5f * wall, TKit::Math::Absolute(local[1]) - extent[1]});
    }
    case BlobPrimitive::Shape::Lattice: {
        const f32 beam = 0.5f * wall;
        const f32v3 outer = TKit::Math::Absolute(local) - extent;
        const f32v3 inner = TKit::Math::Absolute(outer + beam) - beam;
        const f32 alongX = ExcessDistance(f32v3{outer[0], inner[1], inner[2]});
        const f32 alongY = ExcessDistance(f32v3{inner[0], outer[1], inner[2]});
        const f32 alongZ = ExcessDistance(f32v3{inner[0], inner[1], outer[2]});
        return TKit::Math::Min(alongX, TKit::Math::Min(alongY, alongZ));
    }
    }
    TKIT_UNREACHABLE();
}

[[nodiscard]] f32 SmoothMin(const f32 first, const f32 second, const f32 blendRadius)
{
    const f32 nearest = TKit::Math::Min(first, second);
    if (blendRadius <= 0.f)
        return nearest;
    const f32 blend = TKit::Math::Max(blendRadius - TKit::Math::Absolute(first - second), 0.f) / blendRadius;
    return nearest - blend * blend * blendRadius * 0.25f;
}

[[nodiscard]] SampleGrid SampleField(const Blob &blob, const f32v3 &low, const f32v3 &high, const u32 resolution)
{
    const f32v3 size = high - low;
    const u32 innerCells = resolution - 2 * PADDING_CELLS;

    SampleGrid grid{};
    grid.CellSize = TKit::Math::Max(TKit::Math::Max(size) / f32(innerCells), MIN_CELL_SIZE);
    grid.Origin = low - grid.CellSize * f32(PADDING_CELLS);
    for (u32 axis = 0; axis < 3; ++axis)
        grid.Cells[axis] = TKit::Math::Min(u32(size[axis] / grid.CellSize) + 1, innerCells) + 2 * PADDING_CELLS;

    grid.Samples.Resize((grid.Cells[0] + 1) * (grid.Cells[1] + 1) * (grid.Cells[2] + 1));
    for (u32 z = 0; z <= grid.Cells[2]; ++z)
        for (u32 y = 0; y <= grid.Cells[1]; ++y)
            for (u32 x = 0; x <= grid.Cells[0]; ++x)
            {
                const u32v3 point{x, y, z};
                grid.Samples[grid.PointIndex(point)] = blob.Distance(grid.Position(point));
            }
    return grid;
}

template <typename Visit> void ForEachSignChange(const SampleGrid &grid, Visit &&visit)
{
    for (u32 z = 0; z <= grid.Cells[2]; ++z)
        for (u32 y = 0; y <= grid.Cells[1]; ++y)
            for (u32 x = 0; x <= grid.Cells[0]; ++x)
                for (u32 axis = 0; axis < 3; ++axis)
                {
                    const u32v3 point{x, y, z};
                    if (point[axis] >= grid.Cells[axis])
                        continue;
                    u32v3 next = point;
                    next[axis] += 1;
                    const f32 fromDistance = grid.Samples[grid.PointIndex(point)];
                    const f32 toDistance = grid.Samples[grid.PointIndex(next)];
                    if ((fromDistance < 0.f) != (toDistance < 0.f))
                        visit(point, next, axis, fromDistance, toDistance);
                }
}

void PlaceCellVertices(const SampleGrid &grid, const TKit::StackArray<u32> &edgeCrossings,
                       const TKit::StackArray<Crossing> &crossings, TKit::StackArray<f32v3> &cellPoints)
{
    for (u32 z = 0; z < grid.Cells[2]; ++z)
        for (u32 y = 0; y < grid.Cells[1]; ++y)
            for (u32 x = 0; x < grid.Cells[0]; ++x)
            {
                const u32v3 cell{x, y, z};
                u32 insideCorners = 0;
                for (u32 corner = 0; corner < 8; ++corner)
                {
                    const u32v3 offset{corner & 1U, (corner >> 1U) & 1U, (corner >> 2U) & 1U};
                    insideCorners += grid.Samples[grid.PointIndex(cell + offset)] < 0.f ? 1 : 0;
                }
                if (insideCorners == 0 || insideCorners == 8)
                    continue;

                f32v3 mass{0.f};
                f32v3 planeTarget{0.f};
                f32m3 planes{0.f};
                u32 count = 0;
                for (u32 corner = 0; corner < 8; ++corner)
                    for (u32 axis = 0; axis < 3; ++axis)
                    {
                        if (corner & (1U << axis))
                            continue;
                        const u32v3 from = cell + u32v3{corner & 1U, (corner >> 1U) & 1U, (corner >> 2U) & 1U};
                        u32v3 to = from;
                        to[axis] += 1;
                        if ((grid.Samples[grid.PointIndex(from)] < 0.f) == (grid.Samples[grid.PointIndex(to)] < 0.f))
                            continue;
                        const Crossing &crossing = crossings[edgeCrossings[3 * grid.PointIndex(from) + axis]];
                        const f32v3 &normal = crossing.Normal;
                        for (u32 column = 0; column < 3; ++column)
                            for (u32 row = 0; row < 3; ++row)
                                planes[column][row] += normal[column] * normal[row];
                        planeTarget += normal * TKit::Math::Dot(normal, crossing.Position);
                        mass += crossing.Position;
                        ++count;
                    }
                const f32v3 center = mass / f32(count);
                const f32m3 system = planes + f32m3::Identity(VERTEX_BIAS);
                const f32v3 shift = TKit::Math::Inverse(system) * (planeTarget - planes * center);
                const f32v3 low = grid.Position(cell);
                const f32v3 high = low + grid.CellSize;
                const f32v3 vertex = center + shift;
                const bool contained = TKit::Math::Min(vertex - low) >= 0.f && TKit::Math::Min(high - vertex) >= 0.f;
                cellPoints[grid.CellIndex(cell)] = contained ? vertex : center;
            }
}

void AddTriangle(Onyx::StaticMeshData<D3> &mesh, const f32v3 &first, const f32v3 &second, const f32v3 &third)
{
    const f32v3 normal = TKit::Math::Cross(second - first, third - first);
    if (TKit::Math::NormSquared(normal) <= MIN_TRIANGLE_NORMAL)
        return;
    const f32v3 outward = TKit::Math::Normalize(normal);
    const f32v4 tangent{TKit::Math::Normalize(second - first), 1.f};
    const Onyx::Index base = mesh.Vertices.GetSize();
    mesh.Vertices.Append(Onyx::StaticVertex<D3>{first, f32v2{0.f}, outward, tangent});
    mesh.Vertices.Append(Onyx::StaticVertex<D3>{second, f32v2{0.f}, outward, tangent});
    mesh.Vertices.Append(Onyx::StaticVertex<D3>{third, f32v2{0.f}, outward, tangent});
    mesh.Indices.Append(base);
    mesh.Indices.Append(base + 1);
    mesh.Indices.Append(base + 2);
}

[[nodiscard]] f32 Alignment(const f32v3 &first, const f32v3 &second, const f32v3 &third, const f32v3 &normal)
{
    const f32v3 facing = TKit::Math::Cross(second - first, third - first);
    return TKit::Math::Dot(facing, normal) / TKit::Math::Max(TKit::Math::Norm(facing), MIN_EXTENT);
}

/// Emits one quad per sign-changing edge, split along the diagonal whose triangles best face the edge's true normal.
void Polygonize(const SampleGrid &grid, const TKit::StackArray<u32> &edgeCrossings,
                const TKit::StackArray<Crossing> &crossings, const TKit::StackArray<f32v3> &cellPoints,
                Onyx::StaticMeshData<D3> &mesh)
{
    ForEachSignChange(grid, [&](const u32v3 &point, const u32v3 &, const u32 axis, const f32 fromDistance, const f32) {
        const u32 side = (axis + 1) % 3;
        const u32 up = (axis + 2) % 3;
        if (point[side] == 0 || point[up] == 0 || point[side] >= grid.Cells[side] || point[up] >= grid.Cells[up])
            return;

        u32v3 sideStep{0U};
        sideStep[side] = 1;
        u32v3 upStep{0U};
        upStep[up] = 1;
        const f32v3 &corner = cellPoints[grid.CellIndex(point - sideStep - upStep)];
        const f32v3 &below = cellPoints[grid.CellIndex(point - upStep)];
        const f32v3 &center = cellPoints[grid.CellIndex(point)];
        const f32v3 &beside = cellPoints[grid.CellIndex(point - sideStep)];
        const bool inward = fromDistance < 0.f;
        const f32v3 &second = inward ? below : beside;
        const f32v3 &fourth = inward ? beside : below;

        const f32v3 &normal = crossings[edgeCrossings[3 * grid.PointIndex(point) + axis]].Normal;
        const f32 throughCenter =
            TKit::Math::Min(Alignment(corner, second, center, normal), Alignment(corner, center, fourth, normal));
        const f32 throughSides =
            TKit::Math::Min(Alignment(corner, second, fourth, normal), Alignment(second, center, fourth, normal));
        if (throughCenter >= throughSides)
        {
            AddTriangle(mesh, corner, second, center);
            AddTriangle(mesh, corner, center, fourth);
        }
        else
        {
            AddTriangle(mesh, corner, second, fourth);
            AddTriangle(mesh, second, center, fourth);
        }
    });
}

/// Farthest-point sampling over an even stride of the surface crossings, so the points cover corners and faces alike.
void SpreadSamplePoints(const TKit::StackArray<Crossing> &crossings,
                        TKit::StaticArray<f32v3, Blob::MAX_SAMPLE_POINTS> &samplePoints)
{
    const u32 stride = (u32(crossings.GetSize()) + SAMPLE_CANDIDATES - 1) / SAMPLE_CANDIDATES;
    TKit::StackArray<f32v3> candidates{};
    candidates.Reserve((u32(crossings.GetSize()) + stride - 1) / stride);
    for (u32 i = 0; i < crossings.GetSize(); i += stride)
        candidates.Append(crossings[i].Position);
    TKit::StackArray<f32> nearest{};
    nearest.Resize(candidates.GetSize(), TKIT_F32_MAX);

    u32 pick = 0;
    for (u32 i = 1; i < candidates.GetSize(); ++i)
        if (TKit::Math::NormSquared(candidates[i]) > TKit::Math::NormSquared(candidates[pick]))
            pick = i;

    const usize count = TKit::Math::Min(candidates.GetSize(), usize(Blob::MAX_SAMPLE_POINTS));
    while (samplePoints.GetSize() < count)
    {
        const f32v3 chosen = candidates[pick];
        samplePoints.Append(chosen);
        f32 farthest = -1.f;
        for (u32 i = 0; i < candidates.GetSize(); ++i)
        {
            nearest[i] = TKit::Math::Min(nearest[i], TKit::Math::NormSquared(candidates[i] - chosen));
            if (nearest[i] > farthest)
            {
                farthest = nearest[i];
                pick = i;
            }
        }
    }
}

[[nodiscard]] Onyx::StaticMeshData<D3> BuildSurface(const Blob &blob, const SampleGrid &grid,
                                                    TKit::StaticArray<f32v3, Blob::MAX_SAMPLE_POINTS> &samplePoints)
{
    Onyx::StaticMeshData<D3> mesh{};
    mesh.Flags = Onyx::MeshDataFlag_BackCulled;

    u32 crossingCount = 0;
    ForEachSignChange(grid, [&](const u32v3 &, const u32v3 &, const u32, const f32, const f32) { ++crossingCount; });
    if (crossingCount == 0)
        return mesh;

    TKit::StackArray<u32> edgeCrossings{};
    edgeCrossings.Resize(3 * grid.Samples.GetSize());
    TKit::StackArray<Crossing> crossings{};
    crossings.Reserve(crossingCount);
    ForEachSignChange(
        grid, [&](const u32v3 &from, const u32v3 &to, const u32 axis, const f32 fromDistance, const f32 toDistance) {
            const f32 t = fromDistance / (fromDistance - toDistance);
            const f32v3 estimate = TKit::Math::LinearLerp(grid.Position(from), grid.Position(to), t);
            const f32v3 normal = blob.Gradient(estimate);
            edgeCrossings[3 * grid.PointIndex(from) + axis] = u32(crossings.GetSize());
            crossings.Append(Crossing{.Position = estimate - normal * blob.Distance(estimate), .Normal = normal});
        });

    TKit::StackArray<f32v3> cellPoints{};
    cellPoints.Resize(grid.Cells[0] * grid.Cells[1] * grid.Cells[2]);
    PlaceCellVertices(grid, edgeCrossings, crossings, cellPoints);
    Polygonize(grid, edgeCrossings, crossings, cellPoints, mesh);
    SpreadSamplePoints(crossings, samplePoints);
    return mesh;
}
} // namespace

Blob::Blob(const Onyx::ResourcePool pool, const TKit::Span<const BlobPrimitive> primitives, const f32 blendRadius,
           const u32 meshResolution)
    : m_BlendRadius(TKit::Math::Max(blendRadius, 0.f))
{
    TKIT_ASSERT(!primitives.IsEmpty() && primitives.GetSize() <= MAX_PRIMITIVES,
                "[LIM][BLOB] A blob needs between 1 and {} primitives, got {}", MAX_PRIMITIVES, primitives.GetSize());

    f32v3 low{TKIT_F32_MAX};
    f32v3 high{-TKIT_F32_MAX};
    for (u32 i = 0; i < primitives.GetSize(); ++i)
    {
        const BlobPrimitive &primitive = primitives[i];
        m_Primitives.Append(primitive);
        if (i != 0 && primitive.Operation != BlobPrimitive::Op::Union)
            continue;
        const f32 reach = PrimitiveReach(primitive);
        low = TKit::Math::Min(low, primitive.Center - reach);
        high = TKit::Math::Max(high, primitive.Center + reach);
        m_BoundingRadius = TKit::Math::Max(m_BoundingRadius, TKit::Math::Norm(primitive.Center) + reach);
    }

    const u32 resolution = TKit::Math::Clamp(meshResolution, MIN_MESH_RESOLUTION, MAX_MESH_RESOLUTION);
    const SampleGrid grid = SampleField(*this, low - m_BlendRadius, high + m_BlendRadius, resolution);
    m_BoundingRadius += m_BlendRadius + grid.CellSize;

    const f32 cellVolume = grid.CellSize * grid.CellSize * grid.CellSize;
    for (const f32 sample : grid.Samples)
        m_Volume += TKit::Math::Clamp(0.5f - sample / grid.CellSize, 0.f, 1.f) * cellVolume;

    const Onyx::StaticMeshData<D3> mesh = BuildSurface(*this, grid, m_SamplePoints);
    if (!mesh.Vertices.IsEmpty())
        m_Mesh = Onyx::Resources::RegisterMesh(pool, mesh);
}

f32 Blob::Evaluate(const TKit::Span<const BlobPrimitive> primitives, const f32 blendRadius, const f32v3 &localPoint)
{
    f32 distance = PrimitiveDistance(primitives[0], localPoint);
    for (u32 i = 1; i < primitives.GetSize(); ++i)
    {
        const BlobPrimitive &primitive = primitives[i];
        const f32 other = PrimitiveDistance(primitive, localPoint);
        switch (primitive.Operation)
        {
        case BlobPrimitive::Op::Union:
            distance = SmoothMin(distance, other, blendRadius);
            break;
        case BlobPrimitive::Op::Subtract:
            distance = -SmoothMin(-distance, other, blendRadius);
            break;
        case BlobPrimitive::Op::Intersect:
            distance = -SmoothMin(-distance, -other, blendRadius);
            break;
        }
    }
    return distance;
}

f32 Blob::Distance(const f32v3 &localPoint) const
{
    return Evaluate(m_Primitives, m_BlendRadius, localPoint);
}

f32v3 Blob::Gradient(const f32v3 &localPoint) const
{
    f32v3 gradient{0.f};
    for (const f32v3 &offset : TETRAHEDRON)
        gradient += offset * Distance(localPoint + offset * GRADIENT_STEP);
    if (TKit::Math::NormSquared(gradient) > MIN_GRADIENT)
        return TKit::Math::Normalize(gradient);
    if (TKit::Math::NormSquared(localPoint) > MIN_GRADIENT)
        return TKit::Math::Normalize(localPoint);
    return FALLBACK_NORMAL;
}

f32 Blob::GetBoundingRadius() const
{
    return m_BoundingRadius;
}

TKit::Span<const f32v3> Blob::GetSamplePoints() const
{
    return m_SamplePoints;
}

f32 Blob::GetVolume() const
{
    return m_Volume;
}

void Blob::Draw(Onyx::RenderContext<D3> &context, const f32m4 &transform) const
{
    if (m_Mesh)
        context.StaticMesh(*m_Mesh, transform);
}
} // namespace Lim

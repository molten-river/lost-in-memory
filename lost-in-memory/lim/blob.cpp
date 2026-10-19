#include "lim/blob.hpp"
#include "onyx/mesh.hpp"
#include "onyx/resources.hpp"
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
constexpr u32 BLOB_GRID = 24;
constexpr u32 PADDING_CELLS = 1;
constexpr u32 INNER_CELLS = BLOB_GRID - 2 * PADDING_CELLS;
constexpr f32 MIN_CELL_SIZE = 1e-3f;
constexpr f32 MIN_TRIANGLE_NORMAL = 1e-12f;

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

[[nodiscard]] f32 PrimitiveReach(const BlobPrimitive &primitive)
{
    const f32v3 &extent = primitive.Extent;
    switch (primitive.Kind)
    {
    case BlobPrimitive::Shape::Sphere:
        return extent[0];
    case BlobPrimitive::Shape::Box:
        return TKit::Math::Norm(extent);
    case BlobPrimitive::Shape::Capsule:
    case BlobPrimitive::Shape::Torus:
        return extent[0] + extent[1];
    }
    TKIT_UNREACHABLE();
}

[[nodiscard]] f32 PrimitiveDistance(const BlobPrimitive &primitive, const f32v3 &point)
{
    const f32v3 local = TKit::Math::Conjugate(primitive.Rotation) * (point - primitive.Center);
    const f32v3 &extent = primitive.Extent;
    switch (primitive.Kind)
    {
    case BlobPrimitive::Shape::Sphere:
        return TKit::Math::Norm(local) - extent[0];
    case BlobPrimitive::Shape::Box: {
        const f32v3 excess = TKit::Math::Absolute(local) - extent;
        const f32 outside = TKit::Math::Norm(TKit::Math::Max(excess, f32v3{0.f}));
        const f32 inside = TKit::Math::Min(TKit::Math::Max(excess[0], TKit::Math::Max(excess[1], excess[2])), 0.f);
        return outside + inside;
    }
    case BlobPrimitive::Shape::Capsule: {
        const f32v3 spine{0.f, TKit::Math::Clamp(local[1], -extent[1], extent[1]), 0.f};
        return TKit::Math::Norm(local - spine) - extent[0];
    }
    case BlobPrimitive::Shape::Torus: {
        const f32 ring = TKit::Math::Norm(f32v2{local[0], local[2]}) - extent[0];
        return TKit::Math::Norm(f32v2{ring, local[1]}) - extent[1];
    }
    }
    TKIT_UNREACHABLE();
}

[[nodiscard]] SampleGrid SampleField(const Blob &blob, const f32v3 &low, const f32v3 &high)
{
    const f32v3 size = high - low;

    SampleGrid grid{};
    grid.CellSize = TKit::Math::Max(TKit::Math::Max(size) / f32(INNER_CELLS), MIN_CELL_SIZE);
    grid.Origin = low - grid.CellSize * f32(PADDING_CELLS);
    for (u32 axis = 0; axis < 3; ++axis)
        grid.Cells[axis] = TKit::Math::Min(u32(size[axis] / grid.CellSize) + 1, INNER_CELLS) + 2 * PADDING_CELLS;

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

void PlaceCellVertices(const SampleGrid &grid, TKit::StackArray<f32v3> &cellPoints)
{
    for (u32 z = 0; z < grid.Cells[2]; ++z)
        for (u32 y = 0; y < grid.Cells[1]; ++y)
            for (u32 x = 0; x < grid.Cells[0]; ++x)
            {
                const u32v3 cell{x, y, z};
                f32v3 sum{0.f};
                u32 crossings = 0;
                for (u32 corner = 0; corner < 8; ++corner)
                    for (u32 axis = 0; axis < 3; ++axis)
                    {
                        if (corner & (1U << axis))
                            continue;
                        const u32v3 from = cell + u32v3{corner & 1U, (corner >> 1U) & 1U, (corner >> 2U) & 1U};
                        u32v3 to = from;
                        to[axis] += 1;
                        const f32 fromDistance = grid.Samples[grid.PointIndex(from)];
                        const f32 toDistance = grid.Samples[grid.PointIndex(to)];
                        if ((fromDistance < 0.f) == (toDistance < 0.f))
                            continue;
                        const f32 t = fromDistance / (fromDistance - toDistance);
                        sum += TKit::Math::LinearLerp(grid.Position(from), grid.Position(to), t);
                        ++crossings;
                    }
                if (crossings != 0)
                    cellPoints[grid.CellIndex(cell)] = sum / f32(crossings);
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

[[nodiscard]] Onyx::StaticMeshData<D3> Polygonize(const SampleGrid &grid)
{
    TKit::StackArray<f32v3> cellPoints{};
    cellPoints.Resize(grid.Cells[0] * grid.Cells[1] * grid.Cells[2]);
    PlaceCellVertices(grid, cellPoints);

    Onyx::StaticMeshData<D3> mesh{};
    mesh.Flags = Onyx::MeshDataFlag_BackCulled;
    for (u32 z = 0; z <= grid.Cells[2]; ++z)
        for (u32 y = 0; y <= grid.Cells[1]; ++y)
            for (u32 x = 0; x <= grid.Cells[0]; ++x)
                for (u32 axis = 0; axis < 3; ++axis)
                {
                    const u32 side = (axis + 1) % 3;
                    const u32 up = (axis + 2) % 3;
                    const u32v3 point{x, y, z};
                    if (point[axis] >= grid.Cells[axis] || point[side] == 0 || point[up] == 0 ||
                        point[side] >= grid.Cells[side] || point[up] >= grid.Cells[up])
                        continue;

                    u32v3 next = point;
                    next[axis] += 1;
                    const f32 fromDistance = grid.Samples[grid.PointIndex(point)];
                    const f32 toDistance = grid.Samples[grid.PointIndex(next)];
                    if ((fromDistance < 0.f) == (toDistance < 0.f))
                        continue;

                    u32v3 sideStep{0U};
                    sideStep[side] = 1;
                    u32v3 upStep{0U};
                    upStep[up] = 1;
                    const f32v3 &corner = cellPoints[grid.CellIndex(point - sideStep - upStep)];
                    const f32v3 &below = cellPoints[grid.CellIndex(point - upStep)];
                    const f32v3 &center = cellPoints[grid.CellIndex(point)];
                    const f32v3 &beside = cellPoints[grid.CellIndex(point - sideStep)];
                    if (fromDistance < 0.f)
                    {
                        AddTriangle(mesh, corner, below, center);
                        AddTriangle(mesh, corner, center, beside);
                    }
                    else
                    {
                        AddTriangle(mesh, corner, center, below);
                        AddTriangle(mesh, corner, beside, center);
                    }
                }
    return mesh;
}
} // namespace

Blob::Blob(const Onyx::ResourcePool pool, const TKit::Span<const BlobPrimitive> primitives, const f32 blendRadius)
    : m_BlendRadius(TKit::Math::Max(blendRadius, 0.f))
{
    TKIT_ASSERT(!primitives.IsEmpty() && primitives.GetSize() <= MAX_PRIMITIVES,
                "[LIM][BLOB] A blob needs between 1 and {} primitives, got {}", MAX_PRIMITIVES, primitives.GetSize());

    f32v3 low{TKIT_F32_MAX};
    f32v3 high{-TKIT_F32_MAX};
    for (const BlobPrimitive &primitive : primitives)
    {
        m_Primitives.Append(primitive);
        const f32 reach = PrimitiveReach(primitive);
        low = TKit::Math::Min(low, primitive.Center - reach);
        high = TKit::Math::Max(high, primitive.Center + reach);
        m_BoundingRadius = TKit::Math::Max(m_BoundingRadius, TKit::Math::Norm(primitive.Center) + reach);
    }

    const SampleGrid grid = SampleField(*this, low - m_BlendRadius, high + m_BlendRadius);
    m_BoundingRadius += m_BlendRadius + grid.CellSize;

    const Onyx::StaticMeshData<D3> mesh = Polygonize(grid);
    if (!mesh.Vertices.IsEmpty())
        m_Mesh = Onyx::Resources::RegisterMesh(pool, mesh);
}

f32 Blob::Distance(const f32v3 &localPoint) const
{
    f32 distance = PrimitiveDistance(m_Primitives[0], localPoint);
    for (u32 i = 1; i < m_Primitives.GetSize(); ++i)
    {
        const f32 other = PrimitiveDistance(m_Primitives[i], localPoint);
        const f32 nearest = TKit::Math::Min(distance, other);
        if (m_BlendRadius <= 0.f)
        {
            distance = nearest;
            continue;
        }
        const f32 blend = TKit::Math::Max(m_BlendRadius - TKit::Math::Absolute(distance - other), 0.f) / m_BlendRadius;
        distance = nearest - blend * blend * m_BlendRadius * 0.25f;
    }
    return distance;
}

f32 Blob::GetBoundingRadius() const
{
    return m_BoundingRadius;
}

void Blob::Draw(Onyx::RenderContext<D3> &context, const f32m4 &transform) const
{
    if (m_Mesh)
        context.StaticMesh(*m_Mesh, transform);
}
} // namespace Lim

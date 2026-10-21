#include "lim/convex_hull.hpp"

#include "tkit/math/math.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/utils/limits.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
namespace
{
constexpr u32 SIMPLEX_CORNERS = 4;
constexpr f32 RELATIVE_TOLERANCE = 1e-4f;
constexpr f32 MIN_AREA_FRACTION = 1e-6f;
constexpr f32 PLANE_MERGE_COSINE = 0.99f;
constexpr f32 TETRAHEDRON_VOLUME_SHARE = 1.f / 6.f;
constexpr f32v3 FALLBACK_NORMAL{0.f, 1.f, 0.f};

using Simplex = TKit::FixedArray<u8, SIMPLEX_CORNERS>;

struct Face
{
    TKit::FixedArray<u8, 3> Corners;
    f32v3 Normal;
    f32 Offset;
    bool IsAlive;
    bool IsVisible;
};

struct Edge
{
    u8 From;
    u8 To;
};

/// A closed triangle mesh under construction; EdgeFaces maps each directed edge to the face that owns it.
struct HullMesh
{
    TKit::FixedArray<Face, ConvexHull::MAX_TRIANGLES> Faces{};
    TKit::FixedArray<TKit::FixedArray<u8, ConvexHull::MAX_POINTS>, ConvexHull::MAX_POINTS> EdgeFaces{};
    u32 AliveCount = 0;
};

[[nodiscard]] f32 Height(const Face &face, const f32v3 &point)
{
    return TKit::Math::Dot(face.Normal, point) - face.Offset;
}

void AddFace(HullMesh &mesh, const TKit::Span<const f32v3> points, const u8 first, const u8 second, const u8 third)
{
    u32 slot = 0;
    while (mesh.Faces[slot].IsAlive)
        ++slot;

    const f32v3 &origin = points[first];
    const f32v3 normal = TKit::Math::Cross(points[second] - origin, points[third] - origin);
    const f32 length = TKit::Math::Norm(normal);

    Face &face = mesh.Faces[slot];
    face.Corners = {first, second, third};
    face.Normal = length > 0.f ? normal / length : f32v3{0.f};
    face.Offset = TKit::Math::Dot(face.Normal, origin);
    face.IsAlive = true;
    face.IsVisible = false;
    mesh.EdgeFaces[first][second] = u8(slot);
    mesh.EdgeFaces[second][third] = u8(slot);
    mesh.EdgeFaces[third][first] = u8(slot);
    ++mesh.AliveCount;
}

/// Two extremes along the widest axis, the point farthest from their line, then the one farthest from that plane.
[[nodiscard]] TKit::Optional<Simplex> FindSimplex(const TKit::Span<const f32v3> points, const f32v3 &extent,
                                                  const f32 tolerance)
{
    u32 axis = 0;
    for (u32 i = 1; i < 3; ++i)
        if (extent[i] > extent[axis])
            axis = i;
    if (extent[axis] <= tolerance)
        return TKit::Optional<Simplex>::None();

    u8 first = 0;
    u8 second = 0;
    for (u32 i = 1; i < points.GetSize(); ++i)
    {
        if (points[i][axis] < points[first][axis])
            first = u8(i);
        if (points[i][axis] > points[second][axis])
            second = u8(i);
    }

    const f32v3 &origin = points[first];
    const f32v3 direction = TKit::Math::Normalize(points[second] - origin);
    u8 third = 0;
    f32 farthest = 0.f;
    for (u32 i = 0; i < points.GetSize(); ++i)
    {
        const f32 offLine = TKit::Math::NormSquared(TKit::Math::Cross(points[i] - origin, direction));
        if (offLine > farthest)
        {
            farthest = offLine;
            third = u8(i);
        }
    }
    if (farthest <= tolerance * tolerance)
        return TKit::Optional<Simplex>::None();

    const f32v3 normal = TKit::Math::Normalize(TKit::Math::Cross(points[second] - origin, points[third] - origin));
    u8 fourth = 0;
    f32 deepest = 0.f;
    for (u32 i = 0; i < points.GetSize(); ++i)
    {
        const f32 offPlane = TKit::Math::Dot(normal, points[i] - origin);
        if (TKit::Math::Absolute(offPlane) > TKit::Math::Absolute(deepest))
        {
            deepest = offPlane;
            fourth = u8(i);
        }
    }
    if (TKit::Math::Absolute(deepest) <= tolerance)
        return TKit::Optional<Simplex>::None();

    if (deepest > 0.f)
        return Simplex{first, third, second, fourth};
    return Simplex{first, second, third, fourth};
}

/// Grows the hull to an eye beyond tolerance: every connected face it stands above becomes a fan to the horizon.
void AddPoint(HullMesh &mesh, const TKit::Span<const f32v3> points, const u8 eye, const f32 tolerance)
{
    const f32v3 &point = points[eye];
    TKit::Optional<u32> seed = TKit::Optional<u32>::None();
    f32 highest = tolerance;
    for (u32 i = 0; i < ConvexHull::MAX_TRIANGLES; ++i)
    {
        Face &face = mesh.Faces[i];
        face.IsVisible = false;
        if (!face.IsAlive)
            continue;
        const f32 height = Height(face, point);
        if (height > highest)
        {
            highest = height;
            seed = i;
        }
    }
    if (!seed)
        return;

    TKit::StaticArray<u8, ConvexHull::MAX_TRIANGLES> pending{};
    TKit::StaticArray<u8, ConvexHull::MAX_TRIANGLES> visible{};
    mesh.Faces[*seed].IsVisible = true;
    pending.Append(u8(*seed));
    while (!pending.IsEmpty())
    {
        const u8 index = pending.GetBack();
        pending.Pop();
        visible.Append(index);
        const Face &face = mesh.Faces[index];
        for (u32 edge = 0; edge < 3; ++edge)
        {
            const u8 across = mesh.EdgeFaces[face.Corners[(edge + 1) % 3]][face.Corners[edge]];
            Face &neighbour = mesh.Faces[across];
            if (neighbour.IsAlive && !neighbour.IsVisible && Height(neighbour, point) > 0.f)
            {
                neighbour.IsVisible = true;
                pending.Append(across);
            }
        }
    }

    TKit::StaticArray<Edge, 3 * ConvexHull::MAX_TRIANGLES> horizon{};
    for (const u8 index : visible)
    {
        const Face &face = mesh.Faces[index];
        for (u32 edge = 0; edge < 3; ++edge)
        {
            const u8 from = face.Corners[edge];
            const u8 to = face.Corners[(edge + 1) % 3];
            const Face &neighbour = mesh.Faces[mesh.EdgeFaces[to][from]];
            if (!neighbour.IsAlive || !neighbour.IsVisible)
                horizon.Append(Edge{.From = from, .To = to});
        }
    }
    if (mesh.AliveCount - visible.GetSize() + horizon.GetSize() > ConvexHull::MAX_TRIANGLES)
        return;

    for (const u8 index : visible)
        mesh.Faces[index].IsAlive = false;
    mesh.AliveCount -= u32(visible.GetSize());
    for (const Edge &edge : horizon)
        AddFace(mesh, points, edge.From, edge.To, eye);
}
} // namespace

void ConvexHull::Build(const TKit::Span<const f32v3> points)
{
    TKIT_ASSERT(points.GetSize() <= MAX_POINTS, "[LIM][HULL] A hull takes at most {} points, got {}", MAX_POINTS,
                points.GetSize());
    m_Vertices.Clear();
    m_Triangles.Clear();
    m_Planes.Clear();
    m_Volume = 0.f;
    if (points.IsEmpty())
        return;

    f32v3 low = points[0];
    f32v3 high = points[0];
    for (const f32v3 &point : points)
    {
        low = TKit::Math::Min(low, point);
        high = TKit::Math::Max(high, point);
    }
    const f32 diagonal = TKit::Math::Norm(high - low);
    const f32 tolerance = RELATIVE_TOLERANCE * diagonal;

    const TKit::Optional<Simplex> simplex = FindSimplex(points, high - low, tolerance);
    if (!simplex)
        return;

    HullMesh mesh{};
    const Simplex &corners = *simplex;
    AddFace(mesh, points, corners[0], corners[1], corners[2]);
    AddFace(mesh, points, corners[0], corners[3], corners[1]);
    AddFace(mesh, points, corners[1], corners[3], corners[2]);
    AddFace(mesh, points, corners[2], corners[3], corners[0]);
    for (u32 i = 0; i < points.GetSize(); ++i)
        AddPoint(mesh, points, u8(i), tolerance);

    TKit::FixedArray<bool, MAX_POINTS> isUsed{};
    for (const Face &face : mesh.Faces)
        if (face.IsAlive)
            for (const u8 corner : face.Corners)
                isUsed[corner] = true;

    TKit::FixedArray<u8, MAX_POINTS> remap{};
    for (u32 i = 0; i < points.GetSize(); ++i)
        if (isUsed[i])
        {
            remap[i] = u8(m_Vertices.GetSize());
            m_Vertices.Append(points[i]);
        }

    const f32v3 &origin = m_Vertices[0];
    const f32 minArea = MIN_AREA_FRACTION * diagonal * diagonal;
    for (const Face &face : mesh.Faces)
    {
        if (!face.IsAlive)
            continue;
        const f32v3 &first = points[face.Corners[0]];
        const f32v3 &second = points[face.Corners[1]];
        const f32v3 &third = points[face.Corners[2]];
        m_Volume += TKit::Math::Dot(first - origin, TKit::Math::Cross(second - origin, third - origin)) *
                    TETRAHEDRON_VOLUME_SHARE;
        if (TKit::Math::Norm(TKit::Math::Cross(second - first, third - first)) <= minArea)
            continue;

        const auto holdsFace = [&](const Plane &plane) {
            if (TKit::Math::Dot(plane.Normal, face.Normal) < PLANE_MERGE_COSINE)
                return false;
            for (const u8 corner : face.Corners)
                if (TKit::Math::Absolute(TKit::Math::Dot(plane.Normal, points[corner]) - plane.Offset) > tolerance)
                    return false;
            return true;
        };
        u32 plane = 0;
        while (plane < m_Planes.GetSize() && !holdsFace(m_Planes[plane]))
            ++plane;
        if (plane == m_Planes.GetSize())
            m_Planes.Append(Plane{.Normal = face.Normal, .Offset = face.Offset});
        m_Triangles.Append(Triangle{.Corners = {remap[face.Corners[0]], remap[face.Corners[1]], remap[face.Corners[2]]},
                                    .Plane = u8(plane)});
    }
    m_Volume = TKit::Math::Max(m_Volume, 0.f);
}

f32 ConvexHull::Distance(const f32v3 &point) const
{
    if (m_Planes.IsEmpty())
        return TKit::Limits<f32>::Max();
    f32 distance = TKit::Limits<f32>::Min();
    for (const Plane &plane : m_Planes)
        distance = TKit::Math::Max(distance, TKit::Math::Dot(plane.Normal, point) - plane.Offset);
    return distance;
}

f32v3 ConvexHull::Gradient(const f32v3 &point) const
{
    f32v3 normal = FALLBACK_NORMAL;
    f32 highest = TKit::Limits<f32>::Min();
    for (const Plane &plane : m_Planes)
    {
        const f32 height = TKit::Math::Dot(plane.Normal, point) - plane.Offset;
        if (height > highest)
        {
            highest = height;
            normal = plane.Normal;
        }
    }
    return normal;
}

f32 ConvexHull::GetVolume() const
{
    return m_Volume;
}

TKit::Span<const f32v3> ConvexHull::GetVertices() const
{
    return m_Vertices;
}
} // namespace Lim

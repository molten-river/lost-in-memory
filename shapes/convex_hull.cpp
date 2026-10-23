#include "shapes/convex_hull.hpp"

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
constexpr f32 OVERLAP_SHRINK = 0.999f;
constexpr f32 DEGENERATE_SQUARED = 1e-18f;
constexpr u32 MAX_OVERLAP_ITERATIONS = 32;
/// The faces through a tetrahedron's newest corner, each as its two other corners and the corner opposite it.
constexpr TKit::FixedArray<TKit::FixedArray<u32, 3>, 3> TETRAHEDRON_FACES{
    TKit::FixedArray<u32, 3>{1, 2, 3}, TKit::FixedArray<u32, 3>{2, 3, 1}, TKit::FixedArray<u32, 3>{3, 1, 2}};

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

/// Up to four Minkowski difference points, the newest first.
struct SupportSimplex
{
    TKit::FixedArray<f32v3, SIMPLEX_CORNERS> Points{};
    u32 Count = 0;
};

[[nodiscard]] f32v3 FindSupport(const ConvexHull &hull, const f32m4 &pose, const f32v3 &direction)
{
    const f32v3 local{TKit::Math::Dot(f32v3(pose[0]), direction), TKit::Math::Dot(f32v3(pose[1]), direction),
                      TKit::Math::Dot(f32v3(pose[2]), direction)};
    const TKit::Span<const f32v3> vertices = hull.GetVertices();
    u32 best = 0;
    f32 highest = TKit::Math::Dot(vertices[0], local);
    for (u32 i = 1; i < vertices.GetSize(); ++i)
    {
        const f32 height = TKit::Math::Dot(vertices[i], local);
        if (height > highest)
        {
            highest = height;
            best = i;
        }
    }
    const f32v3 shrunk = hull.GetCenter() + (vertices[best] - hull.GetCenter()) * OVERLAP_SHRINK;
    return f32v3(pose * f32v4{shrunk, 1.f});
}

[[nodiscard]] bool IsDegenerate(const f32v3 &direction)
{
    return TKit::Math::NormSquared(direction) <= DEGENERATE_SQUARED;
}

/// Keeps the segment or its newest end, whichever faces the origin; a vanishing direction means the origin is on it.
[[nodiscard]] bool RefineLine(SupportSimplex &simplex, f32v3 &direction)
{
    const f32v3 newest = simplex.Points[0];
    const f32v3 edge = simplex.Points[1] - newest;
    const f32v3 toOrigin = -newest;
    if (TKit::Math::Dot(edge, toOrigin) > 0.f)
    {
        simplex.Count = 2;
        direction = TKit::Math::Cross(TKit::Math::Cross(edge, toOrigin), edge);
    }
    else
    {
        simplex.Count = 1;
        direction = toOrigin;
    }
    return IsDegenerate(direction);
}

[[nodiscard]] bool RefineTriangle(SupportSimplex &simplex, f32v3 &direction)
{
    const f32v3 newest = simplex.Points[0];
    const f32v3 second = simplex.Points[1];
    const f32v3 third = simplex.Points[2];
    const f32v3 toSecond = second - newest;
    const f32v3 toThird = third - newest;
    const f32v3 toOrigin = -newest;
    const f32v3 normal = TKit::Math::Cross(toSecond, toThird);

    if (TKit::Math::Dot(TKit::Math::Cross(normal, toThird), toOrigin) > 0.f)
    {
        if (TKit::Math::Dot(toThird, toOrigin) > 0.f)
        {
            simplex.Points[1] = third;
            simplex.Count = 2;
            direction = TKit::Math::Cross(TKit::Math::Cross(toThird, toOrigin), toThird);
            return IsDegenerate(direction);
        }
        return RefineLine(simplex, direction);
    }
    if (TKit::Math::Dot(TKit::Math::Cross(toSecond, normal), toOrigin) > 0.f)
        return RefineLine(simplex, direction);

    simplex.Count = 3;
    if (TKit::Math::Dot(normal, toOrigin) > 0.f)
        direction = normal;
    else
    {
        simplex.Points[1] = third;
        simplex.Points[2] = second;
        direction = -normal;
    }
    return IsDegenerate(direction);
}

/// Each face normal is turned away from the corner opposite it, so the winding of the inputs never matters.
[[nodiscard]] bool RefineTetrahedron(SupportSimplex &simplex, f32v3 &direction)
{
    const f32v3 newest = simplex.Points[0];
    for (const TKit::FixedArray<u32, 3> &face : TETRAHEDRON_FACES)
    {
        const f32v3 second = simplex.Points[face[0]];
        const f32v3 third = simplex.Points[face[1]];
        const f32v3 opposite = simplex.Points[face[2]];
        f32v3 normal = TKit::Math::Cross(second - newest, third - newest);
        if (TKit::Math::Dot(normal, opposite - newest) > 0.f)
            normal = -normal;
        if (TKit::Math::Dot(normal, -newest) > 0.f)
        {
            simplex.Points = {newest, second, third, f32v3{0.f}};
            simplex.Count = 3;
            return RefineTriangle(simplex, direction);
        }
    }
    return true;
}

[[nodiscard]] bool RefineSimplex(SupportSimplex &simplex, f32v3 &direction)
{
    if (simplex.Count == 2)
        return RefineLine(simplex, direction);
    if (simplex.Count == 3)
        return RefineTriangle(simplex, direction);
    return RefineTetrahedron(simplex, direction);
}
} // namespace

void ConvexHull::Build(const TKit::Span<const f32v3> points)
{
    TKIT_ASSERT(points.GetSize() <= MAX_POINTS, "[LIM][HULL] A hull takes at most {} points, got {}", MAX_POINTS,
                points.GetSize());
    m_Vertices.Clear();
    m_Triangles.Clear();
    m_Planes.Clear();
    m_Center = f32v3{0.f};
    m_Volume = 0.f;
    m_Radius = 0.f;
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

    for (const f32v3 &vertex : m_Vertices)
        m_Center += vertex;
    m_Center = m_Center / f32(m_Vertices.GetSize());
    for (const f32v3 &vertex : m_Vertices)
        m_Radius = TKit::Math::Max(m_Radius, TKit::Math::Norm(vertex - m_Center));
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

bool ConvexHull::IsOverlapping(const f32m4 &pose, const ConvexHull &other, const f32m4 &otherPose) const
{
    if (m_Vertices.IsEmpty() || other.m_Vertices.IsEmpty())
        return false;

    const auto findDifference = [&](const f32v3 &direction) {
        return FindSupport(*this, pose, direction) - FindSupport(other, otherPose, -direction);
    };
    f32v3 direction = f32v3(pose * f32v4{m_Center, 1.f}) - f32v3(otherPose * f32v4{other.m_Center, 1.f});
    if (IsDegenerate(direction))
        direction = FALLBACK_NORMAL;

    SupportSimplex simplex{};
    simplex.Points[0] = findDifference(direction);
    simplex.Count = 1;
    direction = -simplex.Points[0];
    for (u32 iteration = 0; iteration < MAX_OVERLAP_ITERATIONS; ++iteration)
    {
        if (IsDegenerate(direction))
            return true;
        const f32v3 point = findDifference(direction);
        if (TKit::Math::Dot(point, direction) <= 0.f)
            return false;
        for (u32 i = simplex.Count; i > 0; --i)
            simplex.Points[i] = simplex.Points[i - 1];
        simplex.Points[0] = point;
        ++simplex.Count;
        if (RefineSimplex(simplex, direction))
            return true;
    }
    return true;
}

f32 ConvexHull::GetVolume() const
{
    return m_Volume;
}

TKit::Span<const f32v3> ConvexHull::GetVertices() const
{
    return m_Vertices;
}

const f32v3 &ConvexHull::GetCenter() const
{
    return m_Center;
}

f32 ConvexHull::GetRadius() const
{
    return m_Radius;
}
} // namespace Lim

#include "lim/shell.hpp"
#include "lim/capacity.hpp"

#include "onyx/resources.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/container/hash_map.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/utils/limits.hpp"

namespace Lim
{
using Onyx::D3;

namespace
{
struct WaveTerm
{
    f32v3 Axis;
    f32 FrequencyScale;
    f32 SpeedScale;
    f32 Phase;
};

struct NoiseOctave
{
    f32 Frequency;
    f32 Weight;
    f32v3 Drift;
    u32 Seed;
};

constexpr f32 MIN_RADIUS = 4.f;
constexpr u32 MAX_VALENCE = 6;
constexpr u32 MAX_WALK_STEPS = 64;
constexpr f32 MAX_SHEAR = 0.12f;
constexpr f32 MIN_NOISE_SCALE = 1e-3f;
constexpr u32 SUBDIVISION_CHILDREN = 4;
constexpr u32 LATTICE_CORNERS = 8;
constexpr u32 AXES = 3;
constexpr f32 PI = TKit::Math::Pi<f32>();
constexpr f32 TAU = 2.f * PI;
constexpr f32 GOLDEN_RATIO = 1.618034f;

constexpr u32 LATTICE_PRIME_X = 0x8DA6B343U;
constexpr u32 LATTICE_PRIME_Y = 0xD8163841U;
constexpr u32 LATTICE_PRIME_Z = 0xCB1AB31FU;
constexpr u32 AVALANCHE_FIRST = 0x85EBCA6BU;
constexpr u32 AVALANCHE_SECOND = 0xC2B2AE35U;
constexpr u32 BYTE_MASK = 0xFFU;
constexpr f32 BYTE_TO_SIGNED = 2.f / 255.f;

constexpr TKit::FixedArray<u32, 3> NEXT_CORNER{1, 2, 0};

constexpr TKit::FixedArray<WaveTerm, 3> WAVE_TERMS{
    WaveTerm{.Axis = f32v3{0.8f, 0.6f, 0.f}, .FrequencyScale = 1.f, .SpeedScale = 1.f, .Phase = 0.f},
    WaveTerm{.Axis = f32v3{0.f, 0.8f, 0.6f}, .FrequencyScale = 0.7f, .SpeedScale = 0.55f, .Phase = 2.1f},
    WaveTerm{.Axis = f32v3{0.6f, 0.f, 0.8f}, .FrequencyScale = 1.3f, .SpeedScale = 0.8f, .Phase = 4.2f}};

constexpr TKit::FixedArray<NoiseOctave, 2> NOISE_OCTAVES{
    NoiseOctave{.Frequency = 1.f, .Weight = 1.f, .Drift = f32v3{0.6f, 0.48f, 0.64f}, .Seed = 0x9E3779B9U},
    NoiseOctave{.Frequency = 2.3f, .Weight = 0.45f, .Drift = f32v3{-0.54f, 1.2f, -0.72f}, .Seed = 0x7F4A7C15U}};

constexpr TKit::FixedArray<f32v3, 12> ICOSAHEDRON_VERTICES{
    f32v3{-1.f, GOLDEN_RATIO, 0.f},  f32v3{1.f, GOLDEN_RATIO, 0.f},   f32v3{-1.f, -GOLDEN_RATIO, 0.f},
    f32v3{1.f, -GOLDEN_RATIO, 0.f},  f32v3{0.f, -1.f, GOLDEN_RATIO},  f32v3{0.f, 1.f, GOLDEN_RATIO},
    f32v3{0.f, -1.f, -GOLDEN_RATIO}, f32v3{0.f, 1.f, -GOLDEN_RATIO},  f32v3{GOLDEN_RATIO, 0.f, -1.f},
    f32v3{GOLDEN_RATIO, 0.f, 1.f},   f32v3{-GOLDEN_RATIO, 0.f, -1.f}, f32v3{-GOLDEN_RATIO, 0.f, 1.f}};

constexpr TKit::FixedArray<u32v3, 20> ICOSAHEDRON_FACES{
    u32v3{0, 11, 5}, u32v3{0, 5, 1},  u32v3{0, 1, 7},   u32v3{0, 7, 10}, u32v3{0, 10, 11},
    u32v3{1, 5, 9},  u32v3{5, 11, 4}, u32v3{11, 10, 2}, u32v3{10, 7, 6}, u32v3{7, 1, 8},
    u32v3{3, 9, 4},  u32v3{3, 4, 2},  u32v3{3, 2, 6},   u32v3{3, 6, 8},  u32v3{3, 8, 9},
    u32v3{4, 9, 5},  u32v3{2, 4, 11}, u32v3{6, 2, 10},  u32v3{8, 6, 7},  u32v3{9, 8, 1}};

/// Normalised sum of travelling waves over the sphere, in [-1, 1].
[[nodiscard]] f32 EvaluateWave(const f32v3 &direction, const f32 time, const ShellMotionParams &motion)
{
    f32 sum = 0.f;
    for (const WaveTerm &term : WAVE_TERMS)
        sum +=
            TKit::Math::Sine(PI * motion.WaveFrequency * term.FrequencyScale * TKit::Math::Dot(direction, term.Axis) -
                             TAU * motion.WaveSpeed * term.SpeedScale * time + term.Phase);
    return sum / f32(WAVE_TERMS.GetSize());
}

/// Four independent values in [-1, 1] for a lattice cell.
[[nodiscard]] f32v4 HashLattice(const i32v3 &cell, const u32 seed)
{
    u32 hash = u32(cell[0]) * LATTICE_PRIME_X + u32(cell[1]) * LATTICE_PRIME_Y + u32(cell[2]) * LATTICE_PRIME_Z + seed;
    hash ^= hash >> 16;
    hash *= AVALANCHE_FIRST;
    hash ^= hash >> 13;
    hash *= AVALANCHE_SECOND;
    hash ^= hash >> 16;
    const f32v4 bytes{f32(hash & BYTE_MASK), f32((hash >> 8) & BYTE_MASK), f32((hash >> 16) & BYTE_MASK),
                      f32(hash >> 24)};
    return bytes * BYTE_TO_SIGNED - 1.f;
}

/// Four-channel value noise with quintic interpolation between lattice corners.
[[nodiscard]] f32v4 SampleValueNoise(const f32v3 &point, const u32 seed)
{
    i32v3 base{0};
    f32v3 fade{0.f};
    for (u32 axis = 0; axis < AXES; ++axis)
    {
        const f32 truncated = f32(i32(point[axis]));
        const f32 floored = truncated > point[axis] ? truncated - 1.f : truncated;
        const f32 t = point[axis] - floored;
        base[axis] = i32(floored);
        fade[axis] = t * t * t * (t * (t * 6.f - 15.f) + 10.f);
    }

    f32v4 sum{0.f};
    for (u32 corner = 0; corner < LATTICE_CORNERS; ++corner)
    {
        i32v3 cell = base;
        f32 weight = 1.f;
        for (u32 axis = 0; axis < AXES; ++axis)
        {
            const bool upper = ((corner >> axis) & 1U) != 0;
            cell[axis] += upper ? 1 : 0;
            weight *= upper ? fade[axis] : 1.f - fade[axis];
        }
        sum += HashLattice(cell, seed) * weight;
    }
    return sum;
}

/// Weighted octaves of drifting value noise at a rest direction; every channel stays in [-1, 1].
[[nodiscard]] f32v4 SampleNoiseField(const f32v3 &direction, const f32 time, const ShellMotionParams &motion)
{
    f32v4 sum{0.f};
    f32 totalWeight = 0.f;
    for (const NoiseOctave &octave : NOISE_OCTAVES)
    {
        const f32v3 point =
            direction * (motion.NoiseScale * octave.Frequency) + octave.Drift * (motion.NoiseSpeed * time);
        sum += SampleValueNoise(point, octave.Seed) * octave.Weight;
        totalWeight += octave.Weight;
    }
    return sum / totalWeight;
}

[[nodiscard]] u32 PackShellTint(const Palette &colors, const f32 crest)
{
    return Mix(colors.ShellTrough, colors.ShellCrest, crest).ToLinear().Pack();
}
} // namespace

Shell::Shell() : m_Mesh(Onyx::Resources::RegisterDynamicMesh<D3>().Handle)
{
    Onyx::Resources::GetDynamicMeshData<D3>(m_Mesh)->Flags &= ~Onyx::MeshDataFlag_BackCulled;
    Onyx::Resources::Sync(Onyx::SyncFlag_DynamicMeshes);
}

Shell::~Shell()
{
    Onyx::Resources::DestroyDynamicMesh<D3>(m_Mesh);
}

void Shell::Build(const ShellParams &shell, const ShellMotionParams &motion, const Palette &colors,
                  const bool vertexNoise)
{
    m_Shell = shell;
    m_Shell.Subdivisions = TKit::Math::Min(shell.Subdivisions, MAX_SUBDIVISIONS);
    m_Motion = motion;
    m_Colors = colors;
    m_VertexNoise = vertexNoise;
    m_RadialAmplitude = vertexNoise ? motion.NoiseAmplitude * (1.f - motion.TangentialShare) : motion.WaveAmplitude;
    const f32 innerRadius = TKit::Math::Max(m_Shell.Radius - m_RadialAmplitude, 0.f);
    const f32 foldFreeShift = MAX_SHEAR * innerRadius / TKit::Math::Max(motion.NoiseScale, MIN_NOISE_SCALE);
    m_TangentialAmplitude =
        vertexNoise ? TKit::Math::Min(motion.NoiseAmplitude * motion.TangentialShare, foldFreeShift) : 0.f;

    Subdivide();
    LinkNeighbours();

    const u32 triangleCount = m_Triangles.GetSize();
    const u32 vertexCount = m_RestDirections.GetSize();

    f32 minAlignment = 1.f;
    for (u32 triangle = 0; triangle < triangleCount; ++triangle)
        for (u32 corner = 0; corner < 3; ++corner)
            minAlignment = TKit::Math::Min(
                minAlignment, TKit::Math::Dot(m_Centers[triangle], m_RestDirections[m_Triangles[triangle][corner]]));

    m_MinRadius = 0.f;
    if (innerRadius > 0.f)
    {
        const f32 spread =
            TKit::Math::AntiCosine(minAlignment) + TKit::Math::AntiTangent(m_TangentialAmplitude / innerRadius);
        m_MinRadius = innerRadius * TKit::Math::Max(TKit::Math::Cosine(spread), 0.f);
    }

    m_Panels.Clear();
    m_Panels.Resize(triangleCount, Panel::Plain);
    m_PanelColors.Clear();
    m_PanelColors.Resize(triangleCount, m_Colors.Untinted.ToLinear().Pack());
    m_Positions.Resize(vertexCount);
    m_Heights.Resize(vertexCount);

    Onyx::DynamicMeshData<D3> *mesh = Onyx::Resources::GetDynamicMeshData<D3>(m_Mesh);
    const u32 cornerCount = 3 * triangleCount;
    mesh->Vertices.Resize(cornerCount);
    mesh->Indices.Resize(cornerCount);
    for (u32 corner = 0; corner < cornerCount; ++corner)
    {
        mesh->Vertices[corner].TexCoord = f32v2{0.f};
        mesh->Indices[corner] = corner;
    }

    Update(0.f);
}

void Shell::Subdivide()
{
    const u32 scale = 1U << (2 * m_Shell.Subdivisions);
    const u32 triangleCount = ICOSAHEDRON_FACES.GetSize() * scale;
    const u32 vertexCount = 10 * scale + 2;

    m_RestDirections.Clear();
    m_RestDirections.Reserve(vertexCount);
    for (const f32v3 &corner : ICOSAHEDRON_VERTICES)
        m_RestDirections.Append(TKit::Math::Normalize(corner));

    TKit::TierArray<u32v3> faces{};
    faces.Reserve(triangleCount);
    for (const u32v3 &face : ICOSAHEDRON_FACES)
        faces.Append(face);

    TKit::TierArray<u32v3> refined{};
    refined.Reserve(triangleCount);
    TKit::TierHashMap<u64, u32> midpoints{};
    const auto midpoint = [this, &midpoints](const u32 first, const u32 second) {
        const u64 edge = (u64(TKit::Math::Min(first, second)) << 32) | TKit::Math::Max(first, second);
        bool existed = false;
        const u32 index = midpoints.TryInsert(&existed, edge, m_RestDirections.GetSize());
        if (!existed)
        {
            const f32v3 direction = TKit::Math::Normalize(m_RestDirections[first] + m_RestDirections[second]);
            m_RestDirections.Append(direction);
        }
        return index;
    };

    for (u32 level = 0; level < m_Shell.Subdivisions; ++level)
    {
        refined.Clear();
        midpoints.Clear();
        for (const u32v3 &face : faces)
        {
            const u32 ab = midpoint(face[0], face[1]);
            const u32 bc = midpoint(face[1], face[2]);
            const u32 ca = midpoint(face[2], face[0]);
            refined.Append(u32v3{face[0], ab, ca});
            refined.Append(u32v3{face[1], bc, ab});
            refined.Append(u32v3{face[2], ca, bc});
            refined.Append(u32v3{ab, bc, ca});
        }
        faces = refined;
    }

    m_Triangles.Clear();
    m_Triangles.Reserve(triangleCount);
    m_Centers.Clear();
    m_Centers.Reserve(triangleCount);
    for (const u32v3 &face : faces)
    {
        const f32v3 &a = m_RestDirections[face[0]];
        const f32v3 &b = m_RestDirections[face[1]];
        const f32v3 &c = m_RestDirections[face[2]];
        const bool facesOutward = TKit::Math::Dot(a, TKit::Math::Cross(b, c)) > 0.f;
        m_Triangles.Append(facesOutward ? u32v3{face[0], face[2], face[1]} : face);
        m_Centers.Append(TKit::Math::Normalize(a + b + c));
    }
}

void Shell::LinkNeighbours()
{
    using Incidence = TKit::StaticArray<u32, MAX_VALENCE>;
    TKit::TierArray<Incidence> incident{};
    incident.Resize(m_RestDirections.GetSize(), Incidence{});
    for (u32 triangle = 0; triangle < m_Triangles.GetSize(); ++triangle)
        for (u32 corner = 0; corner < 3; ++corner)
            incident[m_Triangles[triangle][corner]].Append(triangle);

    m_Neighbours.Clear();
    m_Neighbours.Resize(m_Triangles.GetSize(), u32v3{0});
    for (u32 triangle = 0; triangle < m_Triangles.GetSize(); ++triangle)
    {
        const u32v3 &corners = m_Triangles[triangle];
        for (u32 edge = 0; edge < 3; ++edge)
        {
            const u32 from = corners[edge];
            for (const u32 candidate : incident[corners[NEXT_CORNER[edge]]])
            {
                const u32v3 &other = m_Triangles[candidate];
                if (candidate != triangle && (other[0] == from || other[1] == from || other[2] == from))
                    m_Neighbours[triangle][edge] = candidate;
            }
        }
    }
}

u32 Shell::GetTriangleCount() const
{
    return m_Triangles.GetSize();
}

void Shell::SetPanel(const u32 triangle, const Panel panel, const Onyx::Color &color)
{
    m_Panels[triangle] = panel;
    m_PanelColors[triangle] = color.ToLinear().Pack();
}

Shell::Panel Shell::GetPanel(const u32 triangle) const
{
    return m_Panels[triangle];
}

void Shell::Update(const f32 time)
{
    if (m_VertexNoise)
        DisplaceByNoise(time);
    else
        DisplaceByWaves(time);

    Onyx::DynamicMeshData<D3> *mesh = Onyx::Resources::GetDynamicMeshData<D3>(m_Mesh);
    for (u32 triangle = 0; triangle < m_Triangles.GetSize(); ++triangle)
    {
        const u32v3 &corners = m_Triangles[triangle];
        const f32v3 &a = m_Positions[corners[0]];
        const f32v3 &b = m_Positions[corners[1]];
        const f32v3 &c = m_Positions[corners[2]];
        const f32v3 normal = TKit::Math::Normalize(TKit::Math::Cross(b - a, c - a));
        const f32v4 tangent{TKit::Math::Normalize(b - a), 1.f};
        const bool isPlain = m_Panels[triangle] == Panel::Plain;
        const bool tintsVertices = isPlain && !m_VertexNoise;

        u32 facetColor = m_PanelColors[triangle];
        if (isPlain && m_VertexNoise)
        {
            const f32 meanHeight = (m_Heights[corners[0]] + m_Heights[corners[1]] + m_Heights[corners[2]]) / 3.f;
            facetColor = PackShellTint(m_Colors, 0.5f * (meanHeight * m_Motion.FacetTintStrength + 1.f));
        }

        for (u32 corner = 0; corner < 3; ++corner)
        {
            Onyx::DynamicVertex<D3> &vertex = mesh->Vertices[3 * triangle + corner];
            vertex.Position = m_Positions[corners[corner]];
            vertex.Normal = normal;
            vertex.Tangent = tangent;
            vertex.Color =
                tintsVertices ? PackShellTint(m_Colors, 0.5f * (m_Heights[corners[corner]] + 1.f)) : facetColor;
        }
    }
}

void Shell::DisplaceByWaves(const f32 time)
{
    for (u32 vertex = 0; vertex < m_RestDirections.GetSize(); ++vertex)
    {
        const f32v3 &direction = m_RestDirections[vertex];
        const f32 wave = EvaluateWave(direction, time, m_Motion);
        m_Positions[vertex] = direction * (m_Shell.Radius + m_Motion.WaveAmplitude * wave);
        m_Heights[vertex] = wave;
    }
}

void Shell::DisplaceByNoise(const f32 time)
{
    for (u32 vertex = 0; vertex < m_RestDirections.GetSize(); ++vertex)
    {
        const f32v3 &direction = m_RestDirections[vertex];
        const f32v4 field = SampleNoiseField(direction, time, m_Motion);
        const f32v3 shove{field[1], field[2], field[3]};
        f32v3 slide = shove - direction * TKit::Math::Dot(shove, direction);
        const f32 slideLengthSquared = TKit::Math::NormSquared(slide);
        if (slideLengthSquared > 1.f)
            slide = slide / TKit::Math::SquareRoot(slideLengthSquared);

        m_Positions[vertex] =
            direction * (m_Shell.Radius + m_RadialAmplitude * field[0]) + slide * m_TangentialAmplitude;
        m_Heights[vertex] = field[0];
    }
}

void Shell::Draw(Onyx::RenderContext<D3> &context) const
{
    context.Push();
    context.FillColor(m_Colors.Untinted);
    context.DynamicMesh(m_Mesh);
    context.Pop();
}

f32 Shell::GetRadius(const f32v3 &direction) const
{
    const u32v3 &corners = m_Triangles[GetTriangleAt(direction)];
    const f32v3 &a = m_Positions[corners[0]];
    const f32v3 &b = m_Positions[corners[1]];
    const f32v3 &c = m_Positions[corners[2]];
    const f32v3 normal = TKit::Math::Cross(b - a, c - a);
    const f32 alignment = TKit::Math::Dot(direction, normal);
    if (TKit::Math::ApproachesZero(alignment))
        return TKit::Math::Norm(a);
    return TKit::Math::Dot(a, normal) / alignment;
}

u32 Shell::GetTriangleAt(const f32v3 &direction) const
{
    TKIT_ASSERT(!m_Triangles.IsEmpty(), "[LIM][SHELL] The shell must be built before querying its triangles");
    u32 triangle = FindRestTriangle(direction);
    u32 closest = triangle;
    f32 closestExcess = TKit::Limits<f32>::Max();
    for (u32 step = 0; step < MAX_WALK_STEPS; ++step)
    {
        const u32v3 &corners = m_Triangles[triangle];
        u32 exitEdge = 0;
        f32 excess = TKit::Limits<f32>::Min();
        for (u32 edge = 0; edge < 3; ++edge)
        {
            const f32 side = TKit::Math::Dot(
                direction, TKit::Math::Cross(m_Positions[corners[edge]], m_Positions[corners[NEXT_CORNER[edge]]]));
            if (side > excess)
            {
                excess = side;
                exitEdge = edge;
            }
        }
        if (excess <= 0.f)
            return triangle;
        if (excess < closestExcess)
        {
            closest = triangle;
            closestExcess = excess;
        }
        triangle = m_Neighbours[triangle][exitEdge];
    }
    return closest;
}

u32 Shell::FindRestTriangle(const f32v3 &direction) const
{
    u32 triangle = 0;
    f32 bestAlignment = TKit::Limits<f32>::Min();
    for (u32 face = 0; face < ICOSAHEDRON_FACES.GetSize(); ++face)
    {
        const u32v3 &corners = ICOSAHEDRON_FACES[face];
        const f32 alignment = TKit::Math::Dot(direction, m_RestDirections[corners[0]] + m_RestDirections[corners[1]] +
                                                             m_RestDirections[corners[2]]);
        if (alignment > bestAlignment)
        {
            triangle = face;
            bestAlignment = alignment;
        }
    }

    const u32v3 &root = ICOSAHEDRON_FACES[triangle];
    f32v3 a = m_RestDirections[root[0]];
    f32v3 b = m_RestDirections[root[1]];
    f32v3 c = m_RestDirections[root[2]];
    const f32 orientation = TKit::Math::Dot(a, TKit::Math::Cross(b, c)) > 0.f ? 1.f : -1.f;
    for (u32 level = 0; level < m_Shell.Subdivisions; ++level)
    {
        const f32v3 ab = TKit::Math::Normalize(a + b);
        const f32v3 bc = TKit::Math::Normalize(b + c);
        const f32v3 ca = TKit::Math::Normalize(c + a);
        triangle *= SUBDIVISION_CHILDREN;
        if (orientation * TKit::Math::Dot(direction, TKit::Math::Cross(ca, ab)) < 0.f)
        {
            b = ab;
            c = ca;
        }
        else if (orientation * TKit::Math::Dot(direction, TKit::Math::Cross(ab, bc)) < 0.f)
        {
            triangle += 1;
            a = b;
            b = bc;
            c = ab;
        }
        else if (orientation * TKit::Math::Dot(direction, TKit::Math::Cross(bc, ca)) < 0.f)
        {
            triangle += 2;
            a = c;
            b = ca;
            c = bc;
        }
        else
        {
            triangle += 3;
            a = ab;
            b = bc;
            c = ca;
        }
    }
    return triangle;
}

f32 Shell::GetMinRadius() const
{
    return m_MinRadius;
}

f32v3 Shell::GetTriangleCenter(const u32 triangle) const
{
    return m_Centers[triangle];
}

ShellParams Sanitize(ShellParams params)
{
    params.Radius = TKit::Math::Max(params.Radius, MIN_RADIUS);
    params.Subdivisions = TKit::Math::Min(params.Subdivisions, MAX_SUBDIVISIONS);
    params.LethalDensity = TKit::Math::Clamp(params.LethalDensity, 0.f, 1.f);
    return params;
}

ShellMotionParams Sanitize(ShellMotionParams params)
{
    params.WaveAmplitude = TKit::Math::Max(params.WaveAmplitude, 0.f);
    params.NoiseAmplitude = TKit::Math::Max(params.NoiseAmplitude, 0.f);
    params.NoiseSpeed = TKit::Math::Max(params.NoiseSpeed, 0.f);
    params.NoiseScale = TKit::Math::Max(params.NoiseScale, 0.f);
    params.TangentialShare = TKit::Math::Clamp(params.TangentialShare, 0.f, 1.f);
    params.FacetTintStrength = TKit::Math::Max(params.FacetTintStrength, 0.f);
    return params;
}
} // namespace Lim

#include "lim/shell.hpp"

#include "lim/palette.hpp"

#include "onyx/resources.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/container/hash_map.hpp"
#include "tkit/math/math.hpp"
#include "tkit/utils/debug.hpp"

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

constexpr u32 MAX_SUBDIVISIONS = 5;
constexpr f32 PI = TKit::Math::Pi<f32>();
constexpr f32 TAU = 2.f * PI;
constexpr f32 GOLDEN_RATIO = 1.618034f;

constexpr TKit::FixedArray<WaveTerm, 3> WAVE_TERMS{
    WaveTerm{.Axis = f32v3{0.8f, 0.6f, 0.f}, .FrequencyScale = 1.f, .SpeedScale = 1.f, .Phase = 0.f},
    WaveTerm{.Axis = f32v3{0.f, 0.8f, 0.6f}, .FrequencyScale = 0.7f, .SpeedScale = 0.55f, .Phase = 2.1f},
    WaveTerm{.Axis = f32v3{0.6f, 0.f, 0.8f}, .FrequencyScale = 1.3f, .SpeedScale = 0.8f, .Phase = 4.2f}};

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
f32 EvaluateWave(const f32v3 &direction, const f32 time, const ShellParams &params)
{
    f32 sum = 0.f;
    for (const WaveTerm &term : WAVE_TERMS)
        sum +=
            TKit::Math::Sine(PI * params.WaveFrequency * term.FrequencyScale * TKit::Math::Dot(direction, term.Axis) -
                             TAU * params.WaveSpeed * term.SpeedScale * time + term.Phase);
    return sum / f32(WAVE_TERMS.GetSize());
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

void Shell::Build(const ShellParams &params)
{
    m_Params = params;
    m_Params.Subdivisions = TKit::Math::Min(params.Subdivisions, MAX_SUBDIVISIONS);

    const u32 scale = 1U << (2 * m_Params.Subdivisions);
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

    for (u32 level = 0; level < m_Params.Subdivisions; ++level)
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

    m_Panels.Clear();
    m_Panels.Resize(triangleCount, Panel::Plain);
    m_PanelColors.Clear();
    m_PanelColors.Resize(triangleCount, UNTINTED.ToLinear().Pack());
    m_Positions.Resize(vertexCount);
    m_Tints.Resize(vertexCount);

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
    for (u32 vertex = 0; vertex < m_RestDirections.GetSize(); ++vertex)
    {
        const f32v3 &direction = m_RestDirections[vertex];
        const f32 wave = EvaluateWave(direction, time, m_Params);
        m_Positions[vertex] = direction * (m_Params.Radius + m_Params.WaveAmplitude * wave);
        m_Tints[vertex] = Mix(SHELL_TROUGH, SHELL_CREST, 0.5f * (wave + 1.f)).ToLinear().Pack();
    }

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
        for (u32 corner = 0; corner < 3; ++corner)
        {
            Onyx::DynamicVertex<D3> &vertex = mesh->Vertices[3 * triangle + corner];
            vertex.Position = m_Positions[corners[corner]];
            vertex.Normal = normal;
            vertex.Tangent = tangent;
            vertex.Color = isPlain ? m_Tints[corners[corner]] : m_PanelColors[triangle];
        }
    }
}

void Shell::Draw(Onyx::RenderContext<D3> &context) const
{
    context.Push();
    context.FillColor(UNTINTED);
    context.DynamicMesh(m_Mesh);
    context.Pop();
}

f32 Shell::GetRadius(const f32v3 &direction, const f32 time) const
{
    return m_Params.Radius + m_Params.WaveAmplitude * EvaluateWave(direction, time, m_Params);
}

u32 Shell::GetTriangleAt(const f32v3 &direction) const
{
    TKIT_ASSERT(!m_Triangles.IsEmpty(), "[LIM][SHELL] The shell must be built before querying its triangles");
    u32 nearest = 0;
    f32 nearestAlignment = TKit::Math::Dot(direction, m_Centers[0]);
    for (u32 triangle = 0; triangle < m_Triangles.GetSize(); ++triangle)
    {
        const f32 alignment = TKit::Math::Dot(direction, m_Centers[triangle]);
        if (alignment > nearestAlignment)
        {
            nearest = triangle;
            nearestAlignment = alignment;
        }
        if (alignment <= 0.f)
            continue;

        const u32v3 &corners = m_Triangles[triangle];
        const f32v3 &a = m_RestDirections[corners[0]];
        const f32v3 &b = m_RestDirections[corners[1]];
        const f32v3 &c = m_RestDirections[corners[2]];
        if (TKit::Math::Dot(direction, TKit::Math::Cross(a, b)) <= 0.f &&
            TKit::Math::Dot(direction, TKit::Math::Cross(b, c)) <= 0.f &&
            TKit::Math::Dot(direction, TKit::Math::Cross(c, a)) <= 0.f)
            return triangle;
    }
    return nearest;
}

f32v3 Shell::GetTriangleCenter(const u32 triangle) const
{
    return m_Centers[triangle];
}
} // namespace Lim

#include "shapes/mesh_builder.hpp"

#include "tkit/container/fixed_array.hpp"

namespace Lim
{
using Onyx::D3;

MeshBuilder::MeshBuilder(const TKit::Span<Onyx::DynamicVertex<D3>> vertices) : m_Vertices(vertices)
{
}

bool MeshBuilder::AppendHull(const ConvexHull &hull, const f32m4 &transform, const u32 packedColor)
{
    u32 triangleCount = 0;
    hull.ForEachTriangle(
        [&triangleCount](const f32v3 &, const f32v3 &, const f32v3 &, const f32v3 &) { ++triangleCount; });
    if (m_VertexCount + 3 * triangleCount > GetCapacity())
        return false;

    hull.ForEachTriangle([&](const f32v3 &first, const f32v3 &second, const f32v3 &third, const f32v3 &normal) {
        const TKit::FixedArray<f32v3, 3> corners{f32v3(transform * f32v4{first, 1.f}),
                                                 f32v3(transform * f32v4{second, 1.f}),
                                                 f32v3(transform * f32v4{third, 1.f})};
        const f32v3 worldNormal = TKit::Math::Normalize(f32v3(transform * f32v4{normal, 0.f}));
        const f32v4 tangent{TKit::Math::Normalize(corners[1] - corners[0]), 1.f};
        for (const f32v3 &corner : corners)
            m_Vertices[m_VertexCount++] = Onyx::DynamicVertex<D3>{.Position = corner,
                                                                  .TexCoord = f32v2{0.f},
                                                                  .Normal = worldNormal,
                                                                  .Tangent = tangent,
                                                                  .Color = packedColor};
    });
    return true;
}

u32 MeshBuilder::GetVertexCount() const
{
    return m_VertexCount;
}

u32 MeshBuilder::GetCapacity() const
{
    return u32(m_Vertices.GetSize());
}
} // namespace Lim

#include "shapes/shape_renderer.hpp"

#include "onyx/resources.hpp"
#include "tkit/container/fixed_array.hpp"

namespace Lim
{
using Onyx::D3;

namespace
{
/// Flat-shaded hull triangles in the hull's own frame; the mesh keeps the default of no back-face culling.
[[nodiscard]] Onyx::StaticMeshData<D3> CreateHullMesh(const ConvexHull &hull)
{
    Onyx::StaticMeshData<D3> mesh{};
    hull.ForEachTriangle([&mesh](const f32v3 &first, const f32v3 &second, const f32v3 &third, const f32v3 &normal) {
        const TKit::FixedArray<f32v3, 3> corners{first, second, third};
        const f32v4 tangent{TKit::Math::Normalize(second - first), 1.f};
        for (const f32v3 &corner : corners)
        {
            mesh.Indices.Append(Onyx::Index(mesh.Vertices.GetSize()));
            mesh.Vertices.Append(Onyx::StaticVertex<D3>{
                .Position = corner, .TexCoord = f32v2{0.f}, .Normal = normal, .Tangent = tangent});
        }
    });
    return mesh;
}
} // namespace

ShapeRenderer::~ShapeRenderer()
{
    if (!m_Pool)
        return;
    Onyx::ResourcePool_Release<D3>(*m_Pool);
    Onyx::Resources_Sync(Onyx::SyncFlag_StaticMeshes);
}

/// The released pool lives until the sync below, which waits for the frames in flight before destroying it.
void ShapeRenderer::Load(const TKit::Span<const Shape *const> shapes)
{
    if (m_Pool)
        Onyx::ResourcePool_Release<D3>(*m_Pool);
    const Onyx::ResourcePool pool = Onyx::ResourcePool_Create<D3>(Onyx::Resource_StaticMesh);
    m_Pool = pool;

    m_Shapes.Clear();
    m_Meshes.Clear();
    for (const Shape *shape : shapes)
    {
        m_Shapes.Append(LoadedShape{.Source = shape, .FirstMesh = u32(m_Meshes.GetSize())});
        for (u32 piece = 0; piece < shape->GetPieceCount(); ++piece)
            m_Meshes.Append(Onyx::ResourcePool_RegisterMesh<D3>(pool, CreateHullMesh(shape->GetPieceHull(piece))));
    }
    Onyx::Resources_Sync(Onyx::SyncFlag_StaticMeshes);
}

void ShapeRenderer::Draw(Onyx::RenderContext<D3> &context, const TKit::Span<const ShapeInstance> instances) const
{
    context.Push();
    for (const ShapeInstance &instance : instances)
    {
        const LoadedShape &loaded = m_Shapes[instance.Slot];
        context.FillColor(instance.Tint);
        for (u32 piece = 0; piece < loaded.Source->GetPieceCount(); ++piece)
            context.StaticMesh(m_Meshes[loaded.FirstMesh + piece],
                               instance.Transform * loaded.Source->GetPiecePose(piece));
    }
    context.Pop();
}
} // namespace Lim

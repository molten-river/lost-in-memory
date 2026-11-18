#include "shapes/shape_renderer.hpp"

#include "onyx/resources.hpp"
#include "tkit/container/fixed_array.hpp"

namespace Lim
{
using Onyx::D3;

namespace
{
/// Resources_CreateDefault keeps its 3D static meshes in one pool of its own.
constexpr u32 DEFAULT_STATIC_POOLS = 1;
/// Load creates the new pools before the sync that destroys the released ones.
constexpr u32 LIVE_POOLS_PER_RELOAD = 2;
static_assert(LIVE_POOLS_PER_RELOAD * ShapeRenderer::POOL_COUNT + DEFAULT_STATIC_POOLS <= ONYX_POSSIBLE_RESOURCE_POOLS);

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
    if (m_Pools.IsEmpty())
        return;
    for (const Onyx::ResourcePool pool : m_Pools)
        Onyx::ResourcePool_Release<D3>(pool);
    Onyx::Resources_Sync(Onyx::SyncFlag_StaticMeshes);
}

/// The released pools live until the sync below, which waits for the frames in flight before destroying them.
void ShapeRenderer::Load(const TKit::Span<const Shape *const> shapes)
{
    for (const Onyx::ResourcePool pool : m_Pools)
        Onyx::ResourcePool_Release<D3>(pool);
    m_Pools.Clear();
    m_Shapes.Clear();
    m_Meshes.Clear();
    for (const Shape *shape : shapes)
    {
        m_Shapes.Append(LoadedShape{.Source = shape, .FirstMesh = u32(m_Meshes.GetSize())});
        for (u32 piece = 0; piece < shape->GetPieceCount(); ++piece)
        {
            if (m_Meshes.GetSize() % MESHES_PER_POOL == 0)
                m_Pools.Append(Onyx::ResourcePool_Create<D3>(Onyx::Resource_StaticMesh));
            m_Meshes.Append(
                Onyx::ResourcePool_RegisterMesh<D3>(m_Pools.GetBack(), CreateHullMesh(shape->GetPieceHull(piece))));
        }
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

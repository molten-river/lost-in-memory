#include "shapes/shape_renderer.hpp"
#include "shapes/mesh_builder.hpp"

#include "onyx/resources.hpp"
#include "tkit/utils/limits.hpp"
#include "tkit/utils/logging.hpp"

namespace Lim
{
using Onyx::D3;

namespace
{
/// The smallest maxDrawIndexedIndexValue Vulkan guarantees; onyx draws a dynamic mesh with indices from 0.
constexpr u32 GUARANTEED_DRAW_INDEX_LIMIT = (1U << 24) - 1;

static_assert(ShapeRenderer::VERTEX_CAPACITY <= TKit::Limits<Onyx::Index>::Max(),
              "Every vertex must be reachable through onyx's index type");
static_assert(ShapeRenderer::VERTEX_CAPACITY <= u32(TKit::Limits<i32>::Max()),
              "Onyx stores a dynamic mesh's vertex offset in a signed 32-bit draw field");
static_assert(ShapeRenderer::VERTEX_CAPACITY - 1 <= GUARANTEED_DRAW_INDEX_LIMIT,
              "The highest index must stay within what every Vulkan device draws");
} // namespace

ShapeRenderer::ShapeRenderer() : m_Mesh(Onyx::DynamicMesh_Register<D3>())
{
    Onyx::DynamicMeshData<D3> &mesh = Onyx::DynamicMesh_GetData<D3>(m_Mesh);
    mesh.Flags &= ~Onyx::MeshFlag_BackCulled;
    mesh.Vertices.Reserve(VERTEX_CAPACITY);
    mesh.Indices.Reserve(VERTEX_CAPACITY);
    Onyx::Resources_Sync(Onyx::SyncFlag_DynamicMeshes);
}

ShapeRenderer::~ShapeRenderer()
{
    Onyx::DynamicMesh_Destroy<D3>(m_Mesh);
}

bool ShapeRenderer::Rewrite(const TKit::Span<const ShapeInstance> instances)
{
    Onyx::DynamicMeshData<D3> &mesh = Onyx::DynamicMesh_GetData<D3>(m_Mesh);
    mesh.Vertices.Resize(VERTEX_CAPACITY);

    MeshBuilder builder{mesh.Vertices};
    bool fits = true;
    for (const ShapeInstance &instance : instances)
        fits = instance.Shape->Emit(builder, instance.Transform, instance.PackedColor) && fits;

    m_VertexCount = builder.GetVertexCount();
    mesh.Vertices.Resize(m_VertexCount);
    mesh.Indices.Resize(m_VertexCount);
    for (u32 vertex = 0; vertex < m_VertexCount; ++vertex)
        mesh.Indices[vertex] = vertex;

    if (!fits && !m_HasOverflowed)
    {
        m_HasOverflowed = true;
        TKIT_LOG_ERROR("[LIM][SHAPES] The shape mesh is full at {} vertices, so some shapes are not drawn",
                       VERTEX_CAPACITY);
    }
    return fits;
}

void ShapeRenderer::Draw(Onyx::RenderContext<D3> &context) const
{
    if (m_VertexCount > 0)
        context.DynamicMesh(m_Mesh);
}

u32 ShapeRenderer::GetVertexCount() const
{
    return m_VertexCount;
}
} // namespace Lim

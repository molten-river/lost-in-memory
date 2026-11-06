#pragma once

#include "alias/alias.hpp"
#include "shapes/convex_hull.hpp"
#include "shapes/shape.hpp"
#include "shapes/shape_instance.hpp"

#include "onyx/context.hpp"
#include "onyx/handle.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
/// Registers every hull piece of the loaded shapes once as a static mesh, then draws them at their current poses.
class ShapeRenderer
{
  public:
    static constexpr u32 MAX_SHAPES = 63;
    static constexpr u32 MAX_MESHES = MAX_SHAPES * Shape::MAX_HULLS;
    /// Pool vertices when every mesh is a hull of MAX_TRIANGLES flat-shaded triangles, with one index per vertex.
    static constexpr u32 MAX_VERTICES = MAX_MESHES * 3 * ConvexHull::MAX_TRIANGLES;

    ShapeRenderer() = default;
    ~ShapeRenderer();

    ShapeRenderer(const ShapeRenderer &) = delete;
    ShapeRenderer &operator=(const ShapeRenderer &) = delete;

    /// Replaces the pool with one mesh per piece of each shape, whose index becomes its slot; never call it per frame.
    void Load(TKit::Span<const Shape *const> shapes);
    void Draw(Onyx::RenderContext<Onyx::D3> &context, TKit::Span<const ShapeInstance> instances) const;

  private:
    struct LoadedShape
    {
        const Shape *Source;
        u32 FirstMesh;
    };

    TKit::Optional<Onyx::ResourcePool> m_Pool{};
    TKit::StaticArray<LoadedShape, MAX_SHAPES> m_Shapes{};
    TKit::StaticArray<Onyx::Resource, MAX_MESHES> m_Meshes{};
};
} // namespace Lim

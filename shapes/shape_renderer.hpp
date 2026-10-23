#pragma once

#include "alias/alias.hpp"
#include "shapes/convex_hull.hpp"
#include "shapes/shape.hpp"
#include "shapes/shape_instance.hpp"

#include "onyx/context.hpp"
#include "onyx/handle.hpp"
#include "tkit/container/span.hpp"

namespace Lim
{
/// Owns one dynamic mesh sized once for the worst case and rewrites it from posed shapes without allocating.
class ShapeRenderer
{
  public:
    static constexpr u32 MAX_SHAPES = 63;
    static constexpr u32 VERTEX_CAPACITY = MAX_SHAPES * Shape::MAX_HULLS * 3 * ConvexHull::MAX_TRIANGLES;

    ShapeRenderer();
    ~ShapeRenderer();

    ShapeRenderer(const ShapeRenderer &) = delete;
    ShapeRenderer &operator=(const ShapeRenderer &) = delete;

    /// Returns false when some shapes did not fit; those are left out and the first overflow is logged.
    bool Rewrite(TKit::Span<const ShapeInstance> instances);
    void Draw(Onyx::RenderContext<Onyx::D3> &context) const;
    [[nodiscard]] u32 GetVertexCount() const;

  private:
    Onyx::Resource m_Mesh;
    u32 m_VertexCount = 0;
    bool m_HasOverflowed = false;
};
} // namespace Lim

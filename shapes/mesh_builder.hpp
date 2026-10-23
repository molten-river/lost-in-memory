#pragma once

#include "alias/alias.hpp"
#include "shapes/convex_hull.hpp"

#include "onyx/vertex.hpp"
#include "tkit/container/span.hpp"
#include "tkit/math/tensor.hpp"

namespace Lim
{
/// Appends flat-shaded, transformed hull triangles to a span of dynamic vertices, never past its end.
class MeshBuilder
{
  public:
    explicit MeshBuilder(TKit::Span<Onyx::DynamicVertex<Onyx::D3>> vertices);

    /// Returns false, writing nothing, when the hull's triangles do not fit.
    [[nodiscard]] bool AppendHull(const ConvexHull &hull, const f32m4 &transform, u32 packedColor);
    [[nodiscard]] u32 GetVertexCount() const;
    [[nodiscard]] u32 GetCapacity() const;

  private:
    TKit::Span<Onyx::DynamicVertex<Onyx::D3>> m_Vertices;
    u32 m_VertexCount = 0;
};
} // namespace Lim

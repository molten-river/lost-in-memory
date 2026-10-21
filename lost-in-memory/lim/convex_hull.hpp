#pragma once

#include "lim/alias.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/tensor.hpp"

namespace Lim
{
/// The convex hull of a few points, queried as a signed distance through its merged face planes.
class ConvexHull
{
  public:
    static constexpr u32 MAX_POINTS = 16;
    static constexpr u32 MAX_TRIANGLES = 2 * MAX_POINTS - 4;

    void Build(TKit::Span<const f32v3> points);

    [[nodiscard]] f32 Distance(const f32v3 &point) const;
    [[nodiscard]] f32v3 Gradient(const f32v3 &point) const;
    [[nodiscard]] f32 GetVolume() const;
    [[nodiscard]] TKit::Span<const f32v3> GetVertices() const;

    /// Visits every outward counter-clockwise triangle as its three corners and its face normal.
    template <typename Emit> void ForEachTriangle(Emit &&emit) const
    {
        for (const Triangle &triangle : m_Triangles)
            emit(m_Vertices[triangle.Corners[0]], m_Vertices[triangle.Corners[1]], m_Vertices[triangle.Corners[2]],
                 m_Planes[triangle.Plane].Normal);
    }

  private:
    struct Plane
    {
        f32v3 Normal;
        f32 Offset;
    };

    struct Triangle
    {
        TKit::FixedArray<u8, 3> Corners;
        u8 Plane;
    };

    TKit::StaticArray<f32v3, MAX_POINTS> m_Vertices{};
    TKit::StaticArray<Triangle, MAX_TRIANGLES> m_Triangles{};
    TKit::StaticArray<Plane, MAX_TRIANGLES> m_Planes{};
    f32 m_Volume = 0.f;
};
} // namespace Lim

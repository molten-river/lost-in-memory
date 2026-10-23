#pragma once

#include "alias/alias.hpp"
#include "shapes/mesh_builder.hpp"

#include "tkit/container/span.hpp"
#include "tkit/math/tensor.hpp"

namespace Lim
{
/// A baked form posed by absolute time; it draws and answers distance queries from the same posed hulls.
class Shape
{
  public:
    static constexpr u32 MAX_HULLS = 15;
    static constexpr u32 MAX_SAMPLE_POINTS = 128;

    virtual ~Shape() = default;

    /// Poses the shape at an absolute time, so the same seconds always give the same pose.
    virtual void Evolve(f32 seconds) = 0;
    /// Appends the posed hulls; returns false, writing nothing, when the builder has too little room.
    [[nodiscard]] virtual bool Emit(MeshBuilder &builder, const f32m4 &transform, u32 packedColor) const = 0;
    [[nodiscard]] virtual f32 Distance(const f32v3 &local) const = 0;
    [[nodiscard]] virtual f32v3 Gradient(const f32v3 &local) const = 0;
    /// Bounds every pose the motions can reach, measured from the shape's origin.
    [[nodiscard]] virtual f32 GetBoundingRadius() const = 0;
    /// Surface points of the current pose in shape space, at most MAX_SAMPLE_POINTS, fixed in count and order.
    [[nodiscard]] virtual TKit::Span<const f32v3> GetSamplePoints() const = 0;
    /// Solid volume, fixed at bake time and unaffected by the pose.
    [[nodiscard]] virtual f32 GetVolume() const = 0;
};
} // namespace Lim

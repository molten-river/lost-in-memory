#pragma once

#include "lim/alias.hpp"
#include "onyx/context.hpp"
#include "onyx/handle.hpp"
#include "onyx/math.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/quaternion.hpp"
#include "tkit/math/tensor.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
/// Extent: sphere x radius; box half sizes; capsule x radius, y half-length on Y; torus x major, y minor radius in XZ.
struct BlobPrimitive
{
    enum class Shape : u8
    {
        Sphere,
        Box,
        Capsule,
        Torus
    };

    Shape Kind;
    f32v3 Center;
    f32v3 Extent;
    f32q Rotation = Onyx::RotType<Onyx::D3>::Identity;
};

/// A rigid smooth union of primitives, meshed once into a static mesh of a caller-owned pool.
class Blob
{
  public:
    static constexpr u32 MAX_PRIMITIVES = 6;

    Blob(Onyx::ResourcePool pool, TKit::Span<const BlobPrimitive> primitives, f32 blendRadius);

    [[nodiscard]] f32 Distance(const f32v3 &localPoint) const;
    [[nodiscard]] f32 GetBoundingRadius() const;
    void Draw(Onyx::RenderContext<Onyx::D3> &context, const f32m4 &transform) const;

  private:
    TKit::StaticArray<BlobPrimitive, MAX_PRIMITIVES> m_Primitives{};
    f32 m_BlendRadius;
    f32 m_BoundingRadius = 0.f;
    TKit::Optional<Onyx::Resource> m_Mesh{};
};
} // namespace Lim

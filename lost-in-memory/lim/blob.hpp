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
/// Extent holds half sizes, or x radius and y half height of round shapes (torus: x major, y minor); Wall is bar width.
struct BlobPrimitive
{
    enum class Shape : u8
    {
        Sphere,
        Box,
        Capsule,
        Torus,
        Cylinder,
        Prism,
        Wedge,
        Frame,
        Ring,
        Lattice
    };

    enum class Op : u8
    {
        Union,
        Subtract,
        Intersect
    };

    Shape Kind;
    Op Operation = Op::Union;
    f32v3 Center;
    f32v3 Extent;
    f32 Wall = 0.f;
    f32q Rotation = Onyx::RotType<Onyx::D3>::Identity;
};

/// A rigid signed distance field folded from ordered primitives, meshed once into a caller-owned static mesh pool.
class Blob
{
  public:
    static constexpr u32 MAX_PRIMITIVES = 10;
    static constexpr u32 MAX_SAMPLE_POINTS = 128;
    static constexpr u32 MIN_MESH_RESOLUTION = 12;
    static constexpr u32 MAX_MESH_RESOLUTION = 48;

    Blob(Onyx::ResourcePool pool, TKit::Span<const BlobPrimitive> primitives, f32 blendRadius, u32 meshResolution);

    /// The primitives folded in order by their operations; the first one is the base whatever its operation.
    [[nodiscard]] static f32 Evaluate(TKit::Span<const BlobPrimitive> primitives, f32 blendRadius,
                                      const f32v3 &localPoint);

    [[nodiscard]] f32 Distance(const f32v3 &localPoint) const;
    [[nodiscard]] f32v3 Gradient(const f32v3 &localPoint) const;
    [[nodiscard]] f32 GetBoundingRadius() const;
    [[nodiscard]] TKit::Span<const f32v3> GetSamplePoints() const;
    [[nodiscard]] f32 GetVolume() const;
    void Draw(Onyx::RenderContext<Onyx::D3> &context, const f32m4 &transform) const;

  private:
    TKit::StaticArray<BlobPrimitive, MAX_PRIMITIVES> m_Primitives{};
    TKit::StaticArray<f32v3, MAX_SAMPLE_POINTS> m_SamplePoints{};
    f32 m_BlendRadius;
    f32 m_BoundingRadius = 0.f;
    f32 m_Volume = 0.f;
    TKit::Optional<Onyx::Resource> m_Mesh{};
};
} // namespace Lim

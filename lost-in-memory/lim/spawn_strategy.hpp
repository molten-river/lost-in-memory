#pragma once

#include "alias/alias.hpp"

#include "tkit/container/span.hpp"
#include "tkit/math/math.hpp"
#include "tkit/math/tensor.hpp"

#include <cstddef>

namespace Lim
{
/// The radii a body may spawn between, measured from the shell's centre.
struct SpawnBand
{
    f32 Nearest;
    f32 Farthest;

    /// The radius that leaves the given share of the band's volume inside it, so uniform shares fill it evenly.
    [[nodiscard]] f32 GetVolumeRadius(const f32 share) const
    {
        constexpr f32 ONE_THIRD = 1.f / 3.f;
        const f32 inner = Nearest * Nearest * Nearest;
        const f32 outer = Farthest * Farthest * Farthest;
        return TKit::Math::Power(TKit::Math::LinearLerp(inner, outer, share), ONE_THIRD);
    }
};

/// Places a record's body inside the band; slot is its order among the count non-empty records.
class SpawnStrategy
{
  public:
    virtual ~SpawnStrategy() = default;

    [[nodiscard]] virtual f32v3 Place(TKit::Span<const std::byte> record, u32 slot, u32 count,
                                      const SpawnBand &band) const = 0;
};
} // namespace Lim

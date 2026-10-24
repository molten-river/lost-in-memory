#pragma once

#include "alias/alias.hpp"
#include "lim/spawn_strategy.hpp"

namespace Lim
{
/// A hash of the whole record and its slot, spread evenly over the sphere and the band's volume.
class ScatterSpawn final : public SpawnStrategy
{
  public:
    [[nodiscard]] f32v3 Place(TKit::Span<const std::byte> record, u32 slot, u32 count,
                              const SpawnBand &band) const override;
};
} // namespace Lim

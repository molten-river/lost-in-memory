#pragma once

#include "alias/alias.hpp"
#include "lim/spawn_strategy.hpp"

namespace Lim
{
/// The placement bytes read directly: azimuth, height and a linear share of the band.
class RecordBytesSpawn final : public SpawnStrategy
{
  public:
    [[nodiscard]] f32v3 Place(TKit::Span<const std::byte> record, u32 slot, u32 count,
                              const SpawnBand &band) const override;
};
} // namespace Lim

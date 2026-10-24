#pragma once

#include "alias/alias.hpp"
#include "lim/spawn_strategy.hpp"

namespace Lim
{
/// A golden-angle Fibonacci sphere over the slots, filled evenly in depth and nudged by the placement bytes.
class LatticeSpawn final : public SpawnStrategy
{
  public:
    [[nodiscard]] f32v3 Place(TKit::Span<const std::byte> record, u32 slot, u32 count,
                              const SpawnBand &band) const override;
};
} // namespace Lim

#pragma once

#include "alias/alias.hpp"
#include "lim/spawn_kind.hpp"
#include "lim/spawn_strategy.hpp"

namespace Lim
{
/// A registry of one stateless strategy per spawn kind.
class SpawnFactory
{
  public:
    [[nodiscard]] static const SpawnStrategy &Get(SpawnKind kind);
};
} // namespace Lim

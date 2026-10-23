#pragma once

#include "alias/alias.hpp"
#include "shapes/motion_kind.hpp"
#include "shapes/motion_strategy.hpp"

namespace Lim
{
/// A registry of one stateless strategy per motion kind.
class MotionFactory
{
  public:
    [[nodiscard]] static const MotionStrategy &Get(MotionKind kind);
};
} // namespace Lim

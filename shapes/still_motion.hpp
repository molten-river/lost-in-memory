#pragma once

#include "alias/alias.hpp"
#include "shapes/motion_strategy.hpp"

namespace Lim
{
class StillMotion final : public MotionStrategy
{
  public:
    [[nodiscard]] JointMotion Bake(const MotionSeed &seed, const WalkMassParams &params) const override;
    [[nodiscard]] JointPose Evaluate(const JointMotion &motion, f32 seconds) const override;
    [[nodiscard]] bool SplitsMember() const override;
};
} // namespace Lim

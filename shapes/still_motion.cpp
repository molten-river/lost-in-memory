#include "shapes/still_motion.hpp"

namespace Lim
{
JointMotion StillMotion::Bake(const MotionSeed &seed, const WalkMassParams &) const
{
    return JointMotion{.Kind = MotionKind::Still, .Timing = seed.Timing, .Axis = f32v3{0.f}, .Reach = 0.f, .Step = 0.f};
}

JointPose StillMotion::Evaluate(const JointMotion &, const f32) const
{
    return JointPose{.Transform = f32m4::Identity()};
}

bool StillMotion::SplitsMember() const
{
    return false;
}
} // namespace Lim

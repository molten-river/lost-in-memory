#include "shapes/ratchet_motion.hpp"
#include "shapes/walk_mass_layout.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/math/quaternion.hpp"

namespace Lim
{
namespace
{
constexpr f32v3 RIGHT{1.f, 0.f, 0.f};
constexpr f32v3 UP{0.f, 1.f, 0.f};
constexpr f32v3 HEADING{0.f, 0.f, 1.f};
constexpr TKit::FixedArray<f32v3, WALK_MASS_AXIS_CODES> AXES{UP, RIGHT, HEADING, HEADING};
} // namespace

JointMotion RatchetMotion::Bake(const MotionSeed &seed, const WalkMassParams &params) const
{
    return JointMotion{.Kind = MotionKind::Ratchet,
                       .Timing = seed.Timing,
                       .Axis = AXES[seed.AxisCode],
                       .Reach = 0.f,
                       .Step = TKit::Math::Radians(f32(seed.StepNibble) * params.RatchetStepDegrees)};
}

/// Each cycle adds one more click to the angle, so it climbs with time and never swings back.
JointPose RatchetMotion::Evaluate(const JointMotion &motion, const f32 seconds) const
{
    const CyclePosition position = LocateInCycle(motion.Timing, seconds);
    const f32 clicks = position.Cycle + ComputeOneWay(position.Progress, motion.Timing.Hold);
    return JointPose{.Transform = TKit::Math::ToMat4(f32q::FromAngleAxis(clicks * motion.Step, motion.Axis))};
}

bool RatchetMotion::SplitsMember() const
{
    return false;
}
} // namespace Lim

#include "shapes/slide_motion.hpp"
#include "shapes/walk_mass_layout.hpp"

#include "tkit/container/fixed_array.hpp"

namespace Lim
{
namespace
{
constexpr f32v3 RIGHT{1.f, 0.f, 0.f};
constexpr f32v3 UP{0.f, 1.f, 0.f};
constexpr TKit::FixedArray<f32v3, WALK_MASS_AXIS_CODES> AXES{RIGHT, UP, RIGHT, UP};
} // namespace

JointMotion SlideMotion::Bake(const MotionSeed &seed, const WalkMassParams &params) const
{
    const f32 amplitude = f32(seed.Amplitude) / f32(WALK_MASS_NIBBLE_MAX);
    return JointMotion{.Kind = MotionKind::Slide,
                       .Timing = seed.Timing,
                       .Axis = AXES[seed.AxisCode],
                       .Reach = amplitude * params.SlideReachMax * seed.Size,
                       .Step = 0.f};
}

JointPose SlideMotion::Evaluate(const JointMotion &motion, const f32 seconds) const
{
    const CyclePosition position = LocateInCycle(motion.Timing, seconds);
    const f32 travel = ComputeBackAndForth(position.Progress, motion.Timing.Hold) * motion.Reach;
    f32m4 transform = f32m4::Identity();
    transform[3] = f32v4{motion.Axis * travel, 1.f};
    return JointPose{.Transform = transform};
}

bool SlideMotion::SplitsMember() const
{
    return false;
}
} // namespace Lim

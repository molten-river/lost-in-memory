#include "shapes/piston_motion.hpp"
#include "shapes/walk_mass_layout.hpp"

namespace Lim
{
namespace
{
constexpr f32v3 HEADING{0.f, 0.f, 1.f};
} // namespace

JointMotion PistonMotion::Bake(const MotionSeed &seed, const WalkMassParams &params) const
{
    const f32 amplitude = f32(seed.Amplitude) / f32(WALK_MASS_NIBBLE_MAX);
    return JointMotion{.Kind = MotionKind::Piston,
                       .Timing = seed.Timing,
                       .Axis = HEADING,
                       .Reach = amplitude * params.PistonReachMax * seed.Length,
                       .Step = 0.f};
}

JointPose PistonMotion::Evaluate(const JointMotion &motion, const f32 seconds) const
{
    const CyclePosition position = LocateInCycle(motion.Timing, seconds);
    const f32 stroke = ComputeBackAndForth(position.Progress, motion.Timing.Hold) * motion.Reach;
    f32m4 transform = f32m4::Identity();
    transform[3] = f32v4{motion.Axis * stroke, 1.f};
    return JointPose{.Transform = transform};
}

bool PistonMotion::SplitsMember() const
{
    return true;
}
} // namespace Lim

#pragma once

#include "alias/alias.hpp"
#include "shapes/motion_kind.hpp"
#include "shapes/walk_mass_params.hpp"

#include "tkit/math/math.hpp"
#include "tkit/math/tensor.hpp"

namespace Lim
{
inline constexpr f32 MOTION_MIN_MOVE_SHARE = 1e-3f;

struct MotionTiming
{
    f32 Period;
    f32 Phase;
    f32 Hold;
};

/// A joint's motion bytes, decoded but not yet scaled into reach, step and axis by the strategy.
struct MotionSeed
{
    MotionTiming Timing;
    u8 Amplitude;
    u8 StepNibble;
    u8 AxisCode;
    f32 Length;
    f32 Size;
};

struct JointMotion
{
    MotionKind Kind;
    MotionTiming Timing;
    f32v3 Axis;
    f32 Reach;
    f32 Step;
};

struct JointPose
{
    f32m4 Transform;
};

struct CyclePosition
{
    f32 Cycle;
    f32 Progress;
};

/// One way a joint moves: it scales its seed once at bake time, then poses the joint frame from absolute time.
class MotionStrategy
{
  public:
    virtual ~MotionStrategy() = default;

    [[nodiscard]] virtual JointMotion Bake(const MotionSeed &seed, const WalkMassParams &params) const = 0;
    [[nodiscard]] virtual JointPose Evaluate(const JointMotion &motion, f32 seconds) const = 0;
    /// Whether the member stays put as a sleeve while a rod and everything outboard follow the motion.
    [[nodiscard]] virtual bool SplitsMember() const = 0;
};

[[nodiscard]] inline CyclePosition LocateInCycle(const MotionTiming &timing, const f32 seconds)
{
    const f32 turns = seconds / timing.Period + timing.Phase;
    f32 cycle = f32(i64(turns));
    if (cycle > turns)
        cycle -= 1.f;
    return CyclePosition{.Cycle = cycle, .Progress = turns - cycle};
}

/// Evaluated in double precision and capped at 1, so single-precision rounding never makes a ratchet step back.
[[nodiscard]] inline f32 Smootherstep(const f32 t)
{
    const f64 x = f64(t);
    return f32(TKit::Math::Min(x * x * x * (x * (6.0 * x - 15.0) + 10.0), 1.0));
}

/// Move out, hold, move back, hold: the two moves share the part of the cycle the hold leaves free.
[[nodiscard]] inline f32 ComputeBackAndForth(const f32 progress, const f32 hold)
{
    const f32 move = TKit::Math::Max(0.5f * (1.f - hold), MOTION_MIN_MOVE_SHARE);
    const f32 rest = 0.5f * hold;
    if (progress < move)
        return Smootherstep(progress / move);
    if (progress < move + rest)
        return 1.f;
    if (progress < 2.f * move + rest)
        return 1.f - Smootherstep((progress - move - rest) / move);
    return 0.f;
}

/// Move, then hold: progress reaches 1 and stays there until the next cycle starts again from 0.
[[nodiscard]] inline f32 ComputeOneWay(const f32 progress, const f32 hold)
{
    const f32 move = TKit::Math::Max(1.f - hold, MOTION_MIN_MOVE_SHARE);
    return progress < move ? Smootherstep(progress / move) : 1.f;
}
} // namespace Lim

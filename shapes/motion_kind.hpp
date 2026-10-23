#pragma once

#include "alias/alias.hpp"

namespace Lim
{
/// The motion of a joint; the values are the motion codes of the joint's drive byte.
enum class MotionKind : u8
{
    Still = 0,
    Piston = 1,
    Ratchet = 2,
    Slide = 3
};

inline constexpr u32 MOTION_KIND_COUNT = u32(MotionKind::Slide) + 1;
} // namespace Lim

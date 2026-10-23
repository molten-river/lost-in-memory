#pragma once

#include "alias/alias.hpp"

namespace Lim
{
inline constexpr usize WALK_MASS_RECORD_BYTES = 64;

inline constexpr usize WALK_MASS_SIZE_BYTE = 6;
inline constexpr usize WALK_MASS_SECTIONS_BYTE = 15;
inline constexpr usize WALK_MASS_SECTION_BYTES = 3;
inline constexpr usize WALK_MASS_SECTION_COUNT = 8;
inline constexpr usize WALK_MASS_LATTICE_BYTE = 39;
inline constexpr usize WALK_MASS_JOINTS_BYTE = 40;
inline constexpr usize WALK_MASS_JOINT_BYTES = 3;
inline constexpr usize WALK_MASS_JOINT_COUNT = 8;

inline constexpr usize WALK_MASS_SECTION_LEAD_BYTE = 0;
inline constexpr usize WALK_MASS_SECTION_SHAPE_BYTE = 1;
inline constexpr usize WALK_MASS_SECTION_FINISH_BYTE = 2;

inline constexpr usize WALK_MASS_JOINT_DRIVE_BYTE = 0;
inline constexpr usize WALK_MASS_JOINT_CADENCE_BYTE = 1;
inline constexpr usize WALK_MASS_JOINT_DETAIL_BYTE = 2;

inline constexpr u32 WALK_MASS_BYTE_MAX = 255;
inline constexpr u32 WALK_MASS_NIBBLE_MAX = 15;

inline constexpr u8 WALK_MASS_LENGTH_MASK = 0x0F;
inline constexpr u32 WALK_MASS_LENGTH_STEPS = 14;
inline constexpr u8 WALK_MASS_TURN_SHIFT = 4;
inline constexpr u8 WALK_MASS_TURN_MASK = 0x07;
inline constexpr u8 WALK_MASS_OFFSET_BIT = 1U << 7;

inline constexpr u32 WALK_MASS_TURN_CODES = 8;
inline constexpr u32 WALK_MASS_TURN_LEFT = 4;
inline constexpr u32 WALK_MASS_TURN_RIGHT = 5;
inline constexpr u32 WALK_MASS_TURN_UP = 6;
inline constexpr u32 WALK_MASS_TURN_DOWN = 7;

inline constexpr u8 WALK_MASS_BREADTH_MASK = 0x0F;
inline constexpr u8 WALK_MASS_THICKNESS_SHIFT = 4;

inline constexpr u8 WALK_MASS_CHAMFER_MASK = 0x03;
inline constexpr u32 WALK_MASS_CHAMFER_LEVELS = 3;
inline constexpr u8 WALK_MASS_ROLL_SHIFT = 2;
inline constexpr u8 WALK_MASS_ROLL_MASK = 0x03;
inline constexpr u8 WALK_MASS_TAPER_SHIFT = 4;

inline constexpr u8 WALK_MASS_MOTION_MASK = 0x03;
inline constexpr u8 WALK_MASS_HOLD_SHIFT = 2;
inline constexpr u8 WALK_MASS_HOLD_MASK = 0x03;
inline constexpr u32 WALK_MASS_HOLD_LEVELS = 3;
inline constexpr u8 WALK_MASS_AMPLITUDE_SHIFT = 4;

inline constexpr u8 WALK_MASS_PERIOD_MASK = 0x0F;
inline constexpr u8 WALK_MASS_PHASE_SHIFT = 4;
inline constexpr u32 WALK_MASS_PHASE_STEPS = 16;

inline constexpr u8 WALK_MASS_STEP_MASK = 0x0F;
inline constexpr u8 WALK_MASS_AXIS_SHIFT = 4;
inline constexpr u8 WALK_MASS_AXIS_MASK = 0x03;
inline constexpr u32 WALK_MASS_AXIS_CODES = 4;

static_assert(WALK_MASS_SECTIONS_BYTE + WALK_MASS_SECTION_COUNT * WALK_MASS_SECTION_BYTES == WALK_MASS_LATTICE_BYTE);
static_assert(WALK_MASS_JOINTS_BYTE + WALK_MASS_JOINT_COUNT * WALK_MASS_JOINT_BYTES == WALK_MASS_RECORD_BYTES);
static_assert(WALK_MASS_TURN_DOWN < WALK_MASS_TURN_CODES);
} // namespace Lim

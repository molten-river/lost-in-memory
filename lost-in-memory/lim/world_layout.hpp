#pragma once

#include "lim/alias.hpp"

namespace Lim
{
inline constexpr usize GENOME_BYTES = 64;
inline constexpr usize RECORD_BYTES = 64;
inline constexpr usize RECORD_COUNT = 63;

inline constexpr usize GENOME_GOAL_BYTE = 0;

inline constexpr usize RECORD_TAG_BYTE = 0;
inline constexpr usize RECORD_SHAPE_BYTE = 1;
inline constexpr usize RECORD_PROFILE_BYTE = 2;
inline constexpr usize RECORD_PROFILE_BYTES = 4;
inline constexpr usize MAX_PROFILE_VERTICES = 8;
inline constexpr usize RECORD_SIZE_BYTE = 6;
inline constexpr usize RECORD_SPAWN_AZIMUTH_BYTE = 7;
inline constexpr usize RECORD_SPAWN_Z_BYTE = 8;
inline constexpr usize RECORD_SPAWN_DISTANCE_BYTE = 9;
inline constexpr usize RECORD_HEADING_AZIMUTH_BYTE = 10;
inline constexpr usize RECORD_HEADING_Z_BYTE = 11;
inline constexpr usize RECORD_SPEED_BYTE = 12;
inline constexpr usize RECORD_TUMBLE_AXIS_BYTE = 13;
inline constexpr usize RECORD_TUMBLE_SCALE_BYTE = 14;
inline constexpr usize RECORD_SECTIONS_BYTE = 15;
inline constexpr usize RECORD_SECTION_SLOT_BYTES = 3;
inline constexpr usize RECORD_SECTION_SLOT_COUNT = 8;
inline constexpr usize RECORD_REACH_BIAS_BYTE = 39;
inline constexpr u8 RECORD_CLOSED_CHAIN_BIT = 1U << 7;
inline constexpr usize RECORD_EVOLUTIONS_BYTE = 40;
inline constexpr usize RECORD_EVOLUTION_SLOT_BYTES = 3;
inline constexpr usize RECORD_EVOLUTION_SLOT_COUNT = 8;

static_assert(RECORD_PROFILE_BYTES * 2 == MAX_PROFILE_VERTICES);
} // namespace Lim

#pragma once

#include "alias/alias.hpp"

namespace Lim
{
inline constexpr usize GENOME_BYTES = 64;
inline constexpr usize RECORD_BYTES = 64;
inline constexpr usize RECORD_COUNT = 63;

inline constexpr usize GENOME_GOAL_BYTE = 0;

inline constexpr usize RECORD_TAG_BYTE = 0;
inline constexpr usize RECORD_SPAWN_AZIMUTH_BYTE = 7;
inline constexpr usize RECORD_SPAWN_Z_BYTE = 8;
inline constexpr usize RECORD_SPAWN_DISTANCE_BYTE = 9;
inline constexpr usize RECORD_HEADING_AZIMUTH_BYTE = 10;
inline constexpr usize RECORD_HEADING_Z_BYTE = 11;
inline constexpr usize RECORD_SPEED_BYTE = 12;
inline constexpr usize RECORD_TUMBLE_AXIS_BYTE = 13;
inline constexpr usize RECORD_TUMBLE_SCALE_BYTE = 14;
} // namespace Lim

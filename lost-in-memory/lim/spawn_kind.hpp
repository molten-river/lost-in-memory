#pragma once

#include "alias/alias.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
TKIT_REFLECT_DECLARE_ENUM(SpawnKind)
enum class SpawnKind : u8
{
    Lattice,
    RecordBytes,
    Scatter
};

inline constexpr u32 SPAWN_KIND_COUNT = u32(SpawnKind::Scatter) + 1;
} // namespace Lim

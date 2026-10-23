#pragma once

#include "alias/alias.hpp"

namespace Lim
{
enum class ShapeKind : u8
{
    WalkMass
};

inline constexpr u32 SHAPE_KIND_COUNT = u32(ShapeKind::WalkMass) + 1;
} // namespace Lim

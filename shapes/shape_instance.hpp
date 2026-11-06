#pragma once

#include "alias/alias.hpp"

#include "onyx/color.hpp"
#include "tkit/math/tensor.hpp"

namespace Lim
{
/// Slot is the shape's index in the span the renderer last loaded.
struct ShapeInstance
{
    u32 Slot;
    f32m4 Transform;
    Onyx::Color Tint;
};
} // namespace Lim

#pragma once

#include "alias/alias.hpp"
#include "shapes/shape.hpp"

#include "tkit/math/tensor.hpp"

namespace Lim
{
struct ShapeInstance
{
    const Lim::Shape *Shape;
    f32m4 Transform;
    u32 PackedColor;
};
} // namespace Lim

#pragma once

#include "alias/alias.hpp"
#include "shapes/walk_mass_params.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct ShapeKitParams
{
    TKIT_REFLECT_DECLARE(ShapeKitParams)
    WalkMassParams WalkMass;
};
} // namespace Lim

#pragma once

#include "lim/alias.hpp"

#include "tkit/math/math.hpp"
#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct PlayerParams
{
    TKIT_REFLECT_DECLARE(PlayerParams)
    f32 Radius = 0.5f;
    f32 Thrust = 40.f;
    f32 MaxSpeed = 12.f;
    f32 VelocityKeptPerSecond = 0.05f;
    f32 MouseRadiansPerPixel = 0.0025f;
    f32 RollSpeed = 2.f;
};

[[nodiscard]] inline PlayerParams Sanitize(PlayerParams params)
{
    params.Radius = TKit::Math::Max(params.Radius, 0.05f);
    params.Thrust = TKit::Math::Max(params.Thrust, 0.f);
    params.MaxSpeed = TKit::Math::Max(params.MaxSpeed, 0.f);
    params.VelocityKeptPerSecond = TKit::Math::Clamp(params.VelocityKeptPerSecond, 0.f, 1.f);
    params.MouseRadiansPerPixel = TKit::Math::Max(params.MouseRadiansPerPixel, 0.f);
    params.RollSpeed = TKit::Math::Max(params.RollSpeed, 0.f);
    return params;
}
} // namespace Lim

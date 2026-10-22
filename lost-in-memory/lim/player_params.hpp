#pragma once

#include "lim/alias.hpp"
#include "lim/param_schema.hpp"

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

template <> struct ParamSchema<PlayerParams>
{
    static constexpr TKit::FixedArray<ParamSpec<PlayerParams>, 6> FIELDS{{
        {.Member = &PlayerParams::Radius,
         .Hint = "Collision radius of the player",
         .Flags = ParamSpecFlag_Min,
         .Min = 0.05f},
        {.Member = &PlayerParams::Thrust, .Hint = "Acceleration while a move key is held", .Flags = ParamSpecFlag_Min},
        {.Member = &PlayerParams::MaxSpeed, .Hint = "Speed cap of the player", .Flags = ParamSpecFlag_Min},
        {.Member = &PlayerParams::VelocityKeptPerSecond,
         .Hint = "Share of velocity left after one second without thrust",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 1.f},
        {.Member = &PlayerParams::MouseRadiansPerPixel, .Hint = "Look sensitivity", .Flags = ParamSpecFlag_Min},
        {.Member = &PlayerParams::RollSpeed,
         .Hint = "Roll rate on Q and E, in radians per second",
         .Flags = ParamSpecFlag_Min},
    }};
};
} // namespace Lim

#pragma once

#include "alias/alias.hpp"
#include "tuning/param_schema.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
inline constexpr u32 MAX_SUBDIVISIONS = 5;

struct ShellParams
{
    TKIT_REFLECT_DECLARE(ShellParams)
    f32 Radius = 120.f;
    u32 Subdivisions = 4;
    f32 LethalDensity = 0.06f;
};

template <> struct ParamSchema<ShellParams>
{
    static constexpr TKit::FixedArray<ParamSpec<ShellParams>, 3> FIELDS{{
        {.Member = &ShellParams::Radius,
         .Hint = "Rest radius of the sphere",
         .Flags = BOTH_BOUNDS,
         .Min = 20.f,
         .Max = 400.f},
        {.Member = &ShellParams::Subdivisions,
         .Hint = "Icosphere subdivisions; each level quadruples the panels",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = f32(MAX_SUBDIVISIONS),
         .Drag = FINE_INTEGER},
        {.Member = &ShellParams::LethalDensity,
         .Hint = "Chance that a shell panel is lethal",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 1.f},
    }};
};

struct ShellMotionParams
{
    TKIT_REFLECT_DECLARE(ShellMotionParams)
    f32 WaveAmplitude = 4.8f;
    f32 WaveSpeed = 0.6f;
    f32 WaveFrequency = 3.f;
    f32 NoiseAmplitude = 10.f;
    f32 NoiseSpeed = 0.3f;
    f32 NoiseScale = 2.5f;
    f32 TangentialShare = 0.35f;
    f32 FacetTintStrength = 2.f;
};

template <> struct ParamSchema<ShellMotionParams>
{
    static constexpr TKit::FixedArray<ParamSpec<ShellMotionParams>, 8> FIELDS{{
        {.Member = &ShellMotionParams::WaveAmplitude,
         .Hint = "Radial height of the travelling waves, in world units",
         .Flags = ParamSpecFlag_Min},
        {.Member = &ShellMotionParams::WaveSpeed, .Hint = "Wave cycles per second"},
        {.Member = &ShellMotionParams::WaveFrequency, .Hint = "Wave crests across the sphere"},
        {.Member = &ShellMotionParams::NoiseAmplitude,
         .Hint = "Total vertex displacement by noise, in world units",
         .Flags = ParamSpecFlag_Min},
        {.Member = &ShellMotionParams::NoiseSpeed,
         .Hint = "How fast the noise field drifts",
         .Flags = ParamSpecFlag_Min},
        {.Member = &ShellMotionParams::NoiseScale,
         .Hint = "Noise features per sphere; higher means smaller bumps",
         .Flags = ParamSpecFlag_Min},
        {.Member = &ShellMotionParams::TangentialShare,
         .Hint = "Share of the noise displacement that slides along the surface",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 1.f,
         .Drag = UNIT_SHARE},
        {.Member = &ShellMotionParams::FacetTintStrength,
         .Hint = "How strongly facet height drives the trough-to-crest tint",
         .Flags = ParamSpecFlag_Min},
    }};
};
} // namespace Lim

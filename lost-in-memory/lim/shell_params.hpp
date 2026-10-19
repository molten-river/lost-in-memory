#pragma once

#include "lim/alias.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct ShellParams
{
    TKIT_REFLECT_DECLARE(ShellParams)
    f32 Radius = 30.f;
    u32 Subdivisions = 4;
    f32 LethalDensity = 0.06f;
};

struct ShellMotionParams
{
    TKIT_REFLECT_DECLARE(ShellMotionParams)
    f32 WaveAmplitude = 1.2f;
    f32 WaveSpeed = 0.6f;
    f32 WaveFrequency = 3.f;
    f32 NoiseAmplitude = 2.5f;
    f32 NoiseSpeed = 0.3f;
    f32 NoiseScale = 2.5f;
    f32 TangentialShare = 0.35f;
    f32 FacetTintStrength = 2.f;
};

[[nodiscard]] ShellParams Sanitize(ShellParams params);
[[nodiscard]] ShellMotionParams Sanitize(ShellMotionParams params);
} // namespace Lim

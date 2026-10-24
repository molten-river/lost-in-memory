#pragma once

#include "alias/alias.hpp"

#include "tkit/math/math.hpp"
#include "tkit/math/tensor.hpp"

namespace Lim
{
/// A unit direction from an azimuth in turns and a height fraction, uniform over the sphere for uniform inputs.
[[nodiscard]] inline f32v3 ToDirection(const f32 turns, const f32 height)
{
    const f32 z = 2.f * height - 1.f;
    const f32 ring = TKit::Math::SquareRoot(TKit::Math::Max(1.f - z * z, 0.f));
    const f32 azimuth = 2.f * TKit::Math::Pi() * turns;
    return f32v3{ring * TKit::Math::Cosine(azimuth), ring * TKit::Math::Sine(azimuth), z};
}
} // namespace Lim

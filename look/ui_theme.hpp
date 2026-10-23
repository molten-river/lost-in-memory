#pragma once

#include "alias/alias.hpp"

#include "onyx/color.hpp"

#include "tkit/math/math.hpp"

namespace Lim
{
struct UiTheme
{
    Onyx::Color Background;
    Onyx::Color Surface;
    Onyx::Color Border;
    Onyx::Color Text;
    Onyx::Color TextDim;
};

[[nodiscard]] inline Onyx::Color Mix(const Onyx::Color &from, const Onyx::Color &to, const f32 t)
{
    const f32 k = TKit::Math::Clamp(t, 0.f, 1.f);
    const auto channel = [k](const f32 a, const f32 b) { return a + (b - a) * k; };
    return Onyx::Color{channel(from.rgba[0], to.rgba[0]), channel(from.rgba[1], to.rgba[1]),
                       channel(from.rgba[2], to.rgba[2]), channel(from.rgba[3], to.rgba[3])};
}
} // namespace Lim

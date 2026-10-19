#pragma once

#include "lim/alias.hpp"

#include "onyx/color.hpp"

#include "tkit/math/math.hpp"
#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct Palette
{
    TKIT_REFLECT_DECLARE(Palette)
    Onyx::Color VoidBlack = Onyx::Color::FromHexadecimal(0x000000, false);
    Onyx::Color Untinted = Onyx::Color::FromHexadecimal(0xFFFFFF, false);
    Onyx::Color ShellTrough = Onyx::Color::FromHexadecimal(0x23272D, false);
    Onyx::Color ShellCrest = Onyx::Color::FromHexadecimal(0x9AA3AE, false);
    Onyx::Color EnemyPurple = Onyx::Color::FromHexadecimal(0x7B2FE0, false);
    Onyx::Color EnemyRed = Onyx::Color::FromHexadecimal(0xE3242B, false);
    Onyx::Color GoalGreen = Onyx::Color::FromHexadecimal(0x9CF5A8, false);
    Onyx::Color SolidLow = Onyx::Color::FromHexadecimal(0x4E535A, false);
    Onyx::Color SolidHigh = Onyx::Color::FromHexadecimal(0x8A8F96, false);
    Onyx::Color CoreLight = Onyx::Color::FromHexadecimal(0xDCE6F2, false);
    Onyx::Color Ambient = Onyx::Color::FromHexadecimal(0x2A3038, false);
    Onyx::Color UiBackground = Onyx::Color::FromHexadecimal(0x0C0E11, false);
    Onyx::Color UiSurface = Onyx::Color::FromHexadecimal(0x1B1F24, false);
    Onyx::Color UiBorder = Onyx::Color::FromHexadecimal(0x3B4149, false);
    Onyx::Color UiText = Onyx::Color::FromHexadecimal(0xD9DEE4, false);
    Onyx::Color UiTextDim = Onyx::Color::FromHexadecimal(0x7E868F, false);
};

[[nodiscard]] inline Onyx::Color Mix(const Onyx::Color &from, const Onyx::Color &to, const f32 t)
{
    const f32 k = TKit::Math::Clamp(t, 0.f, 1.f);
    const auto channel = [k](const f32 a, const f32 b) { return a + (b - a) * k; };
    return Onyx::Color{channel(from.rgba[0], to.rgba[0]), channel(from.rgba[1], to.rgba[1]),
                       channel(from.rgba[2], to.rgba[2]), channel(from.rgba[3], to.rgba[3])};
}
} // namespace Lim

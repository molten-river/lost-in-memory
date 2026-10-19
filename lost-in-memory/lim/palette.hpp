#pragma once

#include "lim/alias.hpp"
#include "onyx/color.hpp"
#include "tkit/math/math.hpp"

namespace Lim
{
inline const Onyx::Color VOID_BLACK = Onyx::Color::FromHexadecimal(0x000000, false);
inline const Onyx::Color UNTINTED = Onyx::Color::FromHexadecimal(0xFFFFFF, false);
inline const Onyx::Color SHELL_TROUGH = Onyx::Color::FromHexadecimal(0x23272D, false);
inline const Onyx::Color SHELL_CREST = Onyx::Color::FromHexadecimal(0x9AA3AE, false);
inline const Onyx::Color ENEMY_PURPLE = Onyx::Color::FromHexadecimal(0x7B2FE0, false);
inline const Onyx::Color ENEMY_RED = Onyx::Color::FromHexadecimal(0xE3242B, false);
inline const Onyx::Color GOAL_GREEN = Onyx::Color::FromHexadecimal(0x9CF5A8, false);
inline const Onyx::Color CORE_LIGHT = Onyx::Color::FromHexadecimal(0xDCE6F2, false);
inline const Onyx::Color AMBIENT = Onyx::Color::FromHexadecimal(0x2A3038, false);
inline const Onyx::Color UI_BACKGROUND = Onyx::Color::FromHexadecimal(0x0C0E11, false);
inline const Onyx::Color UI_SURFACE = Onyx::Color::FromHexadecimal(0x1B1F24, false);
inline const Onyx::Color UI_BORDER = Onyx::Color::FromHexadecimal(0x3B4149, false);
inline const Onyx::Color UI_TEXT = Onyx::Color::FromHexadecimal(0xD9DEE4, false);
inline const Onyx::Color UI_TEXT_DIM = Onyx::Color::FromHexadecimal(0x7E868F, false);

[[nodiscard]] inline Onyx::Color Mix(const Onyx::Color &from, const Onyx::Color &to, const f32 t)
{
    const f32 k = TKit::Math::Clamp(t, 0.f, 1.f);
    const auto channel = [k](const f32 a, const f32 b) { return a + (b - a) * k; };
    return Onyx::Color{channel(from.rgba[0], to.rgba[0]), channel(from.rgba[1], to.rgba[1]),
                       channel(from.rgba[2], to.rgba[2]), channel(from.rgba[3], to.rgba[3])};
}
} // namespace Lim

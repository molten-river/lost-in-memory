#pragma once

#include "alias/alias.hpp"

#include "onyx/view.hpp"

namespace Lim
{
inline constexpr Onyx::RenderViewSpecs RENDER_VIEW_SPECS{.AntiAliasingMode = Onyx::FXAA_QualityHigh,
                                                         .Flags = Onyx::RenderViewFlag_FXAA};
} // namespace Lim

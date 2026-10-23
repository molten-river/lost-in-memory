#pragma once

#include "alias/alias.hpp"

#include "onyx/window.hpp"

#include "tkit/math/tensor.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
/// The primary monitor's resolution, or none when no monitor or no usable video mode is reported.
[[nodiscard]] TKit::Optional<u32v2> GetMainScreenSize();

/// A borderless window covering the primary monitor, or Onyx's default decorated window when no monitor is reported.
[[nodiscard]] Onyx::WindowSpecs MakeBorderlessWindowSpecs(const char *title);
} // namespace Lim

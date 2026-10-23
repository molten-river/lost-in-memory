#pragma once

#include "alias/alias.hpp"

#include "tkit/container/span.hpp"

namespace Lim
{
/// Decides, by name, whether a reflected leaf is shown in the panel.
class VisibilityStrategy
{
  public:
    virtual ~VisibilityStrategy() = default;

    [[nodiscard]] virtual bool IsVisible(TKit::StringView section, TKit::StringView field) const = 0;
};
} // namespace Lim

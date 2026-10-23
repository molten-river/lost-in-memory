#pragma once

#include "alias/alias.hpp"
#include "tuning/visibility_strategy.hpp"

namespace Lim
{
/// The viewer tunes every field, so nothing is ever hidden.
class ViewerVisibility final : public VisibilityStrategy
{
  public:
    [[nodiscard]] bool IsVisible(TKit::StringView section, TKit::StringView field) const override;
};
} // namespace Lim

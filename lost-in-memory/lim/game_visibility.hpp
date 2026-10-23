#pragma once

#include "alias/alias.hpp"
#include "lim/level_params.hpp"
#include "lim/play_mode.hpp"
#include "tuning/visibility_strategy.hpp"

namespace Lim
{
/// The scope (by PlayMode) and feature-flag rules for every tuning section and field of LevelParams.
class GameVisibility : public VisibilityStrategy
{
  public:
    GameVisibility(PlayMode mode, const FeatureFlags &flags);

    [[nodiscard]] bool IsVisible(TKit::StringView section, TKit::StringView field) const override;

  private:
    PlayMode m_Mode;
    const FeatureFlags &m_Flags;
};
} // namespace Lim

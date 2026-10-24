#pragma once

#include "alias/alias.hpp"
#include "lim/level_params.hpp"
#include "lim/play_mode.hpp"
#include "look/font_library.hpp"
#include "shapes/shape_kit_params.hpp"
#include "tuning/param_panel.hpp"

namespace Lim
{
/// Centred overlay panel that edits the caller's level and kit params in place and reports which rebuild was requested.
class SandboxScreen
{
  public:
    enum class Action : u8
    {
        None,
        SameBytes,
        NewRegion,
        Reset
    };

    enum class Title : u8
    {
        Cleared,
        Terminated,
        Interrupted
    };

    SandboxScreen(Onyx::Window &window, FontLibrary &fonts);

    /// Call every frame before Onyx::Transfer(); an opacity of zero submits nothing but still flushes the overlay.
    [[nodiscard]] Action Draw(LevelParams &params, ShapeKitParams &kit, PlayMode mode, Title title, u64 address,
                              f32 opacity);

  private:
    [[nodiscard]] Action DrawPanel(LevelParams &params, ShapeKitParams &kit, PlayMode mode, Title title, u64 address);

    ParamPanel m_Panel;
    const LevelParams m_Defaults{};
    const ShapeKitParams m_KitDefaults{};
};
} // namespace Lim

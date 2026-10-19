#pragma once

#include "lim/alias.hpp"
#include "lim/font_library.hpp"
#include "lim/level_params.hpp"

#include "onyx/handle.hpp"
#include "onyx/overlay.hpp"

#include "tkit/utils/optional.hpp"

namespace Lim
{
/// Centred overlay panel that edits the caller's level params in place and reports which rebuild was requested.
class SandboxScreen
{
  public:
    enum class Action : u8
    {
        None,
        SameBytes,
        NewRegion
    };

    SandboxScreen(Onyx::Window &window, FontLibrary &fonts);

    /// Call every frame before Onyx::Transfer(); an opacity of zero submits nothing but still flushes the overlay.
    [[nodiscard]] Action Draw(LevelParams &params, bool won, u64 address, f32 opacity);

  private:
    [[nodiscard]] Action DrawPanel(LevelParams &params, bool won, u64 address, f32 opacity);
    void ApplyTheme(f32 textSize, f32 screenHeight, f32 alpha);

    Onyx::Resource m_Font;
    Onyx::Overlay *m_Overlay;
    Onyx::OverlayColors m_Colors;
    Onyx::OverlayStyleVariables m_Variables;
    TKit::Optional<f32v2> m_Size{};
};
} // namespace Lim

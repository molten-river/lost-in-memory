#pragma once

#include "alias/alias.hpp"
#include "look/font_library.hpp"
#include "viewer/viewer_params.hpp"

#include "onyx/handle.hpp"
#include "onyx/overlay.hpp"
#include "onyx/window.hpp"

#include "tkit/container/span.hpp"

#include <cstddef>

namespace Lim
{
/// A small top-left overlay with the selected record's bytes grouped by the kit's byte map, and the playback state.
class RecordHud
{
  public:
    struct Readout
    {
        u32 Record;
        TKit::Span<const std::byte> Bytes;
        u64 Source;
        TKit::StringView GalleryMode;
        TKit::StringView CameraMode;
        f32 Seconds;
        f32 TimeScale;
        bool Paused;
    };

    RecordHud(Onyx::Window &window, FontLibrary &fonts);

    /// Call every frame before Onyx::Transfer(); it always draws the overlay, which flushes it.
    void Draw(const ViewerLook &look, const Readout &readout);

  private:
    void ApplyTheme(const ViewerLook &look, f32 screenHeight);
    void DrawReadout(const ViewerPalette &colors, const Readout &readout);
    void Write(const Onyx::Color &color, TKit::StringView text);

    Onyx::Resource m_Font;
    Onyx::Overlay *m_Overlay;
    Onyx::OverlayStyleVariables m_Variables;
};
} // namespace Lim

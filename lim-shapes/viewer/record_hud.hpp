#pragma once

#include "alias/alias.hpp"
#include "tuning/param_panel.hpp"

#include "tkit/container/span.hpp"

#include <cstddef>

namespace Lim
{
/// Writes the selected record's bytes, labelled by the kit's byte map, and the playback state into the panel header.
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
        u32 Page;
        u32 PageCount;
        f32 Seconds;
        f32 TimeScale;
        bool Paused;
        TKit::StringView Timings;
    };

    /// Call between a true ParamPanel::Begin and its tabs.
    void Draw(ParamPanel &panel, const Readout &readout) const;
};
} // namespace Lim

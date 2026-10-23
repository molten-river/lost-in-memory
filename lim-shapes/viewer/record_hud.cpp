#include "viewer/record_hud.hpp"
#include "viewer/record_gallery.hpp"

#include "shapes/walk_mass_layout.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/static_array.hpp"

namespace Lim
{
namespace
{
using Line = TKit::StaticString<80>;

constexpr u32 LINE_COUNT = 6;
constexpr TKit::StringView HEX_DIGITS = "0123456789ABCDEF";
constexpr u32 NIBBLE_BITS = 4;
constexpr u32 NIBBLE_MASK = 0x0F;
constexpr usize RESERVED_FIRST_BYTE = RecordGallery::TAG_BYTE + 1;
constexpr usize PLACEMENT_FIRST_BYTE = WALK_MASS_SIZE_BYTE + 1;

constexpr TKit::StringView CAPTION = "LOOK, CAMERA AND GALLERY ARE LIVE; SHAPE KIT CHANGES REBAKE ON APPLY\n"
                                     "TAB CURSOR  G GRID  LEFT RIGHT SELECT  O CAMERA  R REGION  ESC QUIT\n"
                                     "P PAUSE  [ ] SPEED  , . SCRUB  BACKSPACE RESET\n"
                                     "FLY: MOUSE WASD SPACE CTRL Q E   ORBIT: LEFT DRAG, W S";

static_assert(RESERVED_FIRST_BYTE < WALK_MASS_SIZE_BYTE && PLACEMENT_FIRST_BYTE < WALK_MASS_SECTIONS_BYTE,
              "The tag, reserved, size and placement bytes come in that order before the sections");

[[nodiscard]] u32 ByteAt(const TKit::Span<const std::byte> bytes, const usize index)
{
    return std::to_integer<u32>(bytes[index]);
}

void AppendHex(Line &line, const TKit::Span<const std::byte> bytes, const usize first, const usize end)
{
    for (usize index = first; index < end; ++index)
    {
        const u32 value = ByteAt(bytes, index);
        line += HEX_DIGITS[value >> NIBBLE_BITS];
        line += HEX_DIGITS[value & NIBBLE_MASK];
    }
}

/// One labelled line of fixed-size slots, each slot's bytes written together and slots separated by a space.
[[nodiscard]] Line FormatSlots(const TKit::StringView label, const TKit::Span<const std::byte> bytes, const usize first,
                               const usize count, const usize stride)
{
    Line line = Line::Format("{} ", label);
    for (usize slot = 0; slot < count; ++slot)
    {
        AppendHex(line, bytes, first + slot * stride, first + (slot + 1) * stride);
        line += ' ';
    }
    return line;
}
} // namespace

void RecordHud::Draw(ParamPanel &panel, const Readout &readout) const
{
    const TKit::Span<const std::byte> bytes = readout.Bytes;
    const u32 offset = RecordGallery::GENOME_BYTES + RecordGallery::RECORD_BYTES * readout.Record;

    Line unread = Line::Format("UNREAD   ");
    AppendHex(unread, bytes, RESERVED_FIRST_BYTE, WALK_MASS_SIZE_BYTE);
    unread += ' ';
    AppendHex(unread, bytes, PLACEMENT_FIRST_BYTE, WALK_MASS_SECTIONS_BYTE);

    const TKit::FixedArray<Line, LINE_COUNT> lines{
        Line::Format("SOURCE 0x{:016X} +0x{:03X}", readout.Source, offset),
        Line::Format("{} / {}  TIME {:.2f} S  x{:g}  {}", readout.GalleryMode, readout.CameraMode, readout.Seconds,
                     readout.TimeScale, readout.Paused ? "PAUSED" : "PLAYING"),
        Line::Format("TAG {:02X}  SIZE {:02X}  LATTICE {:02X}", ByteAt(bytes, RecordGallery::TAG_BYTE),
                     ByteAt(bytes, WALK_MASS_SIZE_BYTE), ByteAt(bytes, WALK_MASS_LATTICE_BYTE)),
        FormatSlots("SECTIONS", bytes, WALK_MASS_SECTIONS_BYTE, WALK_MASS_SECTION_COUNT, WALK_MASS_SECTION_BYTES),
        FormatSlots("JOINTS  ", bytes, WALK_MASS_JOINTS_BYTE, WALK_MASS_JOINT_COUNT, WALK_MASS_JOINT_BYTES),
        unread};

    TKit::FixedArray<TKit::StringView, LINE_COUNT> views{};
    for (u32 i = 0; i < LINE_COUNT; ++i)
        views[i] = lines[i];

    const TKit::StaticString16 title =
        TKit::StaticString16::Format("RECORD {:02}/{}", readout.Record + 1, RecordGallery::RECORD_COUNT);
    panel.Header(title, views, CAPTION);
}
} // namespace Lim

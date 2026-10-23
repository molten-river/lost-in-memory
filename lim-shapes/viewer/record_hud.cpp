#include "viewer/record_hud.hpp"
#include "viewer/record_gallery.hpp"

#include "shapes/walk_mass_layout.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/math.hpp"

namespace Lim
{
namespace
{
using Line = TKit::StaticString64;

constexpr TKit::StringView WINDOW_LABEL = "lim-shapes-hud";
constexpr Onyx::OverlayWindowFlags WINDOW_FLAGS =
    Onyx::OverlayWindowFlag_NoMove | Onyx::OverlayWindowFlag_NoResize | Onyx::OverlayWindowFlag_AutoResize |
    Onyx::OverlayWindowFlag_NoHeaderBar | Onyx::OverlayWindowFlag_NoCollapse | Onyx::OverlayWindowFlag_NoScrollBar |
    Onyx::OverlayWindowFlag_MousePassThrough | Onyx::OverlayWindowFlag_NoBringToFocus;

constexpr f32 REFERENCE_SCREEN_HEIGHT = 1080.f;
constexpr f32 MARGIN = 16.f;
constexpr f32 BORDER_WIDTH = 1.f;
constexpr f32 WINDOW_PADDING = 12.f;
constexpr f32 TITLE_SCALE = 1.25f;
constexpr f32 JOINT_ACCENT = 0.45f;
constexpr f32 ALTERNATE_DIM = 0.4f;

constexpr TKit::FixedArray<Onyx::OverlayStyleVariable, 3> SCALED_VARIABLES{
    Onyx::OverlayStyle_UnicodeSize, Onyx::OverlayStyle_ChildGap, Onyx::OverlayStyle_WindowPadding};

constexpr u32 HEX_COLUMNS = 16;
constexpr u32 HEX_ROWS = 4;
constexpr TKit::StringView HEX_DIGITS = "0123456789ABCDEF";
constexpr u32 NIBBLE_BITS = 4;
constexpr u32 NIBBLE_MASK = 0x0F;

constexpr TKit::FixedArray<TKit::StringView, 3> CONTROL_HINTS{
    "TAB PANEL  G GRID  LEFT RIGHT SELECT  O CAMERA  R REGION",
    "P PAUSE  [ ] SPEED  , . SCRUB  BACKSPACE RESET  ESC QUIT",
    "FLY: MOUSE WASD SPACE CTRL Q E  ORBIT: DRAG W S",
};

static_assert(HEX_COLUMNS * HEX_ROWS == RecordGallery::RECORD_BYTES, "The hex grid shows one whole record");

enum class ByteTint : u8
{
    Unread,
    Scalar,
    Section,
    SectionAlternate,
    Joint,
    JointAlternate
};

constexpr u32 BYTE_TINT_COUNT = u32(ByteTint::JointAlternate) + 1;
using ByteTints = TKit::FixedArray<Onyx::Color, BYTE_TINT_COUNT>;

struct LegendEntry
{
    ByteTint Tint;
    TKit::StringView Text;
};

constexpr TKit::FixedArray<LegendEntry, 4> LEGEND{{
    {ByteTint::Scalar, "SIZE LATTICE "},
    {ByteTint::Section, "SECTIONS "},
    {ByteTint::Joint, "JOINTS "},
    {ByteTint::Unread, "UNREAD"},
}};

/// Size and lattice are whole-mass scalars; sections and joints alternate per 3-byte slot so each slot reads apart.
[[nodiscard]] constexpr ByteTint ClassifyByte(const usize index)
{
    if (index == WALK_MASS_SIZE_BYTE || index == WALK_MASS_LATTICE_BYTE)
        return ByteTint::Scalar;
    if (index >= WALK_MASS_JOINTS_BYTE)
        return ((index - WALK_MASS_JOINTS_BYTE) / WALK_MASS_JOINT_BYTES) % 2 == 0 ? ByteTint::Joint
                                                                                  : ByteTint::JointAlternate;
    if (index >= WALK_MASS_SECTIONS_BYTE)
        return ((index - WALK_MASS_SECTIONS_BYTE) / WALK_MASS_SECTION_BYTES) % 2 == 0 ? ByteTint::Section
                                                                                      : ByteTint::SectionAlternate;
    return ByteTint::Unread;
}

[[nodiscard]] ByteTints MakeTints(const ViewerPalette &colors)
{
    const Onyx::Color joint = Mix(colors.UiText, colors.Selected, JOINT_ACCENT);
    return ByteTints{colors.UiTextDim,
                     colors.Selected,
                     colors.UiText,
                     Mix(colors.UiText, colors.UiTextDim, ALTERNATE_DIM),
                     joint,
                     Mix(joint, colors.UiTextDim, ALTERNATE_DIM)};
}

[[nodiscard]] Onyx::OverlayStyleVariables CreateVariables()
{
    Onyx::OverlayStyleVariables variables = Onyx::CreateDefaultOverlayVariables();
    variables[Onyx::OverlayStyle_WindowBorderWidth] = BORDER_WIDTH;
    variables[Onyx::OverlayStyle_LineWidth] = BORDER_WIDTH;
    variables[Onyx::OverlayStyle_WindowPadding] = WINDOW_PADDING;
    return variables;
}
} // namespace

RecordHud::RecordHud(Onyx::Window &window, FontLibrary &fonts)
    : m_Font(fonts.Get()),
      m_Overlay(window.CreateOverlay(Onyx::OverlaySpecs{
          .Layout = {.RootAlignment = {Onyx::Alignment_Left, Onyx::Alignment_Top}, .Font = m_Font}})),
      m_Variables(CreateVariables())
{
}

void RecordHud::Draw(const ViewerLook &look, const Readout &readout)
{
    Onyx::Overlay &ui = *m_Overlay;
    const f32 screenHeight = f32(ui.GetMainNativeWindow()->Window->GetScreenDimensions()[1]);
    if (screenHeight > 0.f)
    {
        ApplyTheme(look, screenHeight);
        ui.SetNextWindowPosition(f32v2{TKit::Math::Round(MARGIN * screenHeight / REFERENCE_SCREEN_HEIGHT)});
        if (ui.BeginWindow(Onyx::OverlayLabel{WINDOW_LABEL}, WINDOW_FLAGS))
        {
            DrawReadout(look.Colors, readout);
            ui.EndWindow();
        }
    }
    ui.Draw();
}

void RecordHud::ApplyTheme(const ViewerLook &look, const f32 screenHeight)
{
    const f32 fontPixels = look.TextSize * screenHeight / REFERENCE_SCREEN_HEIGHT;
    const f32 scale = fontPixels / m_Variables[Onyx::OverlayStyle_FontSize];

    Onyx::OverlayStyle &style = m_Overlay->GetStyle();
    style.Variables = m_Variables;
    style.Variables[Onyx::OverlayStyle_FontSize] = fontPixels;
    for (const Onyx::OverlayStyleVariable variable : SCALED_VARIABLES)
        style.Variables[variable] *= scale;

    const ViewerPalette &colors = look.Colors;
    style.Colors[Onyx::OverlayColor_WindowBackgroundExpanded] = colors.UiBackground;
    style.Colors[Onyx::OverlayColor_WindowBorderIdle] = colors.UiBorder;
    style.Colors[Onyx::OverlayColor_WindowBorderHovered] = colors.UiBorder;
    style.Colors[Onyx::OverlayColor_WindowBorderPressed] = colors.UiBorder;
    style.Colors[Onyx::OverlayColor_Line] = colors.UiBorder;

    m_Overlay->GetMainNativeWindow()->Context->Font(m_Font);
}

void RecordHud::DrawReadout(const ViewerPalette &colors, const Readout &readout)
{
    Onyx::Overlay &ui = *m_Overlay;
    const u32 offset = RecordGallery::GENOME_BYTES + RecordGallery::RECORD_BYTES * readout.Record;
    const ByteTints tints = MakeTints(colors);

    ui.PushStyleVar(Onyx::OverlayStyle_FontSize, TITLE_SCALE * ui.GetStyle()[Onyx::OverlayStyle_FontSize]);
    Write(colors.UiText, Line::Format("RECORD {:02}/{}", readout.Record + 1, RecordGallery::RECORD_COUNT));
    ui.PopStyleVar();
    Write(colors.UiTextDim, Line::Format("SOURCE 0x{:016X} +0x{:03X}", readout.Source, offset));
    ui.HorizontalLine();

    ui.PushDirection(Onyx::LayoutDirection_LeftToRight, 0.f);
    for (const LegendEntry &entry : LEGEND)
        Write(tints[u32(entry.Tint)], entry.Text);
    ui.PopDirection();

    for (u32 row = 0; row < HEX_ROWS; ++row)
    {
        ui.PushDirection(Onyx::LayoutDirection_LeftToRight, 0.f);
        Write(colors.UiTextDim, Line::Format("{:03X}  ", offset + row * HEX_COLUMNS));

        Line run{};
        ByteTint runTint = ClassifyByte(row * HEX_COLUMNS);
        for (u32 column = 0; column < HEX_COLUMNS; ++column)
        {
            const u32 index = row * HEX_COLUMNS + column;
            const ByteTint tint = ClassifyByte(index);
            if (tint != runTint)
            {
                Write(tints[u32(runTint)], run);
                run.Clear();
                runTint = tint;
            }
            const u32 value = std::to_integer<u32>(readout.Bytes[index]);
            run += HEX_DIGITS[value >> NIBBLE_BITS];
            run += HEX_DIGITS[value & NIBBLE_MASK];
            run += ' ';
        }
        Write(tints[u32(runTint)], run);
        ui.PopDirection();
    }
    ui.HorizontalLine();

    Write(colors.UiText, Line::Format("{} / {}", readout.GalleryMode, readout.CameraMode));
    Write(colors.UiText, Line::Format("TIME {:.2f} S  x{:g}  {}", readout.Seconds, readout.TimeScale,
                                      readout.Paused ? "PAUSED" : "PLAYING"));
    ui.HorizontalLine();
    for (const TKit::StringView &hint : CONTROL_HINTS)
        Write(colors.UiTextDim, hint);
}

/// Writes the text colour directly, because PushStyleColor backs up the colour already scaled by the style alpha.
void RecordHud::Write(const Onyx::Color &color, const TKit::StringView text)
{
    m_Overlay->GetStyle().Colors[Onyx::OverlayColor_Text] = color;
    m_Overlay->TextRaw(text);
}
} // namespace Lim

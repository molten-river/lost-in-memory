#include "lim/sandbox_screen.hpp"

#include "lim/palette.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/math.hpp"

namespace Lim
{
namespace
{
constexpr TKit::StringView WINDOW_LABEL = "lim-sandbox";
constexpr Onyx::OverlayWindowFlags WINDOW_FLAGS =
    Onyx::OverlayWindowFlag_NoMove | Onyx::OverlayWindowFlag_NoResize | Onyx::OverlayWindowFlag_AutoResize |
    Onyx::OverlayWindowFlag_NoHeaderBar | Onyx::OverlayWindowFlag_NoCollapse | Onyx::OverlayWindowFlag_NoScrollBar;
constexpr Onyx::OverlaySliderFlags SLIDER_FLAGS = Onyx::OverlaySliderFlag_ClampOnInput;

constexpr TKit::StringView WON_TITLE = "REGION CLEARED";
constexpr TKit::StringView DIED_TITLE = "PROCESS TERMINATED";
constexpr f32 TITLE_SCALE = 1.5f;
constexpr f32 COLUMN_GAP_SCALE = 2.f;

constexpr f32 REFERENCE_SCREEN_HEIGHT = 1080.f;
constexpr f32 BORDER_WIDTH = 1.f;
constexpr f32 WINDOW_PADDING = 16.f;
constexpr f32 SLIDER_WIDTH = 180.f;
constexpr f32 HOVER_TINT = 0.25f;
constexpr f32 PRESS_TINT = 0.5f;

constexpr TKit::FixedArray<Onyx::OverlayStyleVariable, 18> SCALED_VARIABLES{Onyx::OverlayStyle_UnicodeSize,
                                                                            Onyx::OverlayStyle_IndentWidth,
                                                                            Onyx::OverlayStyle_ChildGap,
                                                                            Onyx::OverlayStyle_LineWidth,
                                                                            Onyx::OverlayStyle_WindowPadding,
                                                                            Onyx::OverlayStyle_WindowBorderWidth,
                                                                            Onyx::OverlayStyle_HeaderPadding,
                                                                            Onyx::OverlayStyle_IconWidth,
                                                                            Onyx::OverlayStyle_ContentAreaPadding,
                                                                            Onyx::OverlayStyle_ScrollBarWidth,
                                                                            Onyx::OverlayStyle_ScrollBarGap,
                                                                            Onyx::OverlayStyle_WidgetSize,
                                                                            Onyx::OverlayStyle_WidgetPadding,
                                                                            Onyx::OverlayStyle_WidgetMinimumWidth,
                                                                            Onyx::OverlayStyle_MenuPadding,
                                                                            Onyx::OverlayStyle_DropDownHeightRegular,
                                                                            Onyx::OverlayStyle_DropDownHeightSmall,
                                                                            Onyx::OverlayStyle_TooltipPadding};

constexpr const char *INTEGER = "{}";
constexpr const char *WHOLE = "{:.0f}";
constexpr const char *TWO_DECIMALS = "{:.2f}";
constexpr const char *THREE_DECIMALS = "{:.3f}";

template <typename Group, TKit::Numeric T> struct Slider
{
    TKit::StringView Label;
    T Group::*Field;
    T Min;
    T Max;
    const char *Format;
};

/// Two sliders over one ordered pair of fields: moving either bound past the other drags the other along.
template <typename Group, TKit::Numeric T> struct Range
{
    TKit::StringView LowLabel;
    TKit::StringView HighLabel;
    T Group::*Low;
    T Group::*High;
    T Min;
    T Max;
    const char *Format;
};

constexpr TKit::FixedArray<Slider<ShellParams, f32>, 5> SHELL_REALS{
    {"RADIUS", &ShellParams::Radius, 15.f, 60.f, TWO_DECIMALS},
    {"WAVE AMPLITUDE", &ShellParams::WaveAmplitude, 0.f, 4.f, TWO_DECIMALS},
    {"WAVE SPEED", &ShellParams::WaveSpeed, 0.f, 2.f, TWO_DECIMALS},
    {"WAVE FREQUENCY", &ShellParams::WaveFrequency, 0.5f, 8.f, TWO_DECIMALS},
    {"LETHAL DENSITY", &ShellParams::LethalDensity, 0.f, 0.3f, THREE_DECIMALS}};

constexpr TKit::FixedArray<Slider<ShellParams, u32>, 1> SHELL_COUNTS{
    {"SUBDIVISIONS", &ShellParams::Subdivisions, 1U, 5U, INTEGER}};

constexpr TKit::FixedArray<Range<EnemyParams, u32>, 2> ENEMY_COUNT_RANGES{
    {"COUNT MIN", "COUNT MAX", &EnemyParams::MinCount, &EnemyParams::MaxCount, 0U, 30U, INTEGER},
    {"PRIMITIVES MIN", "PRIMITIVES MAX", &EnemyParams::MinPrimitives, &EnemyParams::MaxPrimitives, 1U, 6U, INTEGER}};

constexpr TKit::FixedArray<Range<EnemyParams, f32>, 2> ENEMY_REAL_RANGES{
    {"SIZE MIN", "SIZE MAX", &EnemyParams::MinSize, &EnemyParams::MaxSize, 0.5f, 6.f, TWO_DECIMALS},
    {"DRIFT MIN", "DRIFT MAX", &EnemyParams::MinDriftSpeed, &EnemyParams::MaxDriftSpeed, 0.f, 12.f, TWO_DECIMALS}};

constexpr TKit::FixedArray<Slider<EnemyParams, f32>, 2> ENEMY_REALS{
    {"BLEND RADIUS", &EnemyParams::BlendRadius, 0.f, 2.f, TWO_DECIMALS},
    {"TUMBLE SPEED", &EnemyParams::TumbleSpeed, 0.f, 2.f, TWO_DECIMALS}};

constexpr TKit::FixedArray<Slider<GoalParams, f32>, 4> GOAL_REALS{
    {"SHELL CHANCE", &GoalParams::ShellChance, 0.f, 1.f, TWO_DECIMALS},
    {"SIZE", &GoalParams::Size, 0.5f, 4.f, TWO_DECIMALS},
    {"DRIFT SPEED", &GoalParams::DriftSpeed, 0.f, 6.f, TWO_DECIMALS},
    {"SPAWN DISTANCE", &GoalParams::MinSpawnDistance, 0.f, 40.f, TWO_DECIMALS}};

constexpr TKit::FixedArray<Slider<LevelParams, f32>, 1> DISPLAY_REALS{
    {"TEXT SIZE", &LevelParams::TextSize, 10.f, 28.f, WHOLE}};

[[nodiscard]] Onyx::OverlayColors CreateColors()
{
    const Onyx::Color hovered = Mix(UI_SURFACE, UI_TEXT_DIM, HOVER_TINT);
    const Onyx::Color pressed = Mix(UI_SURFACE, UI_TEXT_DIM, PRESS_TINT);

    Onyx::OverlayPalette palette{};
    palette[Onyx::OverlayPalette_Idle0] = UI_SURFACE;
    palette[Onyx::OverlayPalette_Idle1] = UI_BORDER;
    palette[Onyx::OverlayPalette_Idle2] = UI_BACKGROUND;

    palette[Onyx::OverlayPalette_Hovered0] = hovered;
    palette[Onyx::OverlayPalette_Hovered1] = Mix(UI_BORDER, UI_TEXT_DIM, HOVER_TINT);
    palette[Onyx::OverlayPalette_Hovered2] = hovered;
    palette[Onyx::OverlayPalette_Hovered3] = hovered;

    palette[Onyx::OverlayPalette_Pressed0] = pressed;
    palette[Onyx::OverlayPalette_Pressed1] = pressed;
    palette[Onyx::OverlayPalette_Pressed2] = pressed;

    palette[Onyx::OverlayPalette_Text0] = UI_TEXT;

    palette[Onyx::OverlayPalette_Inner0] = UI_TEXT_DIM;
    palette[Onyx::OverlayPalette_Inner1] = UI_TEXT;

    palette[Onyx::OverlayPalette_Background0] = UI_BACKGROUND;
    palette[Onyx::OverlayPalette_Background1] = UI_BORDER;
    palette[Onyx::OverlayPalette_Background2] = UI_BACKGROUND;

    Onyx::OverlayColors colors = Onyx::CreateOverlayColorsFromPalette(palette);
    colors[Onyx::OverlayColor_WindowBorderIdle] = UI_BORDER;
    colors[Onyx::OverlayColor_WindowBorderHovered] = UI_BORDER;
    colors[Onyx::OverlayColor_WindowBorderPressed] = UI_BORDER;
    colors[Onyx::OverlayColor_ButtonIdle] = UI_BORDER;
    colors[Onyx::OverlayColor_ButtonHovered] = Mix(UI_BORDER, UI_TEXT_DIM, HOVER_TINT);
    colors[Onyx::OverlayColor_ButtonPressed] = Mix(UI_BORDER, UI_TEXT_DIM, PRESS_TINT);
    return colors;
}

[[nodiscard]] Onyx::OverlayStyleVariables CreateVariables()
{
    Onyx::OverlayStyleVariables variables = Onyx::CreateDefaultOverlayVariables();
    variables[Onyx::OverlayStyle_WindowBorderWidth] = BORDER_WIDTH;
    variables[Onyx::OverlayStyle_LineWidth] = BORDER_WIDTH;
    variables[Onyx::OverlayStyle_WindowPadding] = WINDOW_PADDING;
    variables[Onyx::OverlayStyle_WidgetMinimumWidth] = SLIDER_WIDTH;
    return variables;
}

/// Writes the text colour directly, because PushStyleColor backs up the colour already scaled by the style alpha.
void DimText(Onyx::Overlay &ui, const TKit::StringView text)
{
    Onyx::OverlayColors &colors = ui.GetStyle().Colors;
    const Onyx::Color regular = colors[Onyx::OverlayColor_Text];
    colors[Onyx::OverlayColor_Text] = UI_TEXT_DIM;
    ui.TextRaw(text);
    colors[Onyx::OverlayColor_Text] = regular;
}

void DrawHeader(Onyx::Overlay &ui, const bool won, const u64 address)
{
    ui.PushStyleVar(Onyx::OverlayStyle_FontSize, TITLE_SCALE * ui.GetStyle()[Onyx::OverlayStyle_FontSize]);
    ui.TextRaw(won ? WON_TITLE : DIED_TITLE);
    ui.PopStyleVar();

    const TKit::StaticString32 source = TKit::StaticString32::Format("SOURCE 0x{:016X}", address);
    DimText(ui, source);
}

void BeginColumn(Onyx::Overlay &ui, const TKit::StringView title)
{
    ui.PushDirection(Onyx::LayoutDirection_TopToBottom);
    DimText(ui, title);
}

template <typename Group, TKit::Numeric T, usize N>
void DrawSliders(Onyx::Overlay &ui, const TKit::FixedArray<Slider<Group, T>, N> &sliders, Group &group)
{
    for (const Slider<Group, T> &slider : sliders)
        ui.HorizontalSlider(slider.Label, &(group.*slider.Field), slider.Min, slider.Max, slider.Format, 1,
                            SLIDER_FLAGS);
}

template <typename Group, TKit::Numeric T, usize N>
void DrawRanges(Onyx::Overlay &ui, const TKit::FixedArray<Range<Group, T>, N> &ranges, Group &group)
{
    for (const Range<Group, T> &range : ranges)
    {
        T &low = group.*range.Low;
        T &high = group.*range.High;
        if (ui.HorizontalSlider(range.LowLabel, &low, range.Min, range.Max, range.Format, 1, SLIDER_FLAGS))
            high = TKit::Math::Max(low, high);
        if (ui.HorizontalSlider(range.HighLabel, &high, range.Min, range.Max, range.Format, 1, SLIDER_FLAGS))
            low = TKit::Math::Min(low, high);
    }
}

[[nodiscard]] SandboxScreen::Action DrawButtons(Onyx::Overlay &ui)
{
    ui.BeginPanel(Onyx::Overlay::LyPnPar{.Direction = Onyx::LayoutDirection_LeftToRight,
                                         .Sizing = {Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Fit()},
                                         .ChildGap = ui.GetStyle()[Onyx::OverlayStyle_ChildGap]});
    SandboxScreen::Action action = SandboxScreen::Action::None;
    if (ui.Button("SAME BYTES", Onyx::OverlayButtonFlag_SpanFullWidth))
        action = SandboxScreen::Action::SameBytes;
    if (ui.Button("NEW REGION", Onyx::OverlayButtonFlag_SpanFullWidth))
        action = SandboxScreen::Action::NewRegion;
    ui.EndPanel();
    return action;
}
} // namespace

SandboxScreen::SandboxScreen(Onyx::Window &window, FontLibrary &fonts)
    : m_Font(fonts.Get()),
      m_Overlay(window.CreateOverlay(Onyx::OverlaySpecs{
          .Layout = {.RootAlignment = {Onyx::Alignment_Left, Onyx::Alignment_Top}, .Font = m_Font}})),
      m_Colors(CreateColors()), m_Variables(CreateVariables())
{
}

SandboxScreen::Action SandboxScreen::Draw(LevelParams &params, const bool won, const u64 address, const f32 opacity)
{
    const Action action = opacity > 0.f ? DrawPanel(params, won, address, opacity) : Action::None;
    m_Overlay->Draw();
    return action;
}

SandboxScreen::Action SandboxScreen::DrawPanel(LevelParams &params, const bool won, const u64 address,
                                               const f32 opacity)
{
    Onyx::Overlay &ui = *m_Overlay;
    const Onyx::NativeWindow &native = *ui.GetMainNativeWindow();
    const f32v2 screen{native.Window->GetScreenDimensions()};
    if (screen[1] <= 0.f)
        return Action::None;

    ApplyTheme(params.TextSize, screen[1], m_Size ? opacity : 0.f);
    const f32v2 size = m_Size ? *m_Size : f32v2{0.f};
    ui.SetNextWindowPosition(
        f32v2{TKit::Math::Round(0.5f * (screen[0] - size[0])), TKit::Math::Round(0.5f * (screen[1] - size[1]))});

    const Onyx::OverlayLabel label{WINDOW_LABEL};
    if (!ui.BeginWindow(label, WINDOW_FLAGS))
        return Action::None;
    if (const Onyx::LayoutElementQueryInfo *element = ui.GetActiveLayout()->QueryElement(label.Id))
        m_Size = element->Size;

    DrawHeader(ui, won, address);
    ui.HorizontalLine();

    ui.PushDirection(Onyx::LayoutDirection_LeftToRight, COLUMN_GAP_SCALE * ui.GetStyle()[Onyx::OverlayStyle_ChildGap]);
    BeginColumn(ui, "[01] SHELL");
    DrawSliders(ui, SHELL_REALS, params.Shell);
    DrawSliders(ui, SHELL_COUNTS, params.Shell);
    ui.PopDirection();
    ui.VerticalLine();
    BeginColumn(ui, "[02] ENEMIES");
    DrawRanges(ui, ENEMY_COUNT_RANGES, params.Enemies);
    DrawRanges(ui, ENEMY_REAL_RANGES, params.Enemies);
    DrawSliders(ui, ENEMY_REALS, params.Enemies);
    ui.PopDirection();
    ui.VerticalLine();
    BeginColumn(ui, "[03] GOAL");
    DrawSliders(ui, GOAL_REALS, params.Goal);
    DimText(ui, "[04] DISPLAY");
    DrawSliders(ui, DISPLAY_REALS, params);
    ui.PopDirection();
    ui.PopDirection();

    ui.HorizontalLine();
    const Action action = DrawButtons(ui);
    ui.EndWindow();
    return action;
}

void SandboxScreen::ApplyTheme(const f32 textSize, const f32 screenHeight, const f32 alpha)
{
    const f32 fontPixels = textSize * screenHeight / REFERENCE_SCREEN_HEIGHT;
    const f32 scale = fontPixels / m_Variables[Onyx::OverlayStyle_FontSize];

    Onyx::OverlayStyle &style = m_Overlay->GetStyle();
    style.Colors = m_Colors;
    style.Variables = m_Variables;
    style.Variables[Onyx::OverlayStyle_FontSize] = fontPixels;
    for (const Onyx::OverlayStyleVariable variable : SCALED_VARIABLES)
        style.Variables[variable] *= scale;
    style.Variables[Onyx::OverlayStyle_Alpha] = TKit::Math::Clamp(alpha, 0.f, 1.f);

    m_Overlay->GetMainNativeWindow()->Context->Font(m_Font);
}
} // namespace Lim

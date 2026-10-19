#include "lim/sandbox_screen.hpp"

#include "lim/palette.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/reflection/lim/form_params.hpp"
#include "tkit/reflection/lim/level_params.hpp"
#include "tkit/reflection/lim/palette.hpp"
#include "tkit/reflection/lim/player_params.hpp"
#include "tkit/reflection/lim/shape_params.hpp"
#include "tkit/reflection/lim/shell_params.hpp"

namespace Lim
{
namespace
{
constexpr TKit::StringView WINDOW_LABEL = "lim-sandbox";
constexpr TKit::StringView TAB_BAR_ID = "lim-tuning-tabs";
constexpr TKit::StringView SCROLL_LABEL = "lim-tuning-scroll";
constexpr Onyx::OverlayWindowFlags WINDOW_FLAGS =
    Onyx::OverlayWindowFlag_NoMove | Onyx::OverlayWindowFlag_NoResize | Onyx::OverlayWindowFlag_AutoResize |
    Onyx::OverlayWindowFlag_NoHeaderBar | Onyx::OverlayWindowFlag_NoCollapse | Onyx::OverlayWindowFlag_NoScrollBar;
constexpr Onyx::OverlayTabFlags TAB_FLAGS = Onyx::OverlayTabFlag_StartOpen;
constexpr Onyx::OverlayColorFlags COLOR_FLAGS = Onyx::OverlayColorFlag_NoAlpha | Onyx::OverlayColorFlag_NoColorMarkers;

constexpr TKit::StringView WON_TITLE = "REGION CLEARED";
constexpr TKit::StringView DIED_TITLE = "PROCESS TERMINATED";
constexpr f32 TITLE_SCALE = 1.5f;

constexpr f32 REFERENCE_SCREEN_HEIGHT = 1080.f;
constexpr f32 SCROLL_HEIGHT_SHARE = 0.55f;
constexpr f32 BORDER_WIDTH = 1.f;
constexpr f32 WINDOW_PADDING = 16.f;
constexpr f32 WIDGET_WIDTH = 280.f;
constexpr f32 HOVER_TINT = 0.25f;
constexpr f32 PRESS_TINT = 0.5f;

constexpr TKit::FixedArray<Onyx::OverlayStyleVariable, 26> SCALED_VARIABLES{Onyx::OverlayStyle_UnicodeSize,
                                                                            Onyx::OverlayStyle_IndentWidth,
                                                                            Onyx::OverlayStyle_ChildGap,
                                                                            Onyx::OverlayStyle_TabPadding,
                                                                            Onyx::OverlayStyle_TabGap,
                                                                            Onyx::OverlayStyle_TabBarWidth,
                                                                            Onyx::OverlayStyle_LineWidth,
                                                                            Onyx::OverlayStyle_SeparatorTextOffset,
                                                                            Onyx::OverlayStyle_WindowPadding,
                                                                            Onyx::OverlayStyle_WindowBorderWidth,
                                                                            Onyx::OverlayStyle_HeaderPadding,
                                                                            Onyx::OverlayStyle_IconWidth,
                                                                            Onyx::OverlayStyle_ContentAreaPadding,
                                                                            Onyx::OverlayStyle_ScrollBarWidth,
                                                                            Onyx::OverlayStyle_ScrollBarGap,
                                                                            Onyx::OverlayStyle_ScrollSensitivity,
                                                                            Onyx::OverlayStyle_WidgetSize,
                                                                            Onyx::OverlayStyle_WidgetPadding,
                                                                            Onyx::OverlayStyle_WidgetMinimumWidth,
                                                                            Onyx::OverlayStyle_MenuPadding,
                                                                            Onyx::OverlayStyle_DropDownHeightRegular,
                                                                            Onyx::OverlayStyle_DropDownHeightSmall,
                                                                            Onyx::OverlayStyle_TooltipPadding,
                                                                            Onyx::OverlayStyle_ColorTooltipSize,
                                                                            Onyx::OverlayStyle_ColorPickerSize,
                                                                            Onyx::OverlayStyle_ColorPickerPreviewSize};

constexpr usize LABEL_CAPACITY = 48;
using Label = TKit::StaticString<LABEL_CAPACITY>;

constexpr const char *INTEGER = "{}";
constexpr const char *WHOLE = "{:.0f}";
constexpr const char *TWO_DECIMALS = "{:.2f}";
constexpr TKit::FixedArray<const char *, 5> REAL_FORMATS{"{:.1f}", "{:.2f}", "{:.3f}", "{:.4f}", "{:.5f}"};
constexpr f32 DECADE = 10.f;

constexpr f32 REFERENCE_SHARE_PER_PIXEL = 0.005f;
constexpr f32 RANGE_SCALE = 4.f;
constexpr f32 INTEGER_DRAG_SPEED = 0.05f;

struct DragRange
{
    f32 Min;
    f32 Max;
    f32 Speed;
    const char *Format;
};

/// Keyed by the name of the member holding the struct plus the field name, because reflection carries no type names.
struct DragOverride
{
    TKit::StringView Section;
    TKit::StringView Field;
    DragRange Range;
};

constexpr TKit::FixedArray<DragOverride, 7> DRAG_OVERRIDES{{
    {"LookFeel", "TextSize", {10.f, 28.f, INTEGER_DRAG_SPEED, WHOLE}},
    {"ShellMotion", "TangentialShare", {0.f, 1.f, REFERENCE_SHARE_PER_PIXEL, TWO_DECIMALS}},
    {"Shapes", "MeshResolution", {8.f, 64.f, INTEGER_DRAG_SPEED, INTEGER}},
    {"Shell", "Subdivisions", {1.f, 5.f, INTEGER_DRAG_SPEED, INTEGER}},
    {"Shapes", "CarveChance", {0.f, 1.f, REFERENCE_SHARE_PER_PIXEL, TWO_DECIMALS}},
    {"Physics", "Restitution", {0.f, 1.f, REFERENCE_SHARE_PER_PIXEL, TWO_DECIMALS}},
    {"Physics", "Substeps", {1.f, 8.f, INTEGER_DRAG_SPEED, INTEGER}},
}};

struct TabName
{
    TKit::StringView Field;
    TKit::StringView Title;
};

constexpr TKit::FixedArray<TabName, 3> TAB_NAMES{{
    {"LookFeel", "Look & Feel"},
    {"Mechanics", "Core Mechanics"},
    {"Flags", "Feature flags"},
}};

struct SectionWalk
{
    Onyx::Overlay &Ui;
    Onyx::Color DimText;
    u32 Count;
};

[[nodiscard]] Onyx::OverlayColors CreateColors(const Palette &palette)
{
    const Onyx::Color hovered = Mix(palette.UiSurface, palette.UiTextDim, HOVER_TINT);
    const Onyx::Color pressed = Mix(palette.UiSurface, palette.UiTextDim, PRESS_TINT);

    Onyx::OverlayPalette overlay{};
    overlay[Onyx::OverlayPalette_Idle0] = palette.UiSurface;
    overlay[Onyx::OverlayPalette_Idle1] = palette.UiBorder;
    overlay[Onyx::OverlayPalette_Idle2] = palette.UiBackground;

    overlay[Onyx::OverlayPalette_Hovered0] = hovered;
    overlay[Onyx::OverlayPalette_Hovered1] = Mix(palette.UiBorder, palette.UiTextDim, HOVER_TINT);
    overlay[Onyx::OverlayPalette_Hovered2] = hovered;
    overlay[Onyx::OverlayPalette_Hovered3] = hovered;

    overlay[Onyx::OverlayPalette_Pressed0] = pressed;
    overlay[Onyx::OverlayPalette_Pressed1] = pressed;
    overlay[Onyx::OverlayPalette_Pressed2] = pressed;

    overlay[Onyx::OverlayPalette_Text0] = palette.UiText;

    overlay[Onyx::OverlayPalette_Inner0] = palette.UiTextDim;
    overlay[Onyx::OverlayPalette_Inner1] = palette.UiText;

    overlay[Onyx::OverlayPalette_Background0] = palette.UiBackground;
    overlay[Onyx::OverlayPalette_Background1] = palette.UiBorder;
    overlay[Onyx::OverlayPalette_Background2] = palette.UiBackground;

    Onyx::OverlayColors colors = Onyx::CreateOverlayColorsFromPalette(overlay);
    colors[Onyx::OverlayColor_WindowBorderIdle] = palette.UiBorder;
    colors[Onyx::OverlayColor_WindowBorderHovered] = palette.UiBorder;
    colors[Onyx::OverlayColor_WindowBorderPressed] = palette.UiBorder;
    colors[Onyx::OverlayColor_ButtonIdle] = palette.UiBorder;
    colors[Onyx::OverlayColor_ButtonHovered] = Mix(palette.UiBorder, palette.UiTextDim, HOVER_TINT);
    colors[Onyx::OverlayColor_ButtonPressed] = Mix(palette.UiBorder, palette.UiTextDim, PRESS_TINT);
    return colors;
}

[[nodiscard]] Onyx::OverlayStyleVariables CreateVariables()
{
    Onyx::OverlayStyleVariables variables = Onyx::CreateDefaultOverlayVariables();
    variables[Onyx::OverlayStyle_WindowBorderWidth] = BORDER_WIDTH;
    variables[Onyx::OverlayStyle_LineWidth] = BORDER_WIDTH;
    variables[Onyx::OverlayStyle_WindowPadding] = WINDOW_PADDING;
    variables[Onyx::OverlayStyle_WidgetMinimumWidth] = WIDGET_WIDTH;
    return variables;
}

/// Writes the text colour directly, because PushStyleColor backs up the colour already scaled by the style alpha.
template <typename Widget> void Dimmed(Onyx::Overlay &ui, const Onyx::Color &dim, const Widget &widget)
{
    Onyx::OverlayColors &colors = ui.GetStyle().Colors;
    const Onyx::Color regular = colors[Onyx::OverlayColor_Text];
    colors[Onyx::OverlayColor_Text] = dim;
    widget();
    colors[Onyx::OverlayColor_Text] = regular;
}

void DrawHeader(Onyx::Overlay &ui, const Onyx::Color &dim, const bool won, const u64 address)
{
    ui.PushStyleVar(Onyx::OverlayStyle_FontSize, TITLE_SCALE * ui.GetStyle()[Onyx::OverlayStyle_FontSize]);
    ui.TextRaw(won ? WON_TITLE : DIED_TITLE);
    ui.PopStyleVar();

    const TKit::StaticString32 source = TKit::StaticString32::Format("SOURCE 0x{:016X}", address);
    Dimmed(ui, dim, [&] { ui.TextRaw(source); });
}

/// Upper-cases a PascalCase name and splits it into words: "WaveAmplitude" becomes "WAVE AMPLITUDE".
[[nodiscard]] Label ToLabel(const TKit::StringView name, const TKit::StringView prefix = {})
{
    const auto isUpper = [](const char letter) { return letter >= 'A' && letter <= 'Z'; };
    const auto isLower = [](const char letter) { return letter >= 'a' && letter <= 'z'; };

    Label label{};
    for (const char letter : prefix)
        label.Append(letter);
    for (usize i = 0; i < name.GetSize(); ++i)
    {
        const char letter = name[i];
        const bool afterLower = i > 0 && isLower(name[i - 1]);
        const bool endsAcronym = i > 0 && isUpper(name[i - 1]) && i + 1 < name.GetSize() && isLower(name[i + 1]);
        if (isUpper(letter) && (afterLower || endsAcronym))
            label.Append(' ');
        label.Append(isLower(letter) ? char(letter - 'a' + 'A') : letter);
    }
    return label;
}

/// An override when one matches, else a range scaled from the struct default so new fields need no table entry.
template <TKit::Numeric T>
[[nodiscard]] DragRange FindRange(const TKit::StringView section, const TKit::StringView field, const T defaultValue)
{
    for (const DragOverride &entry : DRAG_OVERRIDES)
        if (entry.Section == section && entry.Field == field)
            return entry.Range;

    const f32 magnitude = TKit::Math::Absolute(f32(defaultValue));
    const f32 reference = magnitude > 0.f ? magnitude : 1.f;
    const f32 limit = RANGE_SCALE * reference;
    const f32 low = f32(defaultValue) < 0.f ? -limit : 0.f;
    if constexpr (TKit::Integer<T>)
        return DragRange{low, limit, INTEGER_DRAG_SPEED, INTEGER};
    else
    {
        usize precision = 0;
        for (f32 bound = DECADE; reference < bound && precision + 1 < REAL_FORMATS.GetSize(); bound /= DECADE)
            ++precision;
        return DragRange{low, limit, REFERENCE_SHARE_PER_PIXEL * reference, REAL_FORMATS[precision]};
    }
}

void DrawLeaf(Onyx::Overlay &ui, const TKit::StringView, const TKit::StringView field, bool &enabled, const bool)
{
    ui.CheckBox(ToLabel(field), &enabled);
}

void DrawLeaf(Onyx::Overlay &ui, const TKit::StringView, const TKit::StringView field, Onyx::Color &color,
              const Onyx::Color &)
{
    ui.ColorEditor(ToLabel(field), &color, COLOR_FLAGS);
}

template <TKit::Numeric T>
void DrawLeaf(Onyx::Overlay &ui, const TKit::StringView section, const TKit::StringView field, T &value,
              const T defaultValue)
{
    const DragRange range = FindRange(section, field, defaultValue);
    ui.HorizontalDrag(ToLabel(field), &value, range.Speed, T(range.Min), T(range.Max), range.Format);
}

/// Draws the plain fields of a struct first, then each nested struct as a numbered section below them.
template <typename Params>
void DrawFields(SectionWalk &walk, Params &params, const Params &defaults, const TKit::StringView section)
{
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (!TKit::Reflect<Type>::Implemented)
            DrawLeaf(walk.Ui, section, field.Name, field.Get(params), field.Get(defaults));
    });
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
        {
            const Label title = ToLabel(field.Name, Label::Format("[{:02}] ", ++walk.Count));
            Dimmed(walk.Ui, walk.DimText, [&] { walk.Ui.HorizontalSeparator(title); });
            walk.Ui.PushId(field.Name);
            DrawFields(walk, field.Get(params), field.Get(defaults), field.Name);
            walk.Ui.PopId();
        }
    });
}

void DrawTabs(Onyx::Overlay &ui, const Onyx::Color &dim, LevelParams &params, const LevelParams &defaults,
              const f32 scrollHeight)
{
    ui.BeginTabBar(TAB_BAR_ID);
    TKit::Reflect<LevelParams>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        static_assert(TKit::Reflect<Type>::Implemented, "Every LevelParams member is a tab, so it must be a struct");

        TKit::StringView name = field.Name;
        for (const TabName &tab : TAB_NAMES)
            if (tab.Field == name)
                name = tab.Title;
        if (!ui.BeginTab(ToLabel(name), TAB_FLAGS))
            return;

        ui.BeginScroll(SCROLL_LABEL, scrollHeight);
        SectionWalk walk{.Ui = ui, .DimText = dim, .Count = 0};
        DrawFields(walk, field.Get(params), field.Get(defaults), field.Name);
        ui.EndScroll();
        ui.EndTab();
    });
    ui.EndTabBar();
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
      m_Variables(CreateVariables())
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

    ApplyTheme(params.LookFeel, screen[1], m_Size ? opacity : 0.f);
    const f32v2 size = m_Size ? *m_Size : f32v2{0.f};
    ui.SetNextWindowPosition(
        f32v2{TKit::Math::Round(0.5f * (screen[0] - size[0])), TKit::Math::Round(0.5f * (screen[1] - size[1]))});

    const Onyx::OverlayLabel label{WINDOW_LABEL};
    if (!ui.BeginWindow(label, WINDOW_FLAGS))
        return Action::None;
    if (const Onyx::LayoutElementQueryInfo *element = ui.GetActiveLayout()->QueryElement(label.Id))
        m_Size = element->Size;

    const Onyx::Color dim = params.LookFeel.Colors.UiTextDim;
    DrawHeader(ui, dim, won, address);
    ui.HorizontalLine();
    DrawTabs(ui, dim, params, m_Defaults, SCROLL_HEIGHT_SHARE * screen[1]);
    const Action action = DrawButtons(ui);
    ui.EndWindow();
    return action;
}

void SandboxScreen::ApplyTheme(const LookFeelParams &lookFeel, const f32 screenHeight, const f32 alpha)
{
    const f32 fontPixels = lookFeel.TextSize * screenHeight / REFERENCE_SCREEN_HEIGHT;
    const f32 scale = fontPixels / m_Variables[Onyx::OverlayStyle_FontSize];

    Onyx::OverlayStyle &style = m_Overlay->GetStyle();
    style.Colors = CreateColors(lookFeel.Colors);
    style.Variables = m_Variables;
    style.Variables[Onyx::OverlayStyle_FontSize] = fontPixels;
    for (const Onyx::OverlayStyleVariable variable : SCALED_VARIABLES)
        style.Variables[variable] *= scale;
    style.Variables[Onyx::OverlayStyle_Alpha] = TKit::Math::Clamp(alpha, 0.f, 1.f);

    m_Overlay->GetMainNativeWindow()->Context->Font(m_Font);
}
} // namespace Lim

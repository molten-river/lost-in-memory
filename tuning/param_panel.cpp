#include "tuning/param_panel.hpp"

#include "tkit/math/math.hpp"

namespace Lim
{
namespace
{
constexpr TKit::StringView WINDOW_LABEL = "lim-tuning-panel";
constexpr Onyx::OverlayWindowFlags WINDOW_FLAGS =
    Onyx::OverlayWindowFlag_NoMove | Onyx::OverlayWindowFlag_NoResize | Onyx::OverlayWindowFlag_AutoResize |
    Onyx::OverlayWindowFlag_NoHeaderBar | Onyx::OverlayWindowFlag_NoCollapse | Onyx::OverlayWindowFlag_NoScrollBar;
constexpr Onyx::OverlayColorFlags COLOR_FLAGS =
    Onyx::OverlayColorFlag_NoAlpha | Onyx::OverlayColorFlag_NoColorMarkers | Onyx::OverlayColorFlag_NoTooltip;

constexpr f32 TITLE_SCALE = 1.5f;
constexpr f32 REFERENCE_SCREEN_HEIGHT = 1080.f;
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

constexpr f32 SCROLL_HEIGHT_SHARE = 0.55f;

[[nodiscard]] Onyx::OverlayColors CreateColors(const UiTheme &theme)
{
    const Onyx::Color hovered = Mix(theme.Surface, theme.TextDim, HOVER_TINT);
    const Onyx::Color pressed = Mix(theme.Surface, theme.TextDim, PRESS_TINT);

    Onyx::OverlayPalette overlay{};
    overlay[Onyx::OverlayPalette_Idle0] = theme.Surface;
    overlay[Onyx::OverlayPalette_Idle1] = theme.Border;
    overlay[Onyx::OverlayPalette_Idle2] = theme.Background;

    overlay[Onyx::OverlayPalette_Hovered0] = hovered;
    overlay[Onyx::OverlayPalette_Hovered1] = Mix(theme.Border, theme.TextDim, HOVER_TINT);
    overlay[Onyx::OverlayPalette_Hovered2] = hovered;
    overlay[Onyx::OverlayPalette_Hovered3] = hovered;

    overlay[Onyx::OverlayPalette_Pressed0] = pressed;
    overlay[Onyx::OverlayPalette_Pressed1] = pressed;
    overlay[Onyx::OverlayPalette_Pressed2] = pressed;

    overlay[Onyx::OverlayPalette_Text0] = theme.Text;

    overlay[Onyx::OverlayPalette_Inner0] = theme.TextDim;
    overlay[Onyx::OverlayPalette_Inner1] = theme.Text;

    overlay[Onyx::OverlayPalette_Background0] = theme.Background;
    overlay[Onyx::OverlayPalette_Background1] = theme.Border;
    overlay[Onyx::OverlayPalette_Background2] = theme.Background;

    Onyx::OverlayColors colors = Onyx::CreateOverlayColorsFromPalette(overlay);
    colors[Onyx::OverlayColor_WindowBorderIdle] = theme.Border;
    colors[Onyx::OverlayColor_WindowBorderHovered] = theme.Border;
    colors[Onyx::OverlayColor_WindowBorderPressed] = theme.Border;
    colors[Onyx::OverlayColor_ButtonIdle] = theme.Border;
    colors[Onyx::OverlayColor_ButtonHovered] = Mix(theme.Border, theme.TextDim, HOVER_TINT);
    colors[Onyx::OverlayColor_ButtonPressed] = Mix(theme.Border, theme.TextDim, PRESS_TINT);
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
} // namespace

ParamPanel::ParamPanel(Onyx::Window &window, FontLibrary &fonts)
    : m_Font(fonts.Get()),
      m_Overlay(window.CreateOverlay(Onyx::OverlaySpecs{
          .Layout = {.RootAlignment = {Onyx::Alignment_Left, Onyx::Alignment_Top}, .Font = m_Font}})),
      m_Variables(CreateVariables())
{
}

bool ParamPanel::Begin(const UiTheme &theme, const f32 textSize, const f32 opacity)
{
    m_WindowOpen = false;
    if (opacity <= 0.f)
        return false;

    Onyx::Overlay &ui = *m_Overlay;
    const Onyx::NativeWindow &native = *ui.GetMainNativeWindow();
    const f32v2 screen{native.Window->GetScreenDimensions()};
    if (screen[1] <= 0.f)
        return false;

    ApplyTheme(theme, textSize, screen[1], m_Size ? opacity : 0.f);
    m_ScrollHeight = SCROLL_HEIGHT_SHARE * screen[1];

    const f32v2 size = m_Size ? *m_Size : f32v2{0.f};
    ui.SetNextWindowPosition(
        f32v2{TKit::Math::Round(0.5f * (screen[0] - size[0])), TKit::Math::Round(0.5f * (screen[1] - size[1]))});

    const Onyx::OverlayLabel label{WINDOW_LABEL};
    if (!ui.BeginWindow(label, WINDOW_FLAGS))
        return false;
    if (const Onyx::LayoutElementQueryInfo *element = ui.GetActiveLayout()->QueryElement(label.Id))
        m_Size = element->Size;

    m_WindowOpen = true;
    return true;
}

void ParamPanel::Header(const TKit::StringView title, const TKit::Span<const TKit::StringView> lines,
                        const TKit::StringView caption)
{
    Onyx::Overlay &ui = *m_Overlay;
    ui.PushStyleVar(Onyx::OverlayStyle_FontSize, TITLE_SCALE * ui.GetStyle()[Onyx::OverlayStyle_FontSize]);
    ui.TextRaw(title);
    ui.PopStyleVar();

    for (const TKit::StringView &line : lines)
        ui.TextRaw(line);

    if (!caption.IsEmpty())
        Dimmed(ui, m_DimText, [&] { ui.TextRaw(caption); });
    ui.HorizontalLine();
}

TKit::Optional<u32> ParamPanel::Buttons(const TKit::Span<const TKit::StringView> labels)
{
    Onyx::Overlay &ui = *m_Overlay;
    ui.BeginPanel(Onyx::Overlay::LyPnPar{.Direction = Onyx::LayoutDirection_LeftToRight,
                                         .Sizing = {Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Fit()},
                                         .ChildGap = ui.GetStyle()[Onyx::OverlayStyle_ChildGap]});
    TKit::Optional<u32> pressed{};
    for (u32 i = 0; i < labels.GetSize(); ++i)
        if (ui.Button(labels[i], Onyx::OverlayButtonFlag_SpanFullWidth))
            pressed = i;
    ui.EndPanel();
    return pressed;
}

void ParamPanel::End()
{
    if (m_WindowOpen)
        m_Overlay->EndWindow();
    m_Overlay->Draw();
}

void ParamPanel::ApplyTheme(const UiTheme &theme, const f32 textSize, const f32 screenHeight, const f32 alpha)
{
    const f32 fontPixels = textSize * screenHeight / REFERENCE_SCREEN_HEIGHT;
    const f32 scale = fontPixels / m_Variables[Onyx::OverlayStyle_FontSize];

    m_DimText = theme.TextDim;
    Onyx::OverlayStyle &style = m_Overlay->GetStyle();
    style.Colors = CreateColors(theme);
    style.Variables = m_Variables;
    style.Variables[Onyx::OverlayStyle_FontSize] = fontPixels;
    for (const Onyx::OverlayStyleVariable variable : SCALED_VARIABLES)
        style.Variables[variable] *= scale;
    style.Variables[Onyx::OverlayStyle_Alpha] = TKit::Math::Clamp(alpha, 0.f, 1.f);

    m_Overlay->GetMainNativeWindow()->Context->Font(m_Font);
}

ParamPanel::Label ParamPanel::ToLabel(const TKit::StringView name, const TKit::StringView prefix)
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

void ParamPanel::DrawLeaf(Onyx::Overlay &ui, const Label &label, bool &enabled)
{
    ui.CheckBox(label, &enabled);
}

void ParamPanel::DrawLeaf(Onyx::Overlay &ui, const Label &label, Onyx::Color &color)
{
    ui.ColorEditor(label, &color, COLOR_FLAGS);
}
} // namespace Lim

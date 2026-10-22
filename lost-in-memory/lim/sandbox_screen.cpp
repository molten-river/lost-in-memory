#include "lim/sandbox_screen.hpp"

#include "lim/palette.hpp"
#include "lim/param_schema.hpp"
#include "lim/params_reflection.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/math.hpp"

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
constexpr Onyx::OverlayColorFlags COLOR_FLAGS =
    Onyx::OverlayColorFlag_NoAlpha | Onyx::OverlayColorFlag_NoColorMarkers | Onyx::OverlayColorFlag_NoTooltip;
constexpr Onyx::OverlayFocusFlags HINT_FLAGS = Onyx::OverlayFocusFlag_ShortDelay;

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

constexpr TKit::FixedArray<const char *, 5> REAL_FORMATS{ONE_DECIMAL_FORMAT, TWO_DECIMALS_FORMAT, THREE_DECIMALS_FORMAT,
                                                         "{:.4f}", "{:.5f}"};
constexpr f32 DECADE = 10.f;

constexpr f32 REFERENCE_SHARE_PER_PIXEL = 0.005f;
constexpr f32 RANGE_SCALE = 4.f;

using ScopeFlags = u8;
enum ScopeFlagBit : ScopeFlags
{
    ScopeFlag_Main = 1U << u8(PlayMode::Main),
    ScopeFlag_Shapes = 1U << u8(PlayMode::TestShapes),
    ScopeFlag_Sphere = 1U << u8(PlayMode::TestSphere)
};

constexpr ScopeFlags MAIN_ONLY = ScopeFlag_Main;
constexpr ScopeFlags SHAPES_ONLY = ScopeFlag_Shapes;
constexpr ScopeFlags WITH_SHAPES = ScopeFlag_Main | ScopeFlag_Shapes;
constexpr ScopeFlags WITH_SPHERE = ScopeFlag_Main | ScopeFlag_Sphere;
constexpr ScopeFlags EVERY_MODE = ScopeFlag_Main | ScopeFlag_Shapes | ScopeFlag_Sphere;

/// A feature flag, looked up by name in the reflected FeatureFlags, that must hold the given value.
struct FlagCondition
{
    TKit::StringView Flag;
    bool Enabled;
};

constexpr TKit::Span<const FlagCondition> UNCONDITIONAL{};
constexpr TKit::FixedArray<FlagCondition, 1> DECODED{FlagCondition{"StructuralDecode", true}};
constexpr TKit::FixedArray<FlagCondition, 1> GRAMMAR{FlagCondition{"StructuralDecode", false}};
constexpr TKit::FixedArray<FlagCondition, 2> CARVED_GRAMMAR{FlagCondition{"StructuralDecode", false},
                                                            FlagCondition{"CarvedShapes", true}};
constexpr TKit::FixedArray<FlagCondition, 2> SMOOTH_GRAMMAR{FlagCondition{"StructuralDecode", false},
                                                            FlagCondition{"CarvedShapes", false}};
constexpr TKit::FixedArray<FlagCondition, 1> NOISE_SHELL{FlagCondition{"ShellVertexNoise", true}};
constexpr TKit::FixedArray<FlagCondition, 1> WAVE_SHELL{FlagCondition{"ShellVertexNoise", false}};

struct Visibility
{
    ScopeFlags Scope;
    TKit::Span<const FlagCondition> Conditions;
};

/// The default for every field of a section; sections not listed show everywhere, unconditionally.
struct SectionRule
{
    TKit::StringView Section;
    ScopeFlags Scope;
    TKit::Span<const FlagCondition> Conditions;
};

/// Replaces the section default for one field.
struct FieldRule
{
    TKit::StringView Section;
    TKit::StringView Field;
    ScopeFlags Scope;
    TKit::Span<const FlagCondition> Conditions;
};

constexpr TKit::FixedArray<SectionRule, 11> SECTION_RULES{{
    {"ShellMotion", WITH_SPHERE, NOISE_SHELL},
    {"Shapes", WITH_SHAPES, CARVED_GRAMMAR},
    {"Shell", WITH_SPHERE, UNCONDITIONAL},
    {"Enemies", MAIN_ONLY, UNCONDITIONAL},
    {"Solids", MAIN_ONLY, UNCONDITIONAL},
    {"Goal", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", WITH_SHAPES, UNCONDITIONAL},
    {"Records", WITH_SHAPES, DECODED},
    {"Graph", WITH_SHAPES, DECODED},
    {"Evolution", WITH_SHAPES, DECODED},
    {"TestBench", SHAPES_ONLY, UNCONDITIONAL},
}};

constexpr TKit::FixedArray<FieldRule, 44> FIELD_RULES{{
    {"Colors", "ShellTrough", WITH_SPHERE, UNCONDITIONAL},
    {"Colors", "ShellCrest", WITH_SPHERE, UNCONDITIONAL},
    {"Colors", "SolidLow", WITH_SHAPES, UNCONDITIONAL},
    {"Colors", "SolidHigh", WITH_SHAPES, UNCONDITIONAL},
    {"ShellMotion", "WaveAmplitude", WITH_SPHERE, WAVE_SHELL},
    {"ShellMotion", "WaveSpeed", WITH_SPHERE, WAVE_SHELL},
    {"ShellMotion", "WaveFrequency", WITH_SPHERE, WAVE_SHELL},
    {"Shapes", "MeshResolution", WITH_SHAPES, GRAMMAR},
    {"Shapes", "SmoothSpread", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothSphereRadius", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothBoxHalfWidth", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothBoxHalfHeight", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothBoxHalfDepth", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothCapsuleRadius", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothCapsuleHalfLength", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothTorusMajorRadius", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothTorusMinorRadius", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Enemies", "MinCount", MAIN_ONLY, GRAMMAR},
    {"Enemies", "MaxCount", MAIN_ONLY, GRAMMAR},
    {"Enemies", "MinSize", WITH_SHAPES, GRAMMAR},
    {"Enemies", "MaxSize", WITH_SHAPES, GRAMMAR},
    {"Enemies", "MinPrimitives", WITH_SHAPES, GRAMMAR},
    {"Enemies", "MaxPrimitives", WITH_SHAPES, GRAMMAR},
    {"Enemies", "BlendRadius", WITH_SHAPES, GRAMMAR},
    {"Solids", "MinCount", MAIN_ONLY, GRAMMAR},
    {"Solids", "MaxCount", MAIN_ONLY, GRAMMAR},
    {"Solids", "MinSize", WITH_SHAPES, GRAMMAR},
    {"Solids", "MaxSize", WITH_SHAPES, GRAMMAR},
    {"Solids", "MinPrimitives", WITH_SHAPES, GRAMMAR},
    {"Solids", "MaxPrimitives", WITH_SHAPES, GRAMMAR},
    {"Solids", "BlendRadius", WITH_SHAPES, GRAMMAR},
    {"Goal", "ShellChance", MAIN_ONLY, GRAMMAR},
    {"Goal", "Size", WITH_SHAPES, GRAMMAR},
    {"Goal", "MinPrimitives", WITH_SHAPES, GRAMMAR},
    {"Goal", "MaxPrimitives", WITH_SHAPES, GRAMMAR},
    {"Physics", "WallMargin", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "MinTumbleScale", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "MaxTumbleScale", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "MinSafeSpawnRadius", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "SafeSpawnFraction", MAIN_ONLY, UNCONDITIONAL},
    {"Flags", "ShellVertexNoise", WITH_SPHERE, UNCONDITIONAL},
    {"Flags", "CarvedShapes", WITH_SHAPES, GRAMMAR},
    {"Flags", "StructuralDecode", WITH_SHAPES, UNCONDITIONAL},
    {"Flags", "ChainLinks", WITH_SHAPES, DECODED},
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

struct Heading
{
    SandboxScreen::Title Title;
    TKit::StringView Text;
};

constexpr TKit::FixedArray<Heading, 3> HEADINGS{{
    {SandboxScreen::Title::Cleared, "REGION CLEARED"},
    {SandboxScreen::Title::Terminated, "PROCESS TERMINATED"},
    {SandboxScreen::Title::Interrupted, "PROCESS INTERRUPTED"},
}};

struct ModeName
{
    PlayMode Mode;
    TKit::StringView Text;
};

constexpr TKit::FixedArray<ModeName, 2> MODE_NAMES{{
    {PlayMode::TestShapes, "TEST SHAPES"},
    {PlayMode::TestSphere, "TEST SPHERE"},
}};

/// Whether a reflected leaf named field sits in a struct held by a member named section.
template <typename Params>
[[nodiscard]] constexpr bool IsLeafReflected(const TKit::StringView section, const TKit::StringView field,
                                             const TKit::StringView current)
{
    bool found = false;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &member) {
        using Type = TKIT_REFLECT_FIELD_TYPE(member);
        if constexpr (TKit::Reflect<Type>::Implemented)
            found = found || IsLeafReflected<Type>(section, field, member.Name);
        else
            found = found || (current == section && field == TKit::StringView{member.Name});
    });
    return found;
}

template <typename Params> [[nodiscard]] constexpr bool IsSectionReflected(const TKit::StringView section)
{
    bool found = false;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &member) {
        using Type = TKIT_REFLECT_FIELD_TYPE(member);
        if constexpr (TKit::Reflect<Type>::Implemented)
            found = found || section == TKit::StringView{member.Name} || IsSectionReflected<Type>(section);
    });
    return found;
}

[[nodiscard]] constexpr bool AreFieldRulesKnown()
{
    for (const FieldRule &rule : FIELD_RULES)
        if (!IsLeafReflected<LevelParams>(rule.Section, rule.Field, TKit::StringView{}))
            return false;
    return true;
}

[[nodiscard]] constexpr bool AreRulesKnown()
{
    const auto areFlagsKnown = [](const TKit::Span<const FlagCondition> conditions) {
        for (const FlagCondition &condition : conditions)
            if (!IsLeafReflected<FeatureFlags>(TKit::StringView{}, condition.Flag, TKit::StringView{}))
                return false;
        return true;
    };

    for (const SectionRule &rule : SECTION_RULES)
        if (!IsSectionReflected<LevelParams>(rule.Section) || !areFlagsKnown(rule.Conditions))
            return false;
    for (const FieldRule &rule : FIELD_RULES)
        if (!areFlagsKnown(rule.Conditions))
            return false;
    return true;
}

static_assert(AreFieldRulesKnown(), "Every FIELD_RULES entry must name a tuning field");
static_assert(AreRulesKnown(), "Every rule must name a tuning section and existing feature flags");

struct SectionWalk
{
    Onyx::Overlay &Ui;
    Onyx::Color DimText;
    ScopeFlags Scope;
    const FeatureFlags &Flags;
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

void DrawHeader(Onyx::Overlay &ui, const Onyx::Color &dim, const PlayMode mode, const SandboxScreen::Title title,
                const u64 address)
{
    ui.PushStyleVar(Onyx::OverlayStyle_FontSize, TITLE_SCALE * ui.GetStyle()[Onyx::OverlayStyle_FontSize]);
    for (const Heading &heading : HEADINGS)
        if (heading.Title == title)
            ui.TextRaw(heading.Text);
    ui.PopStyleVar();

    for (const ModeName &name : MODE_NAMES)
        if (name.Mode == mode)
            ui.TextRaw(name.Text);

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

/// The field's own rule when one matches, else its section's, else visible everywhere.
[[nodiscard]] Visibility FindVisibility(const TKit::StringView section, const TKit::StringView field)
{
    for (const FieldRule &rule : FIELD_RULES)
        if (rule.Section == section && rule.Field == field)
            return Visibility{.Scope = rule.Scope, .Conditions = rule.Conditions};
    for (const SectionRule &rule : SECTION_RULES)
        if (rule.Section == section)
            return Visibility{.Scope = rule.Scope, .Conditions = rule.Conditions};
    return Visibility{.Scope = EVERY_MODE, .Conditions = UNCONDITIONAL};
}

[[nodiscard]] bool IsVisible(const SectionWalk &walk, const TKit::StringView section, const TKit::StringView field)
{
    const Visibility visibility = FindVisibility(section, field);
    if ((visibility.Scope & walk.Scope) == 0)
        return false;
    for (const FlagCondition &condition : visibility.Conditions)
    {
        const auto flag = TKit::Reflect<FeatureFlags>::GetMemberField<bool>(condition.Flag);
        if (!flag || flag.Get(walk.Flags) != condition.Enabled)
            return false;
    }
    return true;
}

template <typename Params> [[nodiscard]] bool HasVisibleField(const SectionWalk &walk, const TKit::StringView section)
{
    bool visible = false;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
            visible = visible || HasVisibleField<Type>(walk, field.Name);
        else
            visible = visible || IsVisible(walk, section, field.Name);
    });
    return visible;
}

void DrawLeaf(Onyx::Overlay &ui, const Label &label, bool &enabled)
{
    ui.CheckBox(label, &enabled);
}

void DrawLeaf(Onyx::Overlay &ui, const Label &label, Onyx::Color &color)
{
    ui.ColorEditor(label, &color, COLOR_FLAGS);
}

/// Bounds and drag style come from the spec; a side it leaves open is scaled from the struct default.
template <typename Params, TKit::Numeric T>
void DrawLeaf(Onyx::Overlay &ui, const Label &label, T &value, const T defaultValue, const ParamSpec<Params> &spec)
{
    const f32 magnitude = TKit::Math::Absolute(f32(defaultValue));
    const f32 reference = magnitude > 0.f ? magnitude : 1.f;
    const f32 low = (spec.Flags & ParamSpecFlag_Min) != 0 ? spec.Min
                    : f32(defaultValue) < 0.f             ? -RANGE_SCALE * reference
                                                          : 0.f;
    const f32 high = (spec.Flags & ParamSpecFlag_Max) != 0 ? spec.Max : TKit::Math::Max(low, RANGE_SCALE * reference);

    DragStyle drag = spec.Drag;
    if ((spec.Flags & ParamSpecFlag_Drag) == 0)
    {
        if constexpr (TKit::Integer<T>)
            drag = FINE_INTEGER;
        else
        {
            usize precision = 0;
            for (f32 bound = DECADE; reference < bound && precision + 1 < REAL_FORMATS.GetSize(); bound /= DECADE)
                ++precision;
            drag = DragStyle{REFERENCE_SHARE_PER_PIXEL * reference, REAL_FORMATS[precision]};
        }
    }
    ui.HorizontalDrag(label, &value, drag.Speed, T(low), T(high), drag.Format);
}

/// Draws the visible plain fields first, then each nested struct holding a visible field as a numbered section.
template <typename Params>
void DrawFields(SectionWalk &walk, Params &params, const Params &defaults, const TKit::StringView section)
{
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (!TKit::Reflect<Type>::Implemented)
        {
            if (!IsVisible(walk, section, field.Name))
                return;
            const ParamSpec<Params> &spec = FindSpec(field.Pointer);
            if constexpr (TKit::Numeric<Type>)
                DrawLeaf(walk.Ui, ToLabel(field.Name), field.Get(params), field.Get(defaults), spec);
            else
                DrawLeaf(walk.Ui, ToLabel(field.Name), field.Get(params));
            walk.Ui.SetItemTooltipRaw(spec.Hint, HINT_FLAGS);
        }
    });
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
        {
            if (!HasVisibleField<Type>(walk, field.Name))
                return;
            const Label title = ToLabel(field.Name, Label::Format("[{:02}] ", ++walk.Count));
            Dimmed(walk.Ui, walk.DimText, [&] { walk.Ui.HorizontalSeparator(title); });
            walk.Ui.PushId(field.Name);
            DrawFields(walk, field.Get(params), field.Get(defaults), field.Name);
            walk.Ui.PopId();
        }
    });
}

void DrawTabs(SectionWalk &walk, LevelParams &params, const LevelParams &defaults, const f32 scrollHeight)
{
    Onyx::Overlay &ui = walk.Ui;
    ui.BeginTabBar(TAB_BAR_ID);
    TKit::Reflect<LevelParams>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        static_assert(TKit::Reflect<Type>::Implemented, "Every LevelParams member is a tab, so it must be a struct");
        if (!HasVisibleField<Type>(walk, field.Name))
            return;

        TKit::StringView name = field.Name;
        for (const TabName &tab : TAB_NAMES)
            if (tab.Field == name)
                name = tab.Title;
        if (!ui.BeginTab(ToLabel(name), TAB_FLAGS))
            return;

        ui.BeginScroll(SCROLL_LABEL, scrollHeight);
        walk.Count = 0;
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

SandboxScreen::Action SandboxScreen::Draw(LevelParams &params, const PlayMode mode, const Title title,
                                          const u64 address, const f32 opacity)
{
    const Action action = opacity > 0.f ? DrawPanel(params, mode, title, address, opacity) : Action::None;
    m_Overlay->Draw();
    return action;
}

SandboxScreen::Action SandboxScreen::DrawPanel(LevelParams &params, const PlayMode mode, const Title title,
                                               const u64 address, const f32 opacity)
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
    DrawHeader(ui, dim, mode, title, address);
    ui.HorizontalLine();
    SectionWalk walk{.Ui = ui, .DimText = dim, .Scope = ScopeFlags(1U << u8(mode)), .Flags = params.Flags, .Count = 0};
    DrawTabs(walk, params, m_Defaults, SCROLL_HEIGHT_SHARE * screen[1]);
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

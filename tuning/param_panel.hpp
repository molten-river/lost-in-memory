#pragma once

#include "alias/alias.hpp"
#include "look/font_library.hpp"
#include "look/ui_theme.hpp"
#include "tuning/param_schema.hpp"
#include "tuning/visibility_strategy.hpp"

#include "onyx/handle.hpp"
#include "onyx/overlay.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
/// One tab of the panel: a top-level reflected member of the root params struct, with its display title.
struct PanelTab
{
    TKit::StringView Field;
    TKit::StringView Title;
};

/// Centred, schema-driven overlay panel: a header, themed tabs of numbered sections, tooltips and a button row.
class ParamPanel
{
  public:
    ParamPanel(Onyx::Window &window, FontLibrary &fonts);

    /// False-y work when opacity is zero or the window is offscreen; call End regardless, which still flushes.
    [[nodiscard]] bool Begin(const UiTheme &theme, f32 textSize, f32 opacity);
    void Header(TKit::StringView title, TKit::Span<const TKit::StringView> lines, TKit::StringView caption);
    [[nodiscard]] bool CornerButton(TKit::StringView label);
    template <typename Root>
    void Tabs(Root &params, const Root &defaults, const VisibilityStrategy &visibility,
              TKit::Span<const PanelTab> tabs);
    [[nodiscard]] TKit::Optional<u32> Buttons(TKit::Span<const TKit::StringView> labels);
    void End();

  private:
    static constexpr usize LABEL_CAPACITY = 48;
    using Label = TKit::StaticString<LABEL_CAPACITY>;

    struct SectionWalk
    {
        Onyx::Overlay &Ui;
        Onyx::Color DimText;
        const VisibilityStrategy &Visibility;
        u32 Count;
    };

    void ApplyTheme(const UiTheme &theme, f32 textSize, f32 screenHeight, f32 alpha);

    [[nodiscard]] static Label ToLabel(TKit::StringView name, TKit::StringView prefix = {});
    static void DrawLeaf(Onyx::Overlay &ui, const Label &label, bool &enabled);
    static void DrawLeaf(Onyx::Overlay &ui, const Label &label, Onyx::Color &color);

    template <typename Widget> static void Dimmed(Onyx::Overlay &ui, const Onyx::Color &dim, const Widget &widget);
    template <typename Params, TKit::Numeric T>
    static void DrawLeaf(Onyx::Overlay &ui, const Label &label, T &value, T defaultValue,
                         const ParamSpec<Params> &spec);
    template <typename Params>
    [[nodiscard]] static bool HasVisibleField(const SectionWalk &walk, TKit::StringView section);
    template <typename Params>
    static void DrawFields(SectionWalk &walk, Params &params, const Params &defaults, TKit::StringView section);
    template <typename Root>
    void DrawTab(Onyx::Overlay &ui, Root &params, const Root &defaults, const VisibilityStrategy &visibility,
                 const PanelTab &tab);

    Onyx::Resource m_Font;
    Onyx::Overlay *m_Overlay;
    Onyx::OverlayStyleVariables m_Variables;
    Onyx::Color m_DimText{};
    TKit::Optional<f32v2> m_Size{};
    f32 m_ScrollHeight = 0.f;
    bool m_WindowOpen = false;
};

/// Writes the text colour directly, because PushStyleColor backs up the colour already scaled by the style alpha.
template <typename Widget> void ParamPanel::Dimmed(Onyx::Overlay &ui, const Onyx::Color &dim, const Widget &widget)
{
    Onyx::OverlayColors &colors = ui.GetStyle().Colors;
    const Onyx::Color regular = colors[Onyx::OverlayColor_Text];
    colors[Onyx::OverlayColor_Text] = dim;
    widget();
    colors[Onyx::OverlayColor_Text] = regular;
}

/// Bounds and drag style come from the spec; a side it leaves open is scaled from the struct default.
template <typename Params, TKit::Numeric T>
void ParamPanel::DrawLeaf(Onyx::Overlay &ui, const Label &label, T &value, const T defaultValue,
                          const ParamSpec<Params> &spec)
{
    constexpr f32 DECADE = 10.f;
    constexpr f32 REFERENCE_SHARE_PER_PIXEL = 0.005f;
    constexpr f32 RANGE_SCALE = 4.f;
    static constexpr TKit::FixedArray<const char *, 5> REAL_FORMATS{ONE_DECIMAL_FORMAT, TWO_DECIMALS_FORMAT,
                                                                    THREE_DECIMALS_FORMAT, "{:.4f}", "{:.5f}"};

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

template <typename Params> bool ParamPanel::HasVisibleField(const SectionWalk &walk, const TKit::StringView section)
{
    bool visible = false;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
            visible = visible || HasVisibleField<Type>(walk, field.Name);
        else
            visible = visible || walk.Visibility.IsVisible(section, field.Name);
    });
    return visible;
}

/// Draws the visible plain fields first, then each nested struct holding a visible field as a numbered section.
template <typename Params>
void ParamPanel::DrawFields(SectionWalk &walk, Params &params, const Params &defaults, const TKit::StringView section)
{
    constexpr Onyx::OverlayFocusFlags HINT_FLAGS = Onyx::OverlayFocusFlag_ShortDelay;

    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (!TKit::Reflect<Type>::Implemented)
        {
            if (!walk.Visibility.IsVisible(section, field.Name))
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

template <typename Root>
void ParamPanel::DrawTab(Onyx::Overlay &ui, Root &params, const Root &defaults, const VisibilityStrategy &visibility,
                         const PanelTab &tab)
{
    constexpr TKit::StringView TAB_BAR_SCROLL_LABEL = "lim-tuning-scroll";
    constexpr Onyx::OverlayTabFlags TAB_FLAGS = Onyx::OverlayTabFlag_StartOpen;

    TKit::Reflect<Root>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if (TKit::StringView{field.Name} != tab.Field)
            return;
        if constexpr (TKit::Reflect<Type>::Implemented)
        {
            SectionWalk walk{.Ui = ui, .DimText = m_DimText, .Visibility = visibility, .Count = 0};
            if (!HasVisibleField<Type>(walk, field.Name) || !ui.BeginTab(ToLabel(tab.Title), TAB_FLAGS))
                return;
            ui.BeginScroll(TAB_BAR_SCROLL_LABEL, m_ScrollHeight);
            DrawFields(walk, field.Get(params), field.Get(defaults), field.Name);
            ui.EndScroll();
            ui.EndTab();
        }
    });
}

template <typename Root>
void ParamPanel::Tabs(Root &params, const Root &defaults, const VisibilityStrategy &visibility,
                      const TKit::Span<const PanelTab> tabs)
{
    constexpr TKit::StringView TAB_BAR_ID = "lim-tuning-tabs";

    Onyx::Overlay &ui = *m_Overlay;
    ui.BeginTabBar(TAB_BAR_ID);
    for (const PanelTab &tab : tabs)
        DrawTab(ui, params, defaults, visibility, tab);
    ui.EndTabBar();
}
} // namespace Lim

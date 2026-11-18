#include "lim/sandbox_screen.hpp"

#include "lim/game_visibility.hpp"
#include "lim/palette.hpp"
#include "lim/params_reflection.hpp"
#include "tuning/param_schema.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
namespace
{
constexpr TKit::FixedArray<PanelTab, 3> TABS{{
    {"LookFeel", "Look & Feel"},
    {"Mechanics", "Core Mechanics"},
    {"Flags", "Feature flags"},
}};
constexpr TKit::FixedArray<PanelTab, 1> KIT_TABS{PanelTab{"WalkMass", "Shape kit"}};

constexpr TKit::FixedArray<TKit::StringView, 2> BUTTON_LABELS{"SAME BYTES", "NEW REGION"};

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

/// Whether a reflected top-level member of Params is itself a struct, named name.
template <typename Params> [[nodiscard]] constexpr bool IsStructMember(const TKit::StringView name)
{
    bool found = false;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (ReflectedStruct<Type>)
            found = found || TKit::StringView{field.Name} == name;
    });
    return found;
}

template <typename Root> [[nodiscard]] constexpr bool AreTabsKnown(const TKit::Span<const PanelTab> tabs)
{
    for (const PanelTab &tab : tabs)
        if (!IsStructMember<Root>(tab.Field))
            return false;
    return true;
}

static_assert(AreTabsKnown<LevelParams>(TABS) && AreTabsKnown<ShapeKitParams>(KIT_TABS),
              "Every tab must name a struct member of its root");

[[nodiscard]] TKit::StringView HeadingText(const SandboxScreen::Title title)
{
    for (const Heading &heading : HEADINGS)
        if (heading.Title == title)
            return heading.Text;
    return {};
}

/// Writes the mode's display name into storage and returns a span over it, or an empty span for an unnamed mode.
[[nodiscard]] TKit::Span<const TKit::StringView> ModeLines(const PlayMode mode, TKit::StringView &storage)
{
    for (const ModeName &name : MODE_NAMES)
        if (name.Mode == mode)
        {
            storage = name.Text;
            return TKit::Span<const TKit::StringView>(&storage, 1);
        }
    return TKit::Span<const TKit::StringView>();
}
} // namespace

SandboxScreen::SandboxScreen(Onyx::Window &window, FontLibrary &fonts) : m_Panel(window, fonts)
{
}

SandboxScreen::Action SandboxScreen::Draw(LevelParams &params, ShapeKitParams &kit, const PlayMode mode,
                                          const Title title, const u64 address, const f32 opacity)
{
    const UiTheme theme = ToUiTheme(params.LookFeel.Colors);
    const Action action = m_Panel.Begin(theme, opacity) ? DrawPanel(params, kit, mode, title, address) : Action::None;
    m_Panel.End();
    return action;
}

SandboxScreen::Action SandboxScreen::DrawPanel(LevelParams &params, ShapeKitParams &kit, const PlayMode mode,
                                               const Title title, const u64 address)
{
    TKit::StringView lineStorage{};
    const TKit::Span<const TKit::StringView> lines = ModeLines(mode, lineStorage);
    const TKit::StaticString32 source = TKit::StaticString32::Format("SOURCE 0x{:016X}", address);
    m_Panel.Header(HeadingText(title), lines, source);
    const bool reset = m_Panel.CornerButton("RESET");

    const GameVisibility visibility{mode, params.Flags};
    m_Panel.BeginTabs();
    m_Panel.Tabs(params, m_Defaults, visibility, TABS);
    m_Panel.Tabs(kit, m_KitDefaults, visibility, KIT_TABS);
    m_Panel.EndTabs();

    const TKit::Optional<u32> pressed = m_Panel.Buttons(BUTTON_LABELS);
    if (reset)
        return Action::Reset;
    if (!pressed)
        return Action::None;
    return *pressed == 0 ? Action::SameBytes : Action::NewRegion;
}
} // namespace Lim

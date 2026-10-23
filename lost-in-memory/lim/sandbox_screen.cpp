#include "lim/sandbox_screen.hpp"

#include "lim/game_visibility.hpp"
#include "lim/palette.hpp"
#include "lim/params_reflection.hpp"

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
        if constexpr (TKit::Reflect<Type>::Implemented)
            found = found || TKit::StringView{field.Name} == name;
    });
    return found;
}

[[nodiscard]] constexpr bool AreTabsKnown()
{
    for (const PanelTab &tab : TABS)
        if (!IsStructMember<LevelParams>(tab.Field))
            return false;
    return true;
}

static_assert(AreTabsKnown(), "Every tab must name a struct member of LevelParams");

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

SandboxScreen::Action SandboxScreen::Draw(LevelParams &params, const PlayMode mode, const Title title,
                                          const u64 address, const f32 opacity)
{
    const UiTheme theme = ToUiTheme(params.LookFeel.Colors);
    const Action action = m_Panel.Begin(theme, params.LookFeel.TextSize, opacity)
                              ? DrawPanel(params, mode, title, address)
                              : Action::None;
    m_Panel.End();
    return action;
}

SandboxScreen::Action SandboxScreen::DrawPanel(LevelParams &params, const PlayMode mode, const Title title,
                                               const u64 address)
{
    TKit::StringView lineStorage{};
    const TKit::Span<const TKit::StringView> lines = ModeLines(mode, lineStorage);
    const TKit::StaticString32 source = TKit::StaticString32::Format("SOURCE 0x{:016X}", address);
    m_Panel.Header(HeadingText(title), lines, source);

    const GameVisibility visibility{mode, params.Flags};
    m_Panel.BeginTabs();
    m_Panel.Tabs(params, m_Defaults, visibility, TKit::Span<const PanelTab>(TABS.GetData(), TABS.GetSize()));
    m_Panel.EndTabs();

    const TKit::Optional<u32> pressed =
        m_Panel.Buttons(TKit::Span<const TKit::StringView>(BUTTON_LABELS.GetData(), BUTTON_LABELS.GetSize()));
    if (!pressed)
        return Action::None;
    return *pressed == 0 ? Action::SameBytes : Action::NewRegion;
}
} // namespace Lim

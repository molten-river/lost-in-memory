#include "lim/game_visibility.hpp"

#include "lim/params_reflection.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/reflection/reflect.hpp"

namespace Lim
{
namespace
{
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

constexpr TKit::FixedArray<SectionRule, 9> SECTION_RULES{{
    {"ShellMotion", WITH_SPHERE, NOISE_SHELL},
    {"Shell", WITH_SPHERE, UNCONDITIONAL},
    {"Enemies", MAIN_ONLY, UNCONDITIONAL},
    {"Solids", MAIN_ONLY, UNCONDITIONAL},
    {"Goal", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", WITH_SHAPES, UNCONDITIONAL},
    {"Records", WITH_SHAPES, UNCONDITIONAL},
    {"TestBench", SHAPES_ONLY, UNCONDITIONAL},
    {"WalkMass", WITH_SHAPES, UNCONDITIONAL},
}};

constexpr TKit::FixedArray<FieldRule, 14> FIELD_RULES{{
    {"Colors", "ShellTrough", WITH_SPHERE, UNCONDITIONAL},
    {"Colors", "ShellCrest", WITH_SPHERE, UNCONDITIONAL},
    {"Colors", "SolidLow", WITH_SHAPES, UNCONDITIONAL},
    {"Colors", "SolidHigh", WITH_SHAPES, UNCONDITIONAL},
    {"ShellMotion", "WaveAmplitude", WITH_SPHERE, WAVE_SHELL},
    {"ShellMotion", "WaveSpeed", WITH_SPHERE, WAVE_SHELL},
    {"ShellMotion", "WaveFrequency", WITH_SPHERE, WAVE_SHELL},
    {"Physics", "WallMargin", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "MinTumbleScale", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "MaxTumbleScale", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "MinSafeSpawnRadius", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "SafeSpawnFraction", MAIN_ONLY, UNCONDITIONAL},
    {"Records", "MaxFormRadiusShare", MAIN_ONLY, UNCONDITIONAL},
    {"Flags", "ShellVertexNoise", WITH_SPHERE, UNCONDITIONAL},
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
        if (!(IsSectionReflected<LevelParams>(rule.Section) || IsSectionReflected<ShapeKitParams>(rule.Section)) ||
            !areFlagsKnown(rule.Conditions))
            return false;
    for (const FieldRule &rule : FIELD_RULES)
        if (!areFlagsKnown(rule.Conditions))
            return false;
    return true;
}

static_assert(AreFieldRulesKnown(), "Every FIELD_RULES entry must name a tuning field");
static_assert(AreRulesKnown(), "Every rule must name a tuning section and existing feature flags");

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
} // namespace

GameVisibility::GameVisibility(const PlayMode mode, const FeatureFlags &flags) : m_Mode(mode), m_Flags(flags)
{
}

bool GameVisibility::IsVisible(const TKit::StringView section, const TKit::StringView field) const
{
    const Visibility visibility = FindVisibility(section, field);
    if ((visibility.Scope & ScopeFlags(1U << u8(m_Mode))) == 0)
        return false;
    for (const FlagCondition &condition : visibility.Conditions)
    {
        const auto flag = TKit::Reflect<FeatureFlags>::GetMemberField<bool>(condition.Flag);
        if (!flag || flag.Get(m_Flags) != condition.Enabled)
            return false;
    }
    return true;
}
} // namespace Lim

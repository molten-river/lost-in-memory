#include "lim/level_params.hpp"
#include "lim/params_reflection.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/container/tier_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/utils/logging.hpp"

#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <type_traits>
#include <utility>
#include <variant>

namespace Lim
{
namespace
{
enum class AssignResult : u8
{
    Unknown,
    Assigned,
    Invalid
};

constexpr usize INDENT_WIDTH = 2;
constexpr usize NUMBER_BUFFER_SIZE = 32;
constexpr usize MAX_NESTING_DEPTH = 4;
constexpr usize COLOR_HEX_DIGITS = 6;
constexpr f32 MAX_WAVE_AMPLITUDE_FRACTION = 0.4f;
constexpr f32 MAX_FORM_SIZE_FRACTION = 0.25f;
constexpr char COMMENT_MARKER = '#';
constexpr char KEY_VALUE_SEPARATOR = ':';
constexpr char LINE_BREAK = '\n';
constexpr const char *TEMP_SUFFIX = ".tmp";

template <typename Number> void AppendNumber(TKit::TierString &out, const Number number)
{
    TKit::FixedArray<char, NUMBER_BUFFER_SIZE> buffer;
    const std::to_chars_result result =
        std::to_chars(buffer.GetData(), buffer.GetData() + NUMBER_BUFFER_SIZE - 1, number);
    *result.ptr = 0;
    out += buffer.GetData();
}

void AppendBool(TKit::TierString &out, const bool value)
{
    out += value ? "true" : "false";
}

void AppendColor(TKit::TierString &out, const Onyx::Color &color)
{
    const TKit::StaticString8 hex = color.ToHexadecimal<TKit::StaticString8>(false);
    out += '#';
    out += hex.CString();
}

template <typename Params> void AppendFields(TKit::TierString &out, const Params &params, const usize depth)
{
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        for (usize i = 0; i < depth * INDENT_WIDTH; ++i)
            out += ' ';
        out += field.Name;
        out += KEY_VALUE_SEPARATOR;
        if constexpr (TKit::Reflect<Type>::Implemented)
        {
            out += LINE_BREAK;
            AppendFields(out, field.Get(params), depth + 1);
        }
        else
        {
            out += ' ';
            if constexpr (std::is_same_v<Type, bool>)
                AppendBool(out, field.Get(params));
            else if constexpr (std::is_same_v<Type, Onyx::Color>)
                AppendColor(out, field.Get(params));
            else
                AppendNumber(out, field.Get(params));
            out += LINE_BREAK;
        }
    });
}

template <typename Number> bool ParseNumber(Number &target, const TKit::StringView text)
{
    const char *end = text.GetData() + text.GetSize();
    Number parsed{};
    const std::from_chars_result result = std::from_chars(text.GetData(), end, parsed);
    if (result.ec != std::errc{} || result.ptr != end)
        return false;
    if constexpr (std::is_floating_point_v<Number>)
        if (!std::isfinite(parsed))
            return false;
    target = parsed;
    return true;
}

[[nodiscard]] bool ParseBool(bool &target, const TKit::StringView text)
{
    if (text == "true")
    {
        target = true;
        return true;
    }
    if (text == "false")
    {
        target = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool IsHexDigit(const char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

[[nodiscard]] bool ParseColor(Onyx::Color &target, const TKit::StringView text)
{
    if (text.GetSize() != COLOR_HEX_DIGITS + 1 || text.GetFront() != '#')
        return false;
    const TKit::StringView digits = text.SubString(1);
    for (const char c : digits)
        if (!IsHexDigit(c))
            return false;
    target = Onyx::Color::FromHexadecimal(digits);
    return true;
}

template <typename Params>
AssignResult AssignPath(Params &params, const TKit::Span<const TKit::StringView> path, const TKit::StringView text)
{
    AssignResult outcome = AssignResult::Unknown;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        if (path.GetFront() != field.Name)
            return;
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
        {
            if (path.GetSize() > 1)
                outcome = AssignPath(field.Get(params),
                                     TKit::Span<const TKit::StringView>(path.GetData() + 1, path.GetSize() - 1), text);
        }
        else if (path.GetSize() == 1)
        {
            if constexpr (std::is_same_v<Type, bool>)
                outcome = ParseBool(field.Get(params), text) ? AssignResult::Assigned : AssignResult::Invalid;
            else if constexpr (std::is_same_v<Type, Onyx::Color>)
                outcome = ParseColor(field.Get(params), text) ? AssignResult::Assigned : AssignResult::Invalid;
            else
                outcome = ParseNumber(field.Get(params), text) ? AssignResult::Assigned : AssignResult::Invalid;
        }
    });
    return outcome;
}

/// Tracks the enclosing section name at each indentation depth, as set by the last header line seen at that depth.
void ApplyLine(LevelParams &params, TKit::FixedArray<TKit::StringView, MAX_NESTING_DEPTH> &sections,
               const TKit::StringView line)
{
    const TKit::StringView content = line.Trim();
    if (content.IsEmpty() || content.GetFront() == COMMENT_MARKER)
        return;

    const usize separator = content.Find(KEY_VALUE_SEPARATOR);
    if (separator == TKit::StringView::npos)
    {
        TKIT_LOG_WARNING("[LIM][LEVEL_PARAMS] Ignoring line without 'key: value': '{}'", content);
        return;
    }

    const TKit::StringView key = content.SubString(0, separator).Trim();
    const TKit::StringView text = content.SubString(separator + 1).Trim();
    if (key.IsEmpty())
    {
        TKIT_LOG_WARNING("[LIM][LEVEL_PARAMS] Ignoring line without a key: '{}'", content);
        return;
    }

    const usize indent = line.FindFirstNotOf(" ");
    const usize depth = indent == TKit::StringView::npos ? 0 : indent / INDENT_WIDTH;
    if (depth >= MAX_NESTING_DEPTH)
    {
        TKIT_LOG_WARNING("[LIM][LEVEL_PARAMS] Ignoring line nested too deeply: '{}'", content);
        return;
    }

    if (text.IsEmpty())
    {
        sections[depth] = key;
        return;
    }

    TKit::FixedArray<TKit::StringView, MAX_NESTING_DEPTH> path{};
    for (usize i = 0; i < depth; ++i)
        path[i] = sections[i];
    path[depth] = key;

    const AssignResult result = AssignPath(params, TKit::Span<const TKit::StringView>(path.GetData(), depth + 1), text);
    if (result == AssignResult::Invalid)
    {
        TKIT_LOG_WARNING("[LIM][LEVEL_PARAMS] Ignoring invalid value for '{}': '{}'", key, text);
    }
    else if (result == AssignResult::Unknown)
    {
        TKIT_LOG_WARNING("[LIM][LEVEL_PARAMS] Ignoring unknown key '{}'", key);
    }
}

void Decode(LevelParams &params, const TKit::StringView text)
{
    TKit::FixedArray<TKit::StringView, MAX_NESTING_DEPTH> sections{};
    for (usize start = 0; start < text.GetSize();)
    {
        const usize end = TKit::Math::Min(text.Find(LINE_BREAK, start), text.GetSize());
        ApplyLine(params, sections, text.SubString(start, end - start));
        start = end + 1;
    }
}

template <typename Value, typename Params> bool OrderMembers(Params &params, const OrderedPair<Params> &pair)
{
    const auto *const low = std::get_if<Value Params::*>(&pair.Low);
    const auto *const high = std::get_if<Value Params::*>(&pair.High);
    if (!low || !high)
        return false;

    Value &first = params.*(*low);
    Value &second = params.*(*high);
    if (first <= second)
        return false;
    second = std::exchange(first, second);
    return true;
}

template <typename Value, typename Params> bool ClampMember(Params &params, const ParamSpec<Params> &spec)
{
    const auto *const member = std::get_if<Value Params::*>(&spec.Member);
    if (!member)
        return false;

    Value &target = params.*(*member);
    Value bounded = target;
    if (spec.Flags & ParamSpecFlag_Min)
        bounded = TKit::Math::Max(bounded, Value(spec.Min));
    if (spec.Flags & ParamSpecFlag_Max)
        bounded = TKit::Math::Min(bounded, Value(spec.Max));
    const bool changed = bounded != target;
    target = bounded;
    return changed;
}

/// Orders each pair, then clamps each bounded field, innermost structs first; reports whether any field changed.
template <typename Params> bool ApplySchema(Params &params)
{
    bool changed = false;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
            changed = ApplySchema(field.Get(params)) || changed;
    });
    if constexpr (HasParamPairs<Params>)
        for (const OrderedPair<Params> &pair : ParamSchema<Params>::PAIRS)
            changed = OrderMembers<f32>(params, pair) || OrderMembers<u32>(params, pair) || changed;
    if constexpr (HasParamFields<Params>)
        for (const ParamSpec<Params> &spec : ParamSchema<Params>::FIELDS)
            changed = ClampMember<f32>(params, spec) || ClampMember<u32>(params, spec) || changed;
    return changed;
}

void FitToRadius(LevelParams &params)
{
    const f32 radius = params.Mechanics.Shell.Radius;
    const f32 maxAmplitude = MAX_WAVE_AMPLITUDE_FRACTION * radius;
    ShellMotionParams &motion = params.LookFeel.ShellMotion;
    motion.WaveAmplitude = TKit::Math::Min(motion.WaveAmplitude, maxAmplitude);
    motion.NoiseAmplitude = TKit::Math::Min(motion.NoiseAmplitude, maxAmplitude);

    const f32 maxSize = MAX_FORM_SIZE_FRACTION * radius;
    MechanicsParams &mechanics = params.Mechanics;
    mechanics.Enemies.MinSize = TKit::Math::Min(mechanics.Enemies.MinSize, maxSize);
    mechanics.Enemies.MaxSize = TKit::Math::Min(mechanics.Enemies.MaxSize, maxSize);
    mechanics.Solids.MinSize = TKit::Math::Min(mechanics.Solids.MinSize, maxSize);
    mechanics.Solids.MaxSize = TKit::Math::Min(mechanics.Solids.MaxSize, maxSize);
    mechanics.Goal.Size = TKit::Math::Min(mechanics.Goal.Size, maxSize);

    RecordParams &records = mechanics.Records;
    records.EnemySizeMin = TKit::Math::Min(records.EnemySizeMin, maxSize);
    records.EnemySizeMax = TKit::Math::Min(records.EnemySizeMax, maxSize);
    records.SolidSizeMin = TKit::Math::Min(records.SolidSizeMin, maxSize);
    records.SolidSizeMax = TKit::Math::Min(records.SolidSizeMax, maxSize);
    records.GoalSize = TKit::Math::Min(records.GoalSize, maxSize);
}

/// Colors have no constexpr constructor, so the defaults cannot be checked at compile time.
template <typename Params> [[nodiscard]] bool AreDefaultsWithinSchema()
{
    Params defaults{};
    return !ApplySchema(defaults);
}

/// Every reflected leaf has exactly one spec, no spec names anything else, and each pair holds two fields of one type.
template <typename Params> [[nodiscard]] constexpr bool IsSchemaSound()
{
    u32 leaves = 0;
    bool sound = true;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
            sound = sound && IsSchemaSound<Type>();
        else
        {
            ++leaves;
            sound = sound && CountSpecs(field.Pointer) == 1;
        }
    });
    if constexpr (HasParamPairs<Params>)
        for (const OrderedPair<Params> &pair : ParamSchema<Params>::PAIRS)
            sound = sound && pair.Low.index() == pair.High.index();
    if constexpr (HasParamFields<Params>)
        return sound && leaves == ParamSchema<Params>::FIELDS.GetSize();
    else
        return sound && leaves == 0;
}

static_assert(IsSchemaSound<LevelParams>(), "Every tuning field needs exactly one ParamSpec in its struct's schema");
} // namespace

LevelParams LoadLevelParams()
{
    std::ifstream stream{LIM_DEFAULTS_PATH};
    if (!stream)
        return LevelParams{};

    TKit::TierString text;
    for (std::istreambuf_iterator<char> it{stream}; it != std::istreambuf_iterator<char>{}; ++it)
        text.Append(*it);

    LevelParams params{};
    Decode(params, text);
    return params;
}

bool SaveLevelParams(const LevelParams &params)
{
    TKit::TierString text;
    AppendFields(text, params, 0);

    const std::filesystem::path path{LIM_DEFAULTS_PATH};
    std::filesystem::path temp{path};
    temp += TEMP_SUFFIX;

    std::error_code error;
    std::ofstream stream{temp, std::ios::trunc};
    stream << text.CString();
    stream.close();
    if (stream.fail())
    {
        std::filesystem::remove(temp, error);
        return false;
    }
    std::filesystem::rename(temp, path, error);
    return !error;
}

LevelParams Sanitize(LevelParams params)
{
    TKIT_ASSERT(AreDefaultsWithinSchema<LevelParams>(),
                "[LIM][LEVEL_PARAMS] A default breaks its pair order or bounds");
    ApplySchema(params);
    FitToRadius(params);
    return params;
}
} // namespace Lim

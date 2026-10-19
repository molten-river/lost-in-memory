#include "lim/level_params.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/tier_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/reflection/lim/level_params.hpp"
#include "tkit/utils/logging.hpp"

#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>

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

template <typename Params>
AssignResult Assign(Params &params, const TKit::StringView section, const TKit::StringView key,
                    const TKit::StringView text)
{
    AssignResult outcome = AssignResult::Unknown;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
        {
            if (section == field.Name)
                outcome = Assign(field.Get(params), TKit::StringView{}, key, text);
        }
        else if (section.IsEmpty() && key == field.Name)
            outcome = ParseNumber(field.Get(params), text) ? AssignResult::Assigned : AssignResult::Invalid;
    });
    return outcome;
}

void ApplyLine(LevelParams &params, TKit::StringView &section, const TKit::StringView line)
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
    if (text.IsEmpty())
    {
        section = key;
        return;
    }

    const bool nested = line.FindFirstNotOf(" ") != 0;
    const AssignResult result = Assign(params, nested ? section : TKit::StringView{}, key, text);
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
    TKit::StringView section{};
    for (usize start = 0; start < text.GetSize();)
    {
        const usize end = TKit::Math::Min(text.Find(LINE_BREAK, start), text.GetSize());
        ApplyLine(params, section, text.SubString(start, end - start));
        start = end + 1;
    }
}
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
} // namespace Lim

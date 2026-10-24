#pragma once

#include "alias/alias.hpp"

#include "onyx/color.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/container/tier_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/utils/logging.hpp"

#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <type_traits>
#include <utility>

namespace Lim
{
enum class ParamAssignResult : u8
{
    Unknown,
    Assigned,
    Invalid
};

inline constexpr usize PARAMS_INDENT_WIDTH = 2;
inline constexpr usize PARAMS_NUMBER_BUFFER_SIZE = 32;
inline constexpr usize PARAMS_MAX_NESTING_DEPTH = 4;
inline constexpr usize PARAMS_COLOR_HEX_DIGITS = 6;
inline constexpr char PARAMS_COMMENT_MARKER = '#';
inline constexpr char PARAMS_KEY_VALUE_SEPARATOR = ':';
inline constexpr char PARAMS_LINE_BREAK = '\n';
inline constexpr const char *PARAMS_TEMP_SUFFIX = ".tmp";
inline constexpr TKit::StringView PARAMS_LOG_TAG = "[LIM][PARAMS]";

template <typename Number> inline void AppendParamNumber(TKit::TierString &out, const Number number)
{
    TKit::FixedArray<char, PARAMS_NUMBER_BUFFER_SIZE> buffer;
    const std::to_chars_result result =
        std::to_chars(buffer.GetData(), buffer.GetData() + PARAMS_NUMBER_BUFFER_SIZE - 1, number);
    *result.ptr = 0;
    out += buffer.GetData();
}

inline void AppendParamBool(TKit::TierString &out, const bool value)
{
    out += value ? "true" : "false";
}

inline void AppendParamColor(TKit::TierString &out, const Onyx::Color &color)
{
    const TKit::StaticString8 hex = color.ToHexadecimal<TKit::StaticString8>(false);
    out += '#';
    out += hex.CString();
}

template <typename Root> void AppendParamFields(TKit::TierString &out, const Root &root, const usize depth)
{
    TKit::Reflect<Root>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        for (usize i = 0; i < depth * PARAMS_INDENT_WIDTH; ++i)
            out += ' ';
        out += field.Name;
        out += PARAMS_KEY_VALUE_SEPARATOR;
        if constexpr (ReflectedStruct<Type>)
        {
            out += PARAMS_LINE_BREAK;
            AppendParamFields(out, field.Get(root), depth + 1);
        }
        else
        {
            out += ' ';
            if constexpr (std::is_enum_v<Type>)
                out += TKit::Reflect<Type>::ToString(field.Get(root));
            else if constexpr (std::is_same_v<Type, bool>)
                AppendParamBool(out, field.Get(root));
            else if constexpr (std::is_same_v<Type, Onyx::Color>)
                AppendParamColor(out, field.Get(root));
            else
                AppendParamNumber(out, field.Get(root));
            out += PARAMS_LINE_BREAK;
        }
    });
}

template <typename Number> inline bool ParseParamNumber(Number &target, const TKit::StringView text)
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

[[nodiscard]] inline bool ParseParamBool(bool &target, const TKit::StringView text)
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

[[nodiscard]] inline bool IsParamHexDigit(const char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

[[nodiscard]] inline bool ParseParamColor(Onyx::Color &target, const TKit::StringView text)
{
    if (text.GetSize() != PARAMS_COLOR_HEX_DIGITS + 1 || text.GetFront() != '#')
        return false;
    const TKit::StringView digits = text.SubString(1);
    for (const char c : digits)
        if (!IsParamHexDigit(c))
            return false;
    target = Onyx::Color::FromHexadecimal(digits);
    return true;
}

/// FromString TKIT_FATALs on an unknown string, so every enumerator is matched against ToString first.
template <typename Type>
    requires std::is_enum_v<Type>
[[nodiscard]] bool ParseParamEnum(Type &target, const TKit::StringView text)
{
    for (u32 i = 0; const char *name = TKit::Reflect<Type>::ToString(Type(i)); ++i)
        if (text == name)
        {
            target = Type(i);
            return true;
        }
    return false;
}

template <typename Root>
ParamAssignResult AssignParamPath(Root &root, const TKit::Span<const TKit::StringView> path,
                                  const TKit::StringView text)
{
    ParamAssignResult outcome = ParamAssignResult::Unknown;
    TKit::Reflect<Root>::ForEachMemberField([&](const auto &field) {
        if (path.GetFront() != field.Name)
            return;
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (ReflectedStruct<Type>)
        {
            if (path.GetSize() > 1)
                outcome = AssignParamPath(
                    field.Get(root), TKit::Span<const TKit::StringView>(path.GetData() + 1, path.GetSize() - 1), text);
        }
        else if (path.GetSize() == 1)
        {
            if constexpr (std::is_enum_v<Type>)
                outcome =
                    ParseParamEnum(field.Get(root), text) ? ParamAssignResult::Assigned : ParamAssignResult::Invalid;
            else if constexpr (std::is_same_v<Type, bool>)
                outcome =
                    ParseParamBool(field.Get(root), text) ? ParamAssignResult::Assigned : ParamAssignResult::Invalid;
            else if constexpr (std::is_same_v<Type, Onyx::Color>)
                outcome =
                    ParseParamColor(field.Get(root), text) ? ParamAssignResult::Assigned : ParamAssignResult::Invalid;
            else
                outcome =
                    ParseParamNumber(field.Get(root), text) ? ParamAssignResult::Assigned : ParamAssignResult::Invalid;
        }
    });
    return outcome;
}

/// Tracks the enclosing section name at each indentation depth, as set by the last header line seen at that depth.
template <typename Root>
void ApplyParamLine(Root &root, TKit::FixedArray<TKit::StringView, PARAMS_MAX_NESTING_DEPTH> &sections,
                    const TKit::StringView line)
{
    const TKit::StringView content = line.Trim();
    if (content.IsEmpty() || content.GetFront() == PARAMS_COMMENT_MARKER)
        return;

    const usize separator = content.Find(PARAMS_KEY_VALUE_SEPARATOR);
    if (separator == TKit::StringView::npos)
    {
        TKIT_LOG_WARNING("{} Ignoring line without 'key: value': '{}'", PARAMS_LOG_TAG, content);
        return;
    }

    const TKit::StringView key = content.SubString(0, separator).Trim();
    const TKit::StringView text = content.SubString(separator + 1).Trim();
    if (key.IsEmpty())
    {
        TKIT_LOG_WARNING("{} Ignoring line without a key: '{}'", PARAMS_LOG_TAG, content);
        return;
    }

    const usize indent = line.FindFirstNotOf(" ");
    const usize depth = indent == TKit::StringView::npos ? 0 : indent / PARAMS_INDENT_WIDTH;
    if (depth >= PARAMS_MAX_NESTING_DEPTH)
    {
        TKIT_LOG_WARNING("{} Ignoring line nested too deeply: '{}'", PARAMS_LOG_TAG, content);
        return;
    }

    if (text.IsEmpty())
    {
        sections[depth] = key;
        return;
    }

    TKit::FixedArray<TKit::StringView, PARAMS_MAX_NESTING_DEPTH> path{};
    for (usize i = 0; i < depth; ++i)
        path[i] = sections[i];
    path[depth] = key;

    const ParamAssignResult result =
        AssignParamPath(root, TKit::Span<const TKit::StringView>(path.GetData(), depth + 1), text);
    if (result == ParamAssignResult::Invalid)
    {
        TKIT_LOG_WARNING("{} Ignoring invalid value for '{}': '{}'", PARAMS_LOG_TAG, key, text);
    }
    else if (result == ParamAssignResult::Unknown)
    {
        TKIT_LOG_WARNING("{} Ignoring unknown key '{}'", PARAMS_LOG_TAG, key);
    }
}

template <typename Root> void DecodeParams(Root &root, const TKit::StringView text)
{
    TKit::FixedArray<TKit::StringView, PARAMS_MAX_NESTING_DEPTH> sections{};
    for (usize start = 0; start < text.GetSize();)
    {
        const usize end = TKit::Math::Min(text.Find(PARAMS_LINE_BREAK, start), text.GetSize());
        ApplyParamLine(root, sections, text.SubString(start, end - start));
        start = end + 1;
    }
}

/// Reads path; a missing file, field, malformed line or invalid value keeps the struct default for it.
template <typename Root> [[nodiscard]] Root LoadParams(const TKit::StringView path)
{
    const std::filesystem::path filePath{path.GetData(), path.GetData() + path.GetSize()};
    std::ifstream stream{filePath};
    if (!stream)
        return Root{};

    TKit::TierString text;
    for (std::istreambuf_iterator<char> it{stream}; it != std::istreambuf_iterator<char>{}; ++it)
        text.Append(*it);

    Root root{};
    DecodeParams(root, text);
    return root;
}

/// Writes path through a temp file and a rename; returns whether it succeeded.
template <typename Root> bool SaveParams(const TKit::StringView path, const Root &root)
{
    TKit::TierString text;
    AppendParamFields(text, root, 0);

    const std::filesystem::path filePath{path.GetData(), path.GetData() + path.GetSize()};
    std::filesystem::path temp{filePath};
    temp += PARAMS_TEMP_SUFFIX;

    std::error_code error;
    std::ofstream stream{temp, std::ios::trunc};
    stream << text.CString();
    stream.close();
    if (stream.fail())
    {
        std::filesystem::remove(temp, error);
        return false;
    }
    std::filesystem::rename(temp, filePath, error);
    return !error;
}
} // namespace Lim

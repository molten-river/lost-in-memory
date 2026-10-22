#pragma once

#include "lim/alias.hpp"

#include "onyx/color.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"

#include <variant>

namespace Lim
{
inline constexpr const char *INTEGER_FORMAT = "{}";
inline constexpr const char *WHOLE_FORMAT = "{:.0f}";
inline constexpr const char *ONE_DECIMAL_FORMAT = "{:.1f}";
inline constexpr const char *TWO_DECIMALS_FORMAT = "{:.2f}";
inline constexpr const char *THREE_DECIMALS_FORMAT = "{:.3f}";

struct DragStyle
{
    f32 Speed = 0.f;
    const char *Format = nullptr;
};

inline constexpr DragStyle FINE_INTEGER{0.05f, INTEGER_FORMAT};
inline constexpr DragStyle COARSE_INTEGER{0.25f, INTEGER_FORMAT};
inline constexpr DragStyle UNIT_SHARE{0.005f, TWO_DECIMALS_FORMAT};
inline constexpr DragStyle FORM_SIZE{0.02f, TWO_DECIMALS_FORMAT};
inline constexpr DragStyle SIZE_FRACTION{0.002f, TWO_DECIMALS_FORMAT};
inline constexpr DragStyle SCALE_FACTOR{0.005f, TWO_DECIMALS_FORMAT};
inline constexpr DragStyle PERIOD_SECONDS{0.05f, ONE_DECIMAL_FORMAT};
inline constexpr DragStyle GRID_CELLS{COARSE_INTEGER};
inline constexpr DragStyle GRID_SPACING{0.05f, ONE_DECIMAL_FORMAT};
inline constexpr DragStyle GRID_DISTANCE{0.1f, ONE_DECIMAL_FORMAT};
inline constexpr DragStyle TIME_MULTIPLIER{0.01f, TWO_DECIMALS_FORMAT};
inline constexpr DragStyle PINCH_SHARE{0.001f, THREE_DECIMALS_FORMAT};
inline constexpr DragStyle TEXT_POINTS{0.05f, WHOLE_FORMAT};

using ParamSpecFlags = u8;
enum ParamSpecFlagBit : ParamSpecFlags
{
    ParamSpecFlag_Min = 1U << 0,
    ParamSpecFlag_Max = 1U << 1,
    ParamSpecFlag_Drag = 1U << 2
};

inline constexpr ParamSpecFlags BOTH_BOUNDS = ParamSpecFlag_Min | ParamSpecFlag_Max;
inline constexpr ParamSpecFlags DRAGGED_BOUNDS = BOTH_BOUNDS | ParamSpecFlag_Drag;

template <typename Params>
using ParamMember = std::variant<f32 Params::*, u32 Params::*, bool Params::*, Onyx::Color Params::*>;

template <typename Params> using PairMember = std::variant<f32 Params::*, u32 Params::*>;

/// One reflected leaf field: its tooltip, the bounds its flags enable and, when flagged, the drag style of its widget.
template <typename Params> struct ParamSpec
{
    ParamMember<Params> Member;
    TKit::StringView Hint;
    ParamSpecFlags Flags = 0;
    f32 Min = 0.f;
    f32 Max = 0.f;
    DragStyle Drag{};

    template <typename Value> [[nodiscard]] constexpr bool Describes(Value Params::*const member) const
    {
        const auto *const described = std::get_if<Value Params::*>(&Member);
        return described && *described == member;
    }
};

/// Two fields of one type that must stay in order: Low never exceeds High.
template <typename Params> struct OrderedPair
{
    PairMember<Params> Low;
    PairMember<Params> High;
};

/// Specialised next to each params struct with a FIELDS table of ParamSpec and, when it has any, a PAIRS table.
template <typename Params> struct ParamSchema
{
};

template <typename Params>
concept HasParamFields = requires { ParamSchema<Params>::FIELDS; };

template <typename Params>
concept HasParamPairs = requires { ParamSchema<Params>::PAIRS; };

/// How many specs of the struct's schema describe the member; a well formed schema yields one per reflected leaf.
template <typename Params, typename Value> [[nodiscard]] constexpr u32 CountSpecs(Value Params::*const member)
{
    u32 count = 0;
    if constexpr (HasParamFields<Params>)
        for (const ParamSpec<Params> &spec : ParamSchema<Params>::FIELDS)
            count += spec.Describes(member) ? 1U : 0U;
    return count;
}

/// The member must have a spec; a missing one trips the bounds assert of the table.
template <typename Params, typename Value>
    requires HasParamFields<Params>
[[nodiscard]] constexpr const ParamSpec<Params> &FindSpec(Value Params::*const member)
{
    usize index = 0;
    while (index < ParamSchema<Params>::FIELDS.GetSize() && !ParamSchema<Params>::FIELDS[index].Describes(member))
        ++index;
    return ParamSchema<Params>::FIELDS[index];
}
} // namespace Lim

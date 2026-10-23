#pragma once

#include "alias/alias.hpp"
#include "tuning/param_schema.hpp"

#include "tkit/math/math.hpp"
#include "tkit/reflection/reflect.hpp"

#include <type_traits>
#include <utility>
#include <variant>

namespace Lim
{
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
} // namespace Lim

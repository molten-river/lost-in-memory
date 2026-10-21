#pragma once

#include "lim/alias.hpp"
#include "lim/form_recipe.hpp"
#include "lim/formula_params.hpp"

#include "tkit/container/span.hpp"
#include <cstddef>

namespace Lim
{
[[nodiscard]] WorldRecipe DecodeWorld(TKit::Span<const std::byte> region, const RecordParams &records,
                                      const EvolutionParams &evolution);
[[nodiscard]] FormRecipe DecodeRecord(TKit::Span<const std::byte> record, FormKind kind, const RecordParams &records,
                                      const EvolutionParams &evolution);
} // namespace Lim

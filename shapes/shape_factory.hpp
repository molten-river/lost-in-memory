#pragma once

#include "alias/alias.hpp"
#include "shapes/shape.hpp"
#include "shapes/shape_kind.hpp"
#include "shapes/shape_kit_params.hpp"

#include "tkit/container/span.hpp"
#include "tkit/memory/ptr.hpp"
#include "tkit/utils/optional.hpp"

#include <cstddef>

namespace Lim
{
/// A registry of one creator per shape kind; it bakes a record into a shape at load time, never in a frame.
class ShapeFactory
{
  public:
    [[nodiscard]] static TKit::Scope<Shape> Create(ShapeKind kind, TKit::Span<const std::byte> record,
                                                   const ShapeKitParams &params, TKit::Optional<f32> sizeOverride = {});
};
} // namespace Lim

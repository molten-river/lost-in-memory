#pragma once

#include "alias/alias.hpp"
#include "shapes/shape_kit_params.hpp"

namespace Lim
{
/// Reads the shared kit tuning file; a missing file, field or invalid value keeps its default, then the schema clamps.
[[nodiscard]] ShapeKitParams LoadShapeKitParams();
/// Writes the shared kit tuning file through a temp file and a rename; returns whether it succeeded.
bool SaveShapeKitParams(const ShapeKitParams &params);
} // namespace Lim

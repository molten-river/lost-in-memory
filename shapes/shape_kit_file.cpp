#include "shapes/shape_kit_file.hpp"
#include "tuning/params_file.hpp"
#include "tuning/schema_rules.hpp"

#include "tkit/reflection/shapes/shape_kit_params.hpp"
#include "tkit/reflection/shapes/walk_mass_params.hpp"

namespace Lim
{
ShapeKitParams LoadShapeKitParams()
{
    ShapeKitParams params = LoadParams<ShapeKitParams>(LIM_SHAPE_KIT_DEFAULTS_PATH);
    ApplySchema(params);
    return params;
}

bool SaveShapeKitParams(const ShapeKitParams &params)
{
    return SaveParams(LIM_SHAPE_KIT_DEFAULTS_PATH, params);
}
} // namespace Lim

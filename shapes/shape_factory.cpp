#include "shapes/shape_factory.hpp"
#include "shapes/walk_mass_builder.hpp"
#include "tuning/schema_rules.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/reflection/shapes/shape_kit_params.hpp"
#include "tkit/reflection/shapes/walk_mass_params.hpp"

namespace Lim
{
namespace
{
using Creator = TKit::Scope<Shape> (*)(TKit::Span<const std::byte>, const ShapeKitParams &, TKit::Optional<f32>);

[[nodiscard]] TKit::Scope<Shape> CreateWalkMass(const TKit::Span<const std::byte> record, const ShapeKitParams &params,
                                                const TKit::Optional<f32> sizeOverride)
{
    return WalkMassBuilder{record, params.WalkMass, sizeOverride}.Build();
}

constexpr TKit::FixedArray<Creator, SHAPE_KIND_COUNT> CREATORS{&CreateWalkMass};

static_assert(u32(ShapeKind::WalkMass) == 0, "CREATORS lists one creator per ShapeKind, in order");
static_assert(IsSchemaSound<ShapeKitParams>(), "Every shape tuning field needs exactly one ParamSpec in its schema");
} // namespace

TKit::Scope<Shape> ShapeFactory::Create(const ShapeKind kind, const TKit::Span<const std::byte> record,
                                        const ShapeKitParams &params, const TKit::Optional<f32> sizeOverride)
{
    return CREATORS[u32(kind)](record, params, sizeOverride);
}
} // namespace Lim

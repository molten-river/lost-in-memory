#pragma once

#include "lim/alias.hpp"
#include "lim/blob.hpp"
#include "lim/byte_reader.hpp"
#include "lim/capacity.hpp"
#include "lim/shape_params.hpp"

#include "tkit/container/static_array.hpp"

namespace Lim
{
/// MinOpening is the diameter of the sphere every through-opening must fit; Carved selects the machined grammar.
struct ShapeRequest
{
    f32 MinSize;
    f32 MaxSize;
    u32 MinPrimitives;
    u32 MaxPrimitives;
    f32 BlendRadius;
    f32 MinOpening;
    bool Carved;
};

struct ShapeRecipe
{
    TKit::StaticArray<BlobPrimitive, MAX_PRIMITIVES> Primitives;
    f32 BlendRadius;
};

/// Smooth unions of soft primitives, or machined slabs, frames, rings and cages cut by CSG when the request is carved.
[[nodiscard]] ShapeRecipe DecodeShape(ByteReader &reader, const ShapeRequest &request, const ShapeParams &params);
} // namespace Lim

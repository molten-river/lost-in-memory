#pragma once

#include "alias/alias.hpp"

#include "onyx/core.hpp"

namespace Lim
{
/// The most 3D geometry a program holds at once and draws in a frame: each mesh once, one index per vertex.
struct EngineBudget
{
    u32 StaticMeshes = 0;
    u32 StaticPoolVertices = 0;
    u32 DynamicMeshes = 0;
    u32 DynamicVertices = 0;
};

[[nodiscard]] Onyx::Specs MakeEngineSpecs(const EngineBudget &budget);
} // namespace Lim

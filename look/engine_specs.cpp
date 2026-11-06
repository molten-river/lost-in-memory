#include "look/engine_specs.hpp"

#include "tkit/math/math.hpp"

namespace Lim
{
namespace
{
/// Resources_CreateDefault registers ten 3D static and parametric meshes, each with its bounds.
constexpr u32 DEFAULT_MESH_COUNT = 10;
/// Every mesh draws in the shaded pass alone: no light casts shadows and nothing is outlined.
constexpr u32 PASSES_PER_MESH = 1;
/// A reload registers the new pool's bounds before the sync that destroys the released pool's.
constexpr u32 LIVE_POOLS_PER_RELOAD = 2;
/// The default pool capacity, which Resources_CreateDefault's meshes rely on.
constexpr Onyx::SpecificSpecs<Onyx::D3> ONYX_DEFAULTS{};
} // namespace

Onyx::Specs MakeEngineSpecs(const EngineBudget &budget)
{
    return Onyx::Specs{.Specs3 = {.MaxInstances = TKit::Math::Max(budget.StaticMeshes, budget.DynamicMeshes),
                                  .MaxDrawCommands = (budget.StaticMeshes + budget.DynamicMeshes) * PASSES_PER_MESH,
                                  .MaxVerticesPerResourcePool = TKit::Math::Max(
                                      budget.StaticPoolVertices, ONYX_DEFAULTS.MaxVerticesPerResourcePool),
                                  .MaxIndicesPerResourcePool = TKit::Math::Max(budget.StaticPoolVertices,
                                                                               ONYX_DEFAULTS.MaxIndicesPerResourcePool),
                                  .MaxDynamicVertices = budget.DynamicVertices,
                                  .MaxDynamicIndices = budget.DynamicVertices,
                                  .MaxDynamicMeshes = budget.DynamicMeshes,
                                  .MaxBounds = DEFAULT_MESH_COUNT + LIVE_POOLS_PER_RELOAD * budget.StaticMeshes}};
}
} // namespace Lim

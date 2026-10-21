#include "lim/formula_params.hpp"

#include "lim/world_layout.hpp"

#include "tkit/math/math.hpp"

namespace Lim
{
namespace
{
constexpr u32 LETHAL_TAG_MIN = 1;
constexpr u32 LETHAL_TAG_MAX = 254;
constexpr f32 MIN_FORM_SIZE = 0.01f;
constexpr f32 MIN_LATTICE_STEP = 0.01f;
constexpr f32 MAX_LATTICE_STEP = 1.f;
constexpr u32 GRAPH_MIN_LINKS = 1;
constexpr u32 GRAPH_MAX_LINKS = 7;
constexpr f32 MIN_SLAB_DEPTH = 0.001f;
constexpr f32 MIN_PERIOD = 0.01f;
constexpr f32 MIN_GRID_SPACING = 0.01f;
constexpr f32 MIN_GRID_DISTANCE = 0.01f;

template <typename T> void OrderPair(T &min, T &max)
{
    const T low = TKit::Math::Min(min, max);
    const T high = TKit::Math::Max(min, max);
    min = low;
    max = high;
}
} // namespace

RecordParams Sanitize(RecordParams params)
{
    params.LethalTagMax = TKit::Math::Clamp(params.LethalTagMax, LETHAL_TAG_MIN, LETHAL_TAG_MAX);
    OrderPair(params.EnemySizeMin, params.EnemySizeMax);
    params.EnemySizeMin = TKit::Math::Max(params.EnemySizeMin, MIN_FORM_SIZE);
    params.EnemySizeMax = TKit::Math::Max(params.EnemySizeMax, MIN_FORM_SIZE);
    OrderPair(params.SolidSizeMin, params.SolidSizeMax);
    params.SolidSizeMin = TKit::Math::Max(params.SolidSizeMin, MIN_FORM_SIZE);
    params.SolidSizeMax = TKit::Math::Max(params.SolidSizeMax, MIN_FORM_SIZE);
    params.GoalSize = TKit::Math::Max(params.GoalSize, MIN_FORM_SIZE);
    OrderPair(params.LatticeStepMin, params.LatticeStepMax);
    params.LatticeStepMin = TKit::Math::Clamp(params.LatticeStepMin, MIN_LATTICE_STEP, MAX_LATTICE_STEP);
    params.LatticeStepMax = TKit::Math::Clamp(params.LatticeStepMax, MIN_LATTICE_STEP, MAX_LATTICE_STEP);
    OrderPair(params.ProfileRadiusMin, params.ProfileRadiusMax);
    params.ProfileRadiusMin = TKit::Math::Max(params.ProfileRadiusMin, 0.f);
    params.ProfileRadiusMax = TKit::Math::Max(params.ProfileRadiusMax, 0.f);
    OrderPair(params.SectionScaleMin, params.SectionScaleMax);
    params.SectionScaleMin = TKit::Math::Max(params.SectionScaleMin, 0.f);
    params.SectionScaleMax = TKit::Math::Max(params.SectionScaleMax, 0.f);
    OrderPair(params.ReachBiasMin, params.ReachBiasMax);
    params.ReachBiasMin = TKit::Math::Max(params.ReachBiasMin, 0.f);
    params.ReachBiasMax = TKit::Math::Max(params.ReachBiasMax, 0.f);
    return params;
}

GraphParams Sanitize(GraphParams params)
{
    params.JoinReach = TKit::Math::Max(params.JoinReach, 0.f);
    params.BreakReach = TKit::Math::Max(params.BreakReach, params.JoinReach);
    params.MaxLinks = TKit::Math::Clamp(params.MaxLinks, GRAPH_MIN_LINKS, GRAPH_MAX_LINKS);
    params.SlabDepth = TKit::Math::Max(params.SlabDepth, MIN_SLAB_DEPTH);
    params.PinchScale = TKit::Math::Clamp(params.PinchScale, 0.f, 1.f);
    return params;
}

EvolutionParams Sanitize(EvolutionParams params)
{
    OrderPair(params.PeriodMin, params.PeriodMax);
    params.PeriodMin = TKit::Math::Max(params.PeriodMin, MIN_PERIOD);
    params.PeriodMax = TKit::Math::Max(params.PeriodMax, params.PeriodMin);
    params.SwayMax = TKit::Math::Max(params.SwayMax, 0.f);
    params.PulseMax = TKit::Math::Clamp(params.PulseMax, 0.f, 1.f);
    params.Speed = TKit::Math::Max(params.Speed, 0.f);
    return params;
}

TestBenchParams Sanitize(TestBenchParams params)
{
    params.GridCount = TKit::Math::Clamp(params.GridCount, 1U, u32(RECORD_COUNT));
    params.GridColumns = TKit::Math::Clamp(params.GridColumns, 1U, u32(RECORD_COUNT));
    params.GridSpacing = TKit::Math::Max(params.GridSpacing, MIN_GRID_SPACING);
    params.GridDistance = TKit::Math::Max(params.GridDistance, MIN_GRID_DISTANCE);
    return params;
}
} // namespace Lim

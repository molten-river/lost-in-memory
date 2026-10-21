#pragma once

#include "lim/alias.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct RecordParams
{
    TKIT_REFLECT_DECLARE(RecordParams)
    u32 LethalTagMax = 170;
    f32 EnemySizeMin = 1.2f;
    f32 EnemySizeMax = 3.f;
    f32 SolidSizeMin = 2.f;
    f32 SolidSizeMax = 4.f;
    f32 GoalSize = 1.5f;
    f32 LatticeStepMin = 0.15f;
    f32 LatticeStepMax = 0.4f;
    f32 ProfileRadiusMin = 0.08f;
    f32 ProfileRadiusMax = 0.35f;
    f32 SectionScaleMin = 0.4f;
    f32 SectionScaleMax = 1.2f;
    f32 ReachBiasMin = 0.7f;
    f32 ReachBiasMax = 1.3f;
};

struct GraphParams
{
    TKIT_REFLECT_DECLARE(GraphParams)
    f32 JoinReach = 0.7f;
    f32 BreakReach = 0.9f;
    u32 MaxLinks = 3;
    f32 SlabDepth = 0.12f;
    f32 PinchScale = 0.05f;
};

struct EvolutionParams
{
    TKIT_REFLECT_DECLARE(EvolutionParams)
    f32 PeriodMin = 2.f;
    f32 PeriodMax = 12.f;
    f32 SwayMax = 0.3f;
    f32 PulseMax = 1.f;
    f32 Speed = 1.f;
};

struct TestBenchParams
{
    TKIT_REFLECT_DECLARE(TestBenchParams)
    u32 GridCount = 63;
    u32 GridColumns = 9;
    f32 GridSpacing = 10.f;
    f32 GridDistance = 30.f;
};

[[nodiscard]] RecordParams Sanitize(RecordParams params);
[[nodiscard]] GraphParams Sanitize(GraphParams params);
[[nodiscard]] EvolutionParams Sanitize(EvolutionParams params);
[[nodiscard]] TestBenchParams Sanitize(TestBenchParams params);
} // namespace Lim

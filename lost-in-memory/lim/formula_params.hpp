#pragma once

#include "alias/alias.hpp"
#include "lim/world_layout.hpp"
#include "tuning/param_schema.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct RecordParams
{
    TKIT_REFLECT_DECLARE(RecordParams)
    u32 LethalTagMax = 170;
    f32 MaxFormRadiusShare = 0.3f;
};

template <> struct ParamSchema<RecordParams>
{
    static constexpr TKit::FixedArray<ParamSpec<RecordParams>, 2> FIELDS{{
        {.Member = &RecordParams::LethalTagMax,
         .Hint = "Tags 1 to this are enemies; higher tags are solids",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = 254.f,
         .Drag = COARSE_INTEGER},
        {.Member = &RecordParams::MaxFormRadiusShare,
         .Hint = "Largest form reach as a share of the shell radius; larger forms are scaled down to fit",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.05f,
         .Max = 0.5f,
         .Drag = UNIT_SHARE},
    }};
};

struct TestBenchParams
{
    TKIT_REFLECT_DECLARE(TestBenchParams)
    u32 GridCount = 63;
    u32 GridColumns = 9;
    f32 GridSpacing = 36.f;
    f32 GridDistance = 120.f;
};

template <> struct ParamSchema<TestBenchParams>
{
    static constexpr TKit::FixedArray<ParamSpec<TestBenchParams>, 4> FIELDS{{
        {.Member = &TestBenchParams::GridCount,
         .Hint = "Records laid out on the test grid",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = f32(RECORD_COUNT),
         .Drag = GRID_CELLS},
        {.Member = &TestBenchParams::GridColumns,
         .Hint = "Columns of the test grid",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = f32(RECORD_COUNT),
         .Drag = GRID_CELLS},
        {.Member = &TestBenchParams::GridSpacing,
         .Hint = "Distance between neighbouring grid cells",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = 50.f,
         .Drag = GRID_SPACING},
        {.Member = &TestBenchParams::GridDistance,
         .Hint = "Distance from the start to the grid plane",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = 200.f,
         .Drag = GRID_DISTANCE},
    }};
};
} // namespace Lim

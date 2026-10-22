#pragma once

#include "lim/alias.hpp"
#include "lim/param_schema.hpp"
#include "lim/world_layout.hpp"

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

template <> struct ParamSchema<RecordParams>
{
    static constexpr TKit::FixedArray<ParamSpec<RecordParams>, 14> FIELDS{{
        {.Member = &RecordParams::LethalTagMax,
         .Hint = "Tags 1 to this are enemies; higher tags are solids",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = 254.f,
         .Drag = COARSE_INTEGER},
        {.Member = &RecordParams::EnemySizeMin,
         .Hint = "Enemy size at size byte 0",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 10.f,
         .Drag = FORM_SIZE},
        {.Member = &RecordParams::EnemySizeMax,
         .Hint = "Enemy size at size byte 255",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 10.f,
         .Drag = FORM_SIZE},
        {.Member = &RecordParams::SolidSizeMin,
         .Hint = "Solid size at size byte 0",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 10.f,
         .Drag = FORM_SIZE},
        {.Member = &RecordParams::SolidSizeMax,
         .Hint = "Solid size at size byte 255",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 10.f,
         .Drag = FORM_SIZE},
        {.Member = &RecordParams::GoalSize,
         .Hint = "Size of the goal record's form",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 10.f,
         .Drag = FORM_SIZE},
        {.Member = &RecordParams::LatticeStepMin,
         .Hint = "Section snapping step at step bits 0, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &RecordParams::LatticeStepMax,
         .Hint = "Section snapping step at step bits 31, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &RecordParams::ProfileRadiusMin,
         .Hint = "Profile vertex radius at nibble 0, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &RecordParams::ProfileRadiusMax,
         .Hint = "Profile vertex radius at nibble 15, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &RecordParams::SectionScaleMin,
         .Hint = "Section scale at scale nibble 1",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.05f,
         .Max = 3.f,
         .Drag = SCALE_FACTOR},
        {.Member = &RecordParams::SectionScaleMax,
         .Hint = "Section scale at scale nibble 15",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.05f,
         .Max = 3.f,
         .Drag = SCALE_FACTOR},
        {.Member = &RecordParams::ReachBiasMin,
         .Hint = "Join and break reach multiplier at reach byte 0",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.05f,
         .Max = 3.f,
         .Drag = SCALE_FACTOR},
        {.Member = &RecordParams::ReachBiasMax,
         .Hint = "Join and break reach multiplier at reach byte 255",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.05f,
         .Max = 3.f,
         .Drag = SCALE_FACTOR},
    }};

    static constexpr TKit::FixedArray<OrderedPair<RecordParams>, 6> PAIRS{{
        {&RecordParams::EnemySizeMin, &RecordParams::EnemySizeMax},
        {&RecordParams::SolidSizeMin, &RecordParams::SolidSizeMax},
        {&RecordParams::LatticeStepMin, &RecordParams::LatticeStepMax},
        {&RecordParams::ProfileRadiusMin, &RecordParams::ProfileRadiusMax},
        {&RecordParams::SectionScaleMin, &RecordParams::SectionScaleMax},
        {&RecordParams::ReachBiasMin, &RecordParams::ReachBiasMax},
    }};
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

template <> struct ParamSchema<GraphParams>
{
    static constexpr TKit::FixedArray<ParamSpec<GraphParams>, 5> FIELDS{{
        {.Member = &GraphParams::JoinReach,
         .Hint = "Sections closer than this times size link up",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.05f,
         .Max = 3.f,
         .Drag = SCALE_FACTOR},
        {.Member = &GraphParams::BreakReach,
         .Hint = "Linked sections farther than this times size break apart",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.05f,
         .Max = 3.f,
         .Drag = SCALE_FACTOR},
        {.Member = &GraphParams::MaxLinks,
         .Hint = "Most links one section can hold",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = 7.f,
         .Drag = FINE_INTEGER},
        {.Member = &GraphParams::SlabDepth,
         .Hint = "Thickness of an unlinked section's slab, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &GraphParams::PinchScale,
         .Hint = "Below this share of its rest scale a section drops its links",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 0.5f,
         .Drag = PINCH_SHARE},
    }};

    static constexpr OrderedPair<GraphParams> REACH{&GraphParams::JoinReach, &GraphParams::BreakReach};
    static constexpr TKit::FixedArray<OrderedPair<GraphParams>, 1> PAIRS{REACH};
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

template <> struct ParamSchema<EvolutionParams>
{
    static constexpr TKit::FixedArray<ParamSpec<EvolutionParams>, 5> FIELDS{{
        {.Member = &EvolutionParams::PeriodMin,
         .Hint = "Motion period at period nibble 0, in seconds",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 60.f,
         .Drag = PERIOD_SECONDS},
        {.Member = &EvolutionParams::PeriodMax,
         .Hint = "Motion period at period nibble 15, in seconds",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 60.f,
         .Drag = PERIOD_SECONDS},
        {.Member = &EvolutionParams::SwayMax,
         .Hint = "Sway distance at sway nibble 15, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 1.f,
         .Drag = UNIT_SHARE},
        {.Member = &EvolutionParams::PulseMax,
         .Hint = "Scale pinch at pulse nibble 15; 1 pinches to nothing",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 1.f,
         .Drag = UNIT_SHARE},
        {.Member = &EvolutionParams::Speed,
         .Hint = "Time multiplier for every section's motion",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 5.f,
         .Drag = TIME_MULTIPLIER},
    }};

    static constexpr OrderedPair<EvolutionParams> PERIOD{&EvolutionParams::PeriodMin, &EvolutionParams::PeriodMax};
    static constexpr TKit::FixedArray<OrderedPair<EvolutionParams>, 1> PAIRS{PERIOD};
};

struct TestBenchParams
{
    TKIT_REFLECT_DECLARE(TestBenchParams)
    u32 GridCount = 63;
    u32 GridColumns = 9;
    f32 GridSpacing = 10.f;
    f32 GridDistance = 30.f;
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

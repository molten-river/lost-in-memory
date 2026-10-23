#pragma once

#include "alias/alias.hpp"
#include "tuning/param_schema.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct WalkMassParams
{
    TKIT_REFLECT_DECLARE(WalkMassParams)
    f32 SizeMin = 2.f;
    f32 SizeMax = 6.f;
    f32 LengthMin = 0.15f;
    f32 LengthMax = 0.6f;
    f32 WidthMin = 0.06f;
    f32 WidthMax = 0.35f;
    f32 HeightMin = 0.06f;
    f32 HeightMax = 0.35f;
    f32 ChamferMax = 0.6f;
    f32 RollStepDegrees = 22.5f;
    f32 TaperMin = 0.6f;
    f32 LatticeStepMin = 0.05f;
    f32 LatticeStepMax = 0.2f;
    f32 OffsetShare = 0.5f;
    f32 SlabDepth = 0.12f;
    f32 PeriodMin = 2.f;
    f32 PeriodMax = 24.f;
    f32 HoldMin = 0.25f;
    f32 HoldMax = 0.9f;
    f32 RatchetStepDegrees = 15.f;
    f32 PistonReachMax = 0.6f;
    f32 RodScale = 0.55f;
    f32 RodOverlap = 0.3f;
    f32 SlideReachMax = 0.4f;
};

template <> struct ParamSchema<WalkMassParams>
{
    static constexpr DragStyle ANGLE_DEGREES{0.1f, ONE_DECIMAL_FORMAT};

    static constexpr TKit::FixedArray<ParamSpec<WalkMassParams>, 24> FIELDS{{
        {.Member = &WalkMassParams::SizeMin,
         .Hint = "Mass size at size byte 0",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 20.f,
         .Drag = FORM_SIZE},
        {.Member = &WalkMassParams::SizeMax,
         .Hint = "Mass size at size byte 255",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 20.f,
         .Drag = FORM_SIZE},
        {.Member = &WalkMassParams::LengthMin,
         .Hint = "Member length at length nibble 1 before snapping, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::LengthMax,
         .Hint = "Member length at length nibble 15 before snapping, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::WidthMin,
         .Hint = "Section width at width nibble 0, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::WidthMax,
         .Hint = "Section width at width nibble 15, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::HeightMin,
         .Hint = "Section height at height nibble 0, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::HeightMax,
         .Hint = "Section height at height nibble 15, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::ChamferMax,
         .Hint = "Corner cut at chamfer level 3, as a share of half the thinner side",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 0.95f,
         .Drag = UNIT_SHARE},
        {.Member = &WalkMassParams::RollStepDegrees,
         .Hint = "Profile roll about the heading per roll step, in degrees",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 90.f,
         .Drag = ANGLE_DEGREES},
        {.Member = &WalkMassParams::TaperMin,
         .Hint = "Start face scale at taper nibble 15; 1 keeps every member straight",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f,
         .Drag = SCALE_FACTOR},
        {.Member = &WalkMassParams::LatticeStepMin,
         .Hint = "Length snapping step at lattice byte 0, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.005f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::LatticeStepMax,
         .Hint = "Length snapping step at lattice byte 255, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.005f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::OffsetShare,
         .Hint = "Sideways joint shift of an offset section, as a share of the previous width",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 0.5f,
         .Drag = UNIT_SHARE},
        {.Member = &WalkMassParams::SlabDepth,
         .Hint = "Thickness of the slab a mass without members becomes, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 1.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::PeriodMin,
         .Hint = "Motion cycle at period nibble 0, in seconds",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 120.f,
         .Drag = PERIOD_SECONDS},
        {.Member = &WalkMassParams::PeriodMax,
         .Hint = "Motion cycle at period nibble 15, in seconds",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 120.f,
         .Drag = PERIOD_SECONDS},
        {.Member = &WalkMassParams::HoldMin,
         .Hint = "Share of a cycle spent still at hold bits 0",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 0.95f,
         .Drag = UNIT_SHARE},
        {.Member = &WalkMassParams::HoldMax,
         .Hint = "Share of a cycle spent still at hold bits 3",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 0.95f,
         .Drag = UNIT_SHARE},
        {.Member = &WalkMassParams::RatchetStepDegrees,
         .Hint = "Ratchet turn per click per step nibble, in degrees",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 180.f,
         .Drag = ANGLE_DEGREES},
        {.Member = &WalkMassParams::PistonReachMax,
         .Hint = "Piston stroke at amplitude 15, as a share of the member length",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 0.95f,
         .Drag = UNIT_SHARE},
        {.Member = &WalkMassParams::RodScale,
         .Hint = "Piston rod cross-section as a share of the member's end profile",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.05f,
         .Max = 0.95f,
         .Drag = UNIT_SHARE},
        {.Member = &WalkMassParams::RodOverlap,
         .Hint = "Rod length kept inside the sleeve at full stroke, as a share of the member length",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 0.95f,
         .Drag = UNIT_SHARE},
        {.Member = &WalkMassParams::SlideReachMax,
         .Hint = "Slide travel at amplitude 15, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 2.f,
         .Drag = SCALE_FACTOR},
    }};

    static constexpr TKit::FixedArray<OrderedPair<WalkMassParams>, 7> PAIRS{{
        {&WalkMassParams::SizeMin, &WalkMassParams::SizeMax},
        {&WalkMassParams::LengthMin, &WalkMassParams::LengthMax},
        {&WalkMassParams::WidthMin, &WalkMassParams::WidthMax},
        {&WalkMassParams::HeightMin, &WalkMassParams::HeightMax},
        {&WalkMassParams::LatticeStepMin, &WalkMassParams::LatticeStepMax},
        {&WalkMassParams::PeriodMin, &WalkMassParams::PeriodMax},
        {&WalkMassParams::HoldMin, &WalkMassParams::HoldMax},
    }};
};
} // namespace Lim

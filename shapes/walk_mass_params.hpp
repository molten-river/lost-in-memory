#pragma once

#include "alias/alias.hpp"
#include "tuning/param_schema.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct WalkMassParams
{
    TKIT_REFLECT_DECLARE(WalkMassParams)
    f32 SizeMin = 4.f;
    f32 SizeMax = 9.f;
    f32 LengthMin = 0.35f;
    f32 LengthMax = 1.4f;
    f32 BreadthMin = 0.5f;
    f32 BreadthMax = 1.2f;
    f32 ThicknessMin = 0.08f;
    f32 ThicknessMax = 0.3f;
    f32 PlinthBreadthMin = 0.9f;
    f32 PlinthBreadthMax = 1.5f;
    f32 PlinthThicknessMin = 0.15f;
    f32 PlinthThicknessMax = 0.4f;
    f32 MinBreadthShare = 0.5f;
    f32 LengthToBreadthMax = 1.5f;
    u32 MinMembers = 3;
    f32 MinContactShare = 0.3f;
    f32 ChamferMax = 0.25f;
    f32 RollStepDegrees = 90.f;
    f32 TaperMin = 0.7f;
    f32 LatticeStepMin = 0.08f;
    f32 LatticeStepMax = 0.25f;
    f32 OffsetShare = 0.35f;
    f32 PeriodMin = 3.f;
    f32 PeriodMax = 30.f;
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

    static constexpr TKit::FixedArray<ParamSpec<WalkMassParams>, 31> FIELDS{{
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
         .Hint = "Member length at length nibble 1 before snapping and capping, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::LengthMax,
         .Hint = "Member length at length nibble 15 before snapping and capping, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::BreadthMin,
         .Hint = "Plate breadth at breadth nibble 0, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::BreadthMax,
         .Hint = "Plate breadth at breadth nibble 15, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::ThicknessMin,
         .Hint = "Plate thickness at thickness nibble 0, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::ThicknessMax,
         .Hint = "Plate thickness at thickness nibble 15, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::PlinthBreadthMin,
         .Hint = "Root plinth breadth at breadth nibble 0, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::PlinthBreadthMax,
         .Hint = "Root plinth breadth at breadth nibble 15, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::PlinthThicknessMin,
         .Hint = "Root plinth thickness at thickness nibble 0, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::PlinthThicknessMax,
         .Hint = "Root plinth thickness at thickness nibble 15, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.01f,
         .Max = 3.f,
         .Drag = SIZE_FRACTION},
        {.Member = &WalkMassParams::MinBreadthShare,
         .Hint = "Smallest member breadth, as a share of the plinth breadth",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 2.f,
         .Drag = UNIT_SHARE},
        {.Member = &WalkMassParams::LengthToBreadthMax,
         .Hint = "Longest member, as a multiple of the narrower of its two faces' breadths",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 10.f,
         .Drag = SCALE_FACTOR},
        {.Member = &WalkMassParams::MinMembers,
         .Hint = "Members every mass keeps; shorter spines read a length nibble of 0 as one lattice step",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = 7.f,
         .Drag = FINE_INTEGER},
        {.Member = &WalkMassParams::MinContactShare,
         .Hint = "Share of a sheared member's start face that always stays on its parent",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 1.f,
         .Drag = UNIT_SHARE},
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
         .Hint = "Sideways joint shift of an offset section, as a share of the previous breadth",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 0.5f,
         .Drag = UNIT_SHARE},
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
         .Hint = "Shear travel at amplitude 15 before the contact cap, as a fraction of size",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 2.f,
         .Drag = SCALE_FACTOR},
    }};

    static constexpr TKit::FixedArray<OrderedPair<WalkMassParams>, 9> PAIRS{{
        {&WalkMassParams::SizeMin, &WalkMassParams::SizeMax},
        {&WalkMassParams::LengthMin, &WalkMassParams::LengthMax},
        {&WalkMassParams::BreadthMin, &WalkMassParams::BreadthMax},
        {&WalkMassParams::ThicknessMin, &WalkMassParams::ThicknessMax},
        {&WalkMassParams::PlinthBreadthMin, &WalkMassParams::PlinthBreadthMax},
        {&WalkMassParams::PlinthThicknessMin, &WalkMassParams::PlinthThicknessMax},
        {&WalkMassParams::LatticeStepMin, &WalkMassParams::LatticeStepMax},
        {&WalkMassParams::PeriodMin, &WalkMassParams::PeriodMax},
        {&WalkMassParams::HoldMin, &WalkMassParams::HoldMax},
    }};
};
} // namespace Lim

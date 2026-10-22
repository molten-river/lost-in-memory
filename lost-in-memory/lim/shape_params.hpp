#pragma once

#include "lim/alias.hpp"
#include "lim/capacity.hpp"
#include "lim/param_schema.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct ShapeParams
{
    TKIT_REFLECT_DECLARE(ShapeParams)
    f32 CarveChance = 0.45f;
    f32 OpeningMargin = 1.5f;
    f32 HardBlendScale = 0.15f;
    f32 TrimChance = 0.35f;
    f32 MonolithWeight = 3.f;
    f32 PortalWeight = 2.f;
    f32 RingWeight = 1.5f;
    f32 CageWeight = 1.f;
    f32 GateWeight = 1.5f;
    f32 WallFraction = 0.22f;
    f32 MinSpanFraction = 0.7f;
    f32 MaxSpanFraction = 1.1f;
    f32 MinDepthFraction = 0.25f;
    f32 MaxDepthFraction = 0.5f;
    f32 MinWindowFraction = 0.3f;
    f32 MaxWindowFraction = 0.6f;
    f32 AttachmentReach = 0.6f;
    f32 NotchFraction = 0.4f;
    f32 GrooveDepthFraction = 0.35f;
    f32 SmoothSpread = 0.8f;
    f32 SmoothSphereRadius = 0.8f;
    f32 SmoothBoxHalfWidth = 0.7f;
    f32 SmoothBoxHalfHeight = 0.5f;
    f32 SmoothBoxHalfDepth = 0.35f;
    f32 SmoothCapsuleRadius = 0.4f;
    f32 SmoothCapsuleHalfLength = 0.8f;
    f32 SmoothTorusMajorRadius = 0.8f;
    f32 SmoothTorusMinorRadius = 0.3f;
};

template <> struct ParamSchema<ShapeParams>
{
    static constexpr TKit::FixedArray<ParamSpec<ShapeParams>, 28> FIELDS{{
        {.Member = &ShapeParams::CarveChance,
         .Hint = "Chance a spare primitive cuts into the core instead of attaching",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 1.f,
         .Drag = UNIT_SHARE},
        {.Member = &ShapeParams::OpeningMargin,
         .Hint = "Smallest opening as a multiple of the player diameter",
         .Flags = BOTH_BOUNDS,
         .Min = 1.f,
         .Max = 4.f},
        {.Member = &ShapeParams::HardBlendScale,
         .Hint = "Blend radius multiplier for machined forms",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 1.f},
        {.Member = &ShapeParams::TrimChance,
         .Hint = "Chance a trimmable core gets a trim piece",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 1.f},
        {.Member = &ShapeParams::MonolithWeight,
         .Hint = "Pick weight of a solid monolith core",
         .Flags = ParamSpecFlag_Min},
        {.Member = &ShapeParams::PortalWeight,
         .Hint = "Pick weight of a framed portal core",
         .Flags = ParamSpecFlag_Min},
        {.Member = &ShapeParams::RingWeight, .Hint = "Pick weight of a ring core", .Flags = ParamSpecFlag_Min},
        {.Member = &ShapeParams::CageWeight, .Hint = "Pick weight of a lattice cage core", .Flags = ParamSpecFlag_Min},
        {.Member = &ShapeParams::GateWeight,
         .Hint = "Pick weight of a pillar and lintel gate core",
         .Flags = ParamSpecFlag_Min},
        {.Member = &ShapeParams::WallFraction,
         .Hint = "Wall thickness and snapping step, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 0.5f},
        {.Member = &ShapeParams::MinSpanFraction,
         .Hint = "Shortest core span, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::MaxSpanFraction,
         .Hint = "Longest core span, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::MinDepthFraction,
         .Hint = "Thinnest core depth, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::MaxDepthFraction,
         .Hint = "Thickest core depth, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::MinWindowFraction,
         .Hint = "Narrowest opening, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::MaxWindowFraction,
         .Hint = "Widest opening, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::AttachmentReach,
         .Hint = "Farthest an attachment sits from the core, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 2.f},
        {.Member = &ShapeParams::NotchFraction,
         .Hint = "Deepest notch in a core edge, as a share of its extent",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 0.45f},
        {.Member = &ShapeParams::GrooveDepthFraction,
         .Hint = "Groove depth as a share of the core depth",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 0.5f},
        {.Member = &ShapeParams::SmoothSpread,
         .Hint = "How far blob primitives scatter from the centre, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 2.f},
        {.Member = &ShapeParams::SmoothSphereRadius,
         .Hint = "Sphere radius, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::SmoothBoxHalfWidth,
         .Hint = "Box half width, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::SmoothBoxHalfHeight,
         .Hint = "Box half height, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::SmoothBoxHalfDepth,
         .Hint = "Box half depth, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::SmoothCapsuleRadius,
         .Hint = "Capsule radius, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::SmoothCapsuleHalfLength,
         .Hint = "Capsule half length, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::SmoothTorusMajorRadius,
         .Hint = "Torus ring radius, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
        {.Member = &ShapeParams::SmoothTorusMinorRadius,
         .Hint = "Torus tube radius, as a fraction of size",
         .Flags = BOTH_BOUNDS,
         .Min = 0.05f,
         .Max = 2.f},
    }};

    static constexpr TKit::FixedArray<OrderedPair<ShapeParams>, 3> PAIRS{{
        {&ShapeParams::MinSpanFraction, &ShapeParams::MaxSpanFraction},
        {&ShapeParams::MinDepthFraction, &ShapeParams::MaxDepthFraction},
        {&ShapeParams::MinWindowFraction, &ShapeParams::MaxWindowFraction},
    }};
};

struct ShapeLookParams
{
    TKIT_REFLECT_DECLARE(ShapeLookParams)
    u32 MeshResolution = 24;
};

template <> struct ParamSchema<ShapeLookParams>
{
    static constexpr ParamSpec<ShapeLookParams> MESH_RESOLUTION{.Member = &ShapeLookParams::MeshResolution,
                                                                .Hint = "Grid cells per axis when meshing a blob form",
                                                                .Flags = DRAGGED_BOUNDS,
                                                                .Min = f32(MIN_MESH_RESOLUTION),
                                                                .Max = f32(MAX_MESH_RESOLUTION),
                                                                .Drag = FINE_INTEGER};
    static constexpr TKit::FixedArray<ParamSpec<ShapeLookParams>, 1> FIELDS{MESH_RESOLUTION};
};
} // namespace Lim

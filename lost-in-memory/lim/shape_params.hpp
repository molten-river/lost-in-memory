#pragma once

#include "lim/alias.hpp"

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

struct ShapeLookParams
{
    TKIT_REFLECT_DECLARE(ShapeLookParams)
    u32 MeshResolution = 24;
};

[[nodiscard]] ShapeParams Sanitize(ShapeParams params);
[[nodiscard]] ShapeLookParams Sanitize(ShapeLookParams params);
} // namespace Lim

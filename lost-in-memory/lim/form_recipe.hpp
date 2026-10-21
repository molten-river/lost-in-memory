#pragma once

#include "lim/alias.hpp"
#include "lim/world_layout.hpp"

#include "tkit/container/static_array.hpp"
#include "tkit/math/tensor.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
enum class FormKind : u8
{
    Enemy,
    Solid,
    Goal
};

enum class LinkRule : u8
{
    Proximity,
    Chain
};

struct SectionMotion
{
    f32 Period;
    f32 Phase;
    f32v3 SwayDirection;
    f32 Sway;
    f32 Pulse;
};

struct SectionSlot
{
    u8 Slot;
    f32v3 RestCenter;
    f32 RestScale;
    f32 Roll;
    SectionMotion Motion;
};

struct FormPlacement
{
    f32v3 Direction;
    f32 DistanceFraction;
    f32v3 Heading;
    f32 SpeedFraction;
    f32v3 TumbleAxis;
    f32 TumbleScale;
};

struct FormRecipe
{
    FormKind Kind;
    f32 TintFraction;
    f32 Size;
    f32 ReachBias;
    bool ClosedChain;
    TKit::StaticArray<f32v2, MAX_PROFILE_VERTICES> Profile;
    TKit::StaticArray<SectionSlot, RECORD_SECTION_SLOT_COUNT> Sections;
    FormPlacement Placement;
};

struct WorldRecipe
{
    TKit::StaticArray<FormRecipe, RECORD_COUNT> Forms;
    TKit::StaticArray<u8, RECORD_COUNT> RecordIndices;
    TKit::Optional<u32> Goal;
};
} // namespace Lim

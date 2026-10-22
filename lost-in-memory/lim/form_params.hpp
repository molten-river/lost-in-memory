#pragma once

#include "lim/alias.hpp"
#include "lim/capacity.hpp"
#include "lim/param_schema.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct EnemyParams
{
    TKIT_REFLECT_DECLARE(EnemyParams)
    u32 MinCount = 6;
    u32 MaxCount = 12;
    f32 MinSize = 1.2f;
    f32 MaxSize = 3.f;
    u32 MinPrimitives = 2;
    u32 MaxPrimitives = 5;
    f32 BlendRadius = 0.6f;
    f32 MinDriftSpeed = 1.f;
    f32 MaxDriftSpeed = 5.f;
    f32 TumbleSpeed = 0.4f;
};

template <> struct ParamSchema<EnemyParams>
{
    static constexpr TKit::FixedArray<ParamSpec<EnemyParams>, 10> FIELDS{{
        {.Member = &EnemyParams::MinCount,
         .Hint = "Fewest enemies the byte stream can spawn",
         .Flags = ParamSpecFlag_Max,
         .Max = f32(MAX_ENEMIES)},
        {.Member = &EnemyParams::MaxCount,
         .Hint = "Most enemies the byte stream can spawn",
         .Flags = ParamSpecFlag_Max,
         .Max = f32(MAX_ENEMIES)},
        {.Member = &EnemyParams::MinSize, .Hint = "Smallest enemy size", .Flags = ParamSpecFlag_Min, .Min = 0.1f},
        {.Member = &EnemyParams::MaxSize, .Hint = "Largest enemy size", .Flags = ParamSpecFlag_Min, .Min = 0.1f},
        {.Member = &EnemyParams::MinPrimitives,
         .Hint = "Fewest primitives in one enemy",
         .Flags = BOTH_BOUNDS,
         .Min = 1.f,
         .Max = f32(MAX_PRIMITIVES)},
        {.Member = &EnemyParams::MaxPrimitives,
         .Hint = "Most primitives in one enemy",
         .Flags = BOTH_BOUNDS,
         .Min = 1.f,
         .Max = f32(MAX_PRIMITIVES)},
        {.Member = &EnemyParams::BlendRadius,
         .Hint = "Smooth union radius of enemies and the goal form",
         .Flags = ParamSpecFlag_Min},
        {.Member = &EnemyParams::MinDriftSpeed,
         .Hint = "Enemy drift speed at speed fraction 0",
         .Flags = ParamSpecFlag_Min},
        {.Member = &EnemyParams::MaxDriftSpeed,
         .Hint = "Enemy drift speed at speed fraction 1",
         .Flags = ParamSpecFlag_Min},
        {.Member = &EnemyParams::TumbleSpeed,
         .Hint = "Enemy spin rate before the tumble scale",
         .Flags = ParamSpecFlag_Min},
    }};

    static constexpr TKit::FixedArray<OrderedPair<EnemyParams>, 4> PAIRS{{
        {&EnemyParams::MinCount, &EnemyParams::MaxCount},
        {&EnemyParams::MinSize, &EnemyParams::MaxSize},
        {&EnemyParams::MinPrimitives, &EnemyParams::MaxPrimitives},
        {&EnemyParams::MinDriftSpeed, &EnemyParams::MaxDriftSpeed},
    }};
};

struct SolidParams
{
    TKIT_REFLECT_DECLARE(SolidParams)
    u32 MinCount = 3;
    u32 MaxCount = 7;
    f32 MinSize = 2.f;
    f32 MaxSize = 4.f;
    u32 MinPrimitives = 2;
    u32 MaxPrimitives = 4;
    f32 BlendRadius = 0.2f;
    f32 MinDriftSpeed = 0.3f;
    f32 MaxDriftSpeed = 1.5f;
    f32 TumbleSpeed = 0.15f;
};

template <> struct ParamSchema<SolidParams>
{
    static constexpr TKit::FixedArray<ParamSpec<SolidParams>, 10> FIELDS{{
        {.Member = &SolidParams::MinCount,
         .Hint = "Fewest solids the byte stream can spawn",
         .Flags = ParamSpecFlag_Max,
         .Max = f32(MAX_SOLIDS)},
        {.Member = &SolidParams::MaxCount,
         .Hint = "Most solids the byte stream can spawn",
         .Flags = ParamSpecFlag_Max,
         .Max = f32(MAX_SOLIDS)},
        {.Member = &SolidParams::MinSize, .Hint = "Smallest solid size", .Flags = ParamSpecFlag_Min, .Min = 0.1f},
        {.Member = &SolidParams::MaxSize, .Hint = "Largest solid size", .Flags = ParamSpecFlag_Min, .Min = 0.1f},
        {.Member = &SolidParams::MinPrimitives,
         .Hint = "Fewest primitives in one solid",
         .Flags = BOTH_BOUNDS,
         .Min = 1.f,
         .Max = f32(MAX_PRIMITIVES)},
        {.Member = &SolidParams::MaxPrimitives,
         .Hint = "Most primitives in one solid",
         .Flags = BOTH_BOUNDS,
         .Min = 1.f,
         .Max = f32(MAX_PRIMITIVES)},
        {.Member = &SolidParams::BlendRadius, .Hint = "Smooth union radius of solids", .Flags = ParamSpecFlag_Min},
        {.Member = &SolidParams::MinDriftSpeed,
         .Hint = "Solid drift speed at speed fraction 0",
         .Flags = ParamSpecFlag_Min},
        {.Member = &SolidParams::MaxDriftSpeed,
         .Hint = "Solid drift speed at speed fraction 1",
         .Flags = ParamSpecFlag_Min},
        {.Member = &SolidParams::TumbleSpeed,
         .Hint = "Solid spin rate before the tumble scale",
         .Flags = ParamSpecFlag_Min},
    }};

    static constexpr TKit::FixedArray<OrderedPair<SolidParams>, 4> PAIRS{{
        {&SolidParams::MinCount, &SolidParams::MaxCount},
        {&SolidParams::MinSize, &SolidParams::MaxSize},
        {&SolidParams::MinPrimitives, &SolidParams::MaxPrimitives},
        {&SolidParams::MinDriftSpeed, &SolidParams::MaxDriftSpeed},
    }};
};

struct GoalParams
{
    TKIT_REFLECT_DECLARE(GoalParams)
    f32 ShellChance = 0.3f;
    f32 Size = 1.5f;
    f32 DriftSpeed = 1.5f;
    f32 MinSpawnDistance = 15.f;
    u32 MinPrimitives = 1;
    u32 MaxPrimitives = 3;
};

template <> struct ParamSchema<GoalParams>
{
    static constexpr TKit::FixedArray<ParamSpec<GoalParams>, 6> FIELDS{{
        {.Member = &GoalParams::ShellChance,
         .Hint = "Chance the goal is a shell panel instead of a form",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 1.f},
        {.Member = &GoalParams::Size, .Hint = "Size of the goal form", .Flags = ParamSpecFlag_Min, .Min = 0.1f},
        {.Member = &GoalParams::DriftSpeed, .Hint = "Drift speed of the goal form", .Flags = ParamSpecFlag_Min},
        {.Member = &GoalParams::MinSpawnDistance,
         .Hint = "Nearest the goal form spawns to the start",
         .Flags = ParamSpecFlag_Min},
        {.Member = &GoalParams::MinPrimitives,
         .Hint = "Fewest primitives in the goal form",
         .Flags = BOTH_BOUNDS,
         .Min = 1.f,
         .Max = f32(MAX_PRIMITIVES)},
        {.Member = &GoalParams::MaxPrimitives,
         .Hint = "Most primitives in the goal form",
         .Flags = BOTH_BOUNDS,
         .Min = 1.f,
         .Max = f32(MAX_PRIMITIVES)},
    }};

    static constexpr OrderedPair<GoalParams> PRIMITIVES{&GoalParams::MinPrimitives, &GoalParams::MaxPrimitives};
    static constexpr TKit::FixedArray<OrderedPair<GoalParams>, 1> PAIRS{PRIMITIVES};
};

struct PhysicsParams
{
    TKIT_REFLECT_DECLARE(PhysicsParams)
    f32 Restitution = 1.f;
    f32 Density = 1.f;
    f32 AngularDamping = 0.f;
    u32 Substeps = 2;
    f32 WallMargin = 0.5f;
    f32 MinTumbleScale = 0.5f;
    f32 MaxTumbleScale = 1.5f;
    f32 MinSafeSpawnRadius = 8.f;
    f32 SafeSpawnFraction = 0.3f;
};

template <> struct ParamSchema<PhysicsParams>
{
    static constexpr TKit::FixedArray<ParamSpec<PhysicsParams>, 9> FIELDS{{
        {.Member = &PhysicsParams::Restitution,
         .Hint = "Bounciness of every collision; 1 keeps all energy",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 1.f,
         .Drag = UNIT_SHARE},
        {.Member = &PhysicsParams::Density,
         .Hint = "Mass per unit volume of every form",
         .Flags = ParamSpecFlag_Min,
         .Min = 1e-3f},
        {.Member = &PhysicsParams::AngularDamping,
         .Hint = "Share of spin lost per second",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 1.f},
        {.Member = &PhysicsParams::Substeps,
         .Hint = "Physics steps per frame",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = 8.f,
         .Drag = FINE_INTEGER},
        {.Member = &PhysicsParams::WallMargin,
         .Hint = "Gap kept between spawned forms and the shell",
         .Flags = ParamSpecFlag_Min},
        {.Member = &PhysicsParams::MinTumbleScale,
         .Hint = "Tumble multiplier at tumble fraction 0",
         .Flags = ParamSpecFlag_Min},
        {.Member = &PhysicsParams::MaxTumbleScale,
         .Hint = "Tumble multiplier at tumble fraction 1",
         .Flags = ParamSpecFlag_Min},
        {.Member = &PhysicsParams::MinSafeSpawnRadius,
         .Hint = "Smallest clear radius around the start",
         .Flags = ParamSpecFlag_Min},
        {.Member = &PhysicsParams::SafeSpawnFraction,
         .Hint = "Clear radius around the start, as a share of the shell radius",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 1.f},
    }};

    static constexpr OrderedPair<PhysicsParams> TUMBLE_SCALE{&PhysicsParams::MinTumbleScale,
                                                             &PhysicsParams::MaxTumbleScale};
    static constexpr TKit::FixedArray<OrderedPair<PhysicsParams>, 1> PAIRS{TUMBLE_SCALE};
};
} // namespace Lim

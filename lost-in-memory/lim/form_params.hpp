#pragma once

#include "alias/alias.hpp"
#include "tuning/param_schema.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct EnemyParams
{
    TKIT_REFLECT_DECLARE(EnemyParams)
    f32 MinDriftSpeed = 1.f;
    f32 MaxDriftSpeed = 5.f;
    f32 TumbleSpeed = 0.4f;
};

template <> struct ParamSchema<EnemyParams>
{
    static constexpr TKit::FixedArray<ParamSpec<EnemyParams>, 3> FIELDS{{
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

    static constexpr OrderedPair<EnemyParams> DRIFT_SPEED{&EnemyParams::MinDriftSpeed, &EnemyParams::MaxDriftSpeed};
    static constexpr TKit::FixedArray<OrderedPair<EnemyParams>, 1> PAIRS{DRIFT_SPEED};
};

struct SolidParams
{
    TKIT_REFLECT_DECLARE(SolidParams)
    f32 MinDriftSpeed = 0.3f;
    f32 MaxDriftSpeed = 1.5f;
    f32 TumbleSpeed = 0.15f;
};

template <> struct ParamSchema<SolidParams>
{
    static constexpr TKit::FixedArray<ParamSpec<SolidParams>, 3> FIELDS{{
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

    static constexpr OrderedPair<SolidParams> DRIFT_SPEED{&SolidParams::MinDriftSpeed, &SolidParams::MaxDriftSpeed};
    static constexpr TKit::FixedArray<OrderedPair<SolidParams>, 1> PAIRS{DRIFT_SPEED};
};

struct GoalParams
{
    TKIT_REFLECT_DECLARE(GoalParams)
    f32 DriftSpeed = 1.5f;
    f32 MinSpawnDistance = 40.f;
};

template <> struct ParamSchema<GoalParams>
{
    static constexpr TKit::FixedArray<ParamSpec<GoalParams>, 2> FIELDS{{
        {.Member = &GoalParams::DriftSpeed, .Hint = "Drift speed of the goal form", .Flags = ParamSpecFlag_Min},
        {.Member = &GoalParams::MinSpawnDistance,
         .Hint = "Nearest the goal form spawns to the start",
         .Flags = ParamSpecFlag_Min},
    }};
};

struct PhysicsParams
{
    TKIT_REFLECT_DECLARE(PhysicsParams)
    f32 Restitution = 1.f;
    f32 Density = 1.f;
    f32 AngularDamping = 0.f;
    u32 Substeps = 2;
    f32 WallMargin = 2.f;
    f32 MinTumbleScale = 0.5f;
    f32 MaxTumbleScale = 1.5f;
    f32 MinSafeSpawnRadius = 24.f;
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

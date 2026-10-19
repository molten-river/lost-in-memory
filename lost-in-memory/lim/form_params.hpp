#pragma once

#include "lim/alias.hpp"

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

[[nodiscard]] EnemyParams Sanitize(EnemyParams params);
[[nodiscard]] SolidParams Sanitize(SolidParams params);
[[nodiscard]] GoalParams Sanitize(GoalParams params);
[[nodiscard]] PhysicsParams Sanitize(PhysicsParams params);
} // namespace Lim

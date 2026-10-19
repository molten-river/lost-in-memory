#pragma once

#include "lim/alias.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct ShellParams
{
    TKIT_REFLECT_DECLARE(ShellParams)
    f32 Radius = 30.f;
    u32 Subdivisions = 4;
    f32 WaveAmplitude = 1.2f;
    f32 WaveSpeed = 0.6f;
    f32 WaveFrequency = 3.f;
    f32 LethalDensity = 0.06f;
};

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

struct GoalParams
{
    TKIT_REFLECT_DECLARE(GoalParams)
    f32 ShellChance = 0.3f;
    f32 Size = 1.5f;
    f32 DriftSpeed = 1.5f;
    f32 MinSpawnDistance = 15.f;
};

struct LevelParams
{
    TKIT_REFLECT_DECLARE(LevelParams)
    ShellParams Shell;
    EnemyParams Enemies;
    GoalParams Goal;
    f32 TextSize = 16.f;
};

/// Reads defaults.yaml; a missing file, field, malformed line or invalid value keeps the struct default for it.
[[nodiscard]] LevelParams LoadLevelParams();

/// Writes defaults.yaml through a temp file and a rename; returns whether it succeeded.
bool SaveLevelParams(const LevelParams &params);
} // namespace Lim

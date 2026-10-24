#pragma once

#include "alias/alias.hpp"
#include "lim/form_params.hpp"
#include "lim/formula_params.hpp"
#include "lim/palette.hpp"
#include "lim/player_params.hpp"
#include "lim/shell_params.hpp"
#include "tuning/param_schema.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct FeatureFlags
{
    TKIT_REFLECT_DECLARE(FeatureFlags)
    bool ShellVertexNoise = true;
};

template <> struct ParamSchema<FeatureFlags>
{
    static constexpr ParamSpec<FeatureFlags> SHELL_VERTEX_NOISE{
        .Member = &FeatureFlags::ShellVertexNoise, .Hint = "Shell moves by drifting noise instead of travelling waves"};
    static constexpr TKit::FixedArray<ParamSpec<FeatureFlags>, 1> FIELDS{SHELL_VERTEX_NOISE};
};

struct LightParams
{
    TKIT_REFLECT_DECLARE(LightParams)
    f32 AmbientIntensity = 1.f;
    f32 CoreIntensity = 5.f;
    f32 CoreRadiusScale = 1.f;
};

template <> struct ParamSchema<LightParams>
{
    static constexpr TKit::FixedArray<ParamSpec<LightParams>, 3> FIELDS{{
        {.Member = &LightParams::AmbientIntensity,
         .Hint = "Strength of the ambient light; Onyx clamps it at 1",
         .Flags = BOTH_BOUNDS,
         .Min = 0.f,
         .Max = 1.f},
        {.Member = &LightParams::CoreIntensity,
         .Hint = "Strength of the point light at the centre",
         .Flags = ParamSpecFlag_Min},
        {.Member = &LightParams::CoreRadiusScale,
         .Hint = "Point light radius as a multiple of the shell radius",
         .Flags = ParamSpecFlag_Min,
         .Min = 0.01f},
    }};
};

struct LookFeelParams
{
    TKIT_REFLECT_DECLARE(LookFeelParams)
    Palette Colors;
    ShellMotionParams ShellMotion;
    PlayerParams Player;
    LightParams Light;
    f32 FadeSeconds = 0.5f;
    f32 TextSize = 16.f;
};

template <> struct ParamSchema<LookFeelParams>
{
    static constexpr TKit::FixedArray<ParamSpec<LookFeelParams>, 2> FIELDS{{
        {.Member = &LookFeelParams::FadeSeconds,
         .Hint = "Duration of each fade in and out",
         .Flags = ParamSpecFlag_Min,
         .Min = 0.01f},
        {.Member = &LookFeelParams::TextSize,
         .Hint = "Panel text height at 1080 lines, scaled with the screen",
         .Flags = DRAGGED_BOUNDS,
         .Min = 10.f,
         .Max = 28.f,
         .Drag = TEXT_POINTS},
    }};
};

struct MechanicsParams
{
    TKIT_REFLECT_DECLARE(MechanicsParams)
    ShellParams Shell;
    EnemyParams Enemies;
    SolidParams Solids;
    GoalParams Goal;
    PhysicsParams Physics;
    RecordParams Records;
    TestBenchParams TestBench;
};

struct LevelParams
{
    TKIT_REFLECT_DECLARE(LevelParams)
    LookFeelParams LookFeel;
    MechanicsParams Mechanics;
    FeatureFlags Flags;
};

/// Reads defaults.yaml; a missing file, field, malformed line or invalid value keeps the struct default for it.
[[nodiscard]] LevelParams LoadLevelParams();

/// Writes defaults.yaml through a temp file and a rename; returns whether it succeeded.
bool SaveLevelParams(const LevelParams &params);

/// Orders and clamps every field by its schema, then fits the shell motion to the shell radius.
[[nodiscard]] LevelParams Sanitize(LevelParams params);
} // namespace Lim

#pragma once

#include "lim/alias.hpp"
#include "lim/form_params.hpp"
#include "lim/formula_params.hpp"
#include "lim/palette.hpp"
#include "lim/param_schema.hpp"
#include "lim/player_params.hpp"
#include "lim/shape_params.hpp"
#include "lim/shell_params.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct FeatureFlags
{
    TKIT_REFLECT_DECLARE(FeatureFlags)
    bool ShellVertexNoise = true;
    bool CarvedShapes = true;
    bool StructuralDecode = true;
    bool ChainLinks = false;
};

template <> struct ParamSchema<FeatureFlags>
{
    static constexpr TKit::FixedArray<ParamSpec<FeatureFlags>, 4> FIELDS{{
        {.Member = &FeatureFlags::ShellVertexNoise,
         .Hint = "Shell moves by drifting noise instead of travelling waves"},
        {.Member = &FeatureFlags::CarvedShapes, .Hint = "Byte stream builds machined forms instead of smooth blobs"},
        {.Member = &FeatureFlags::StructuralDecode,
         .Hint = "Forms come from the record layout instead of the byte stream"},
        {.Member = &FeatureFlags::ChainLinks, .Hint = "Sections link in slot order instead of by proximity"},
    }};
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
        {.Member = &LightParams::AmbientIntensity, .Hint = "Strength of the ambient light", .Flags = ParamSpecFlag_Min},
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
    ShapeLookParams Shapes;
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
    ShapeParams Shapes;
    EnemyParams Enemies;
    SolidParams Solids;
    GoalParams Goal;
    PhysicsParams Physics;
    RecordParams Records;
    GraphParams Graph;
    EvolutionParams Evolution;
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

/// Orders and clamps every field by its schema, then fits form sizes and shell motion to the shell radius.
[[nodiscard]] LevelParams Sanitize(LevelParams params);
} // namespace Lim

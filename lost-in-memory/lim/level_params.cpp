#include "lim/level_params.hpp"
#include "lim/params_reflection.hpp"

#include "tuning/params_file.hpp"
#include "tuning/schema_rules.hpp"

#include "tkit/math/math.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/utils/logging.hpp"

namespace Lim
{
namespace
{
constexpr f32 MAX_WAVE_AMPLITUDE_FRACTION = 0.4f;
constexpr f32 MAX_FORM_SIZE_FRACTION = 0.25f;

void FitToRadius(LevelParams &params)
{
    const f32 radius = params.Mechanics.Shell.Radius;
    const f32 maxAmplitude = MAX_WAVE_AMPLITUDE_FRACTION * radius;
    ShellMotionParams &motion = params.LookFeel.ShellMotion;
    motion.WaveAmplitude = TKit::Math::Min(motion.WaveAmplitude, maxAmplitude);
    motion.NoiseAmplitude = TKit::Math::Min(motion.NoiseAmplitude, maxAmplitude);

    const f32 maxSize = MAX_FORM_SIZE_FRACTION * radius;
    MechanicsParams &mechanics = params.Mechanics;
    mechanics.Enemies.MinSize = TKit::Math::Min(mechanics.Enemies.MinSize, maxSize);
    mechanics.Enemies.MaxSize = TKit::Math::Min(mechanics.Enemies.MaxSize, maxSize);
    mechanics.Solids.MinSize = TKit::Math::Min(mechanics.Solids.MinSize, maxSize);
    mechanics.Solids.MaxSize = TKit::Math::Min(mechanics.Solids.MaxSize, maxSize);
    mechanics.Goal.Size = TKit::Math::Min(mechanics.Goal.Size, maxSize);

    RecordParams &records = mechanics.Records;
    records.EnemySizeMin = TKit::Math::Min(records.EnemySizeMin, maxSize);
    records.EnemySizeMax = TKit::Math::Min(records.EnemySizeMax, maxSize);
    records.SolidSizeMin = TKit::Math::Min(records.SolidSizeMin, maxSize);
    records.SolidSizeMax = TKit::Math::Min(records.SolidSizeMax, maxSize);
    records.GoalSize = TKit::Math::Min(records.GoalSize, maxSize);
}

static_assert(IsSchemaSound<LevelParams>(), "Every tuning field needs exactly one ParamSpec in its struct's schema");
} // namespace

LevelParams LoadLevelParams()
{
    return LoadParams<LevelParams>(LIM_DEFAULTS_PATH);
}

bool SaveLevelParams(const LevelParams &params)
{
    return SaveParams<LevelParams>(LIM_DEFAULTS_PATH, params);
}

LevelParams Sanitize(LevelParams params)
{
    TKIT_ASSERT(AreDefaultsWithinSchema<LevelParams>(),
                "[LIM][LEVEL_PARAMS] A default breaks its pair order or bounds");
    ApplySchema(params);
    FitToRadius(params);
    return params;
}
} // namespace Lim

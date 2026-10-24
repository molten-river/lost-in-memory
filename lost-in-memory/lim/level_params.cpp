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

void FitToRadius(LevelParams &params)
{
    const f32 maxAmplitude = MAX_WAVE_AMPLITUDE_FRACTION * params.Mechanics.Shell.Radius;
    ShellMotionParams &motion = params.LookFeel.ShellMotion;
    motion.WaveAmplitude = TKit::Math::Min(motion.WaveAmplitude, maxAmplitude);
    motion.NoiseAmplitude = TKit::Math::Min(motion.NoiseAmplitude, maxAmplitude);
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

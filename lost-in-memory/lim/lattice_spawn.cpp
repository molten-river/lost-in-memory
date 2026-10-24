#include "lim/lattice_spawn.hpp"

#include "lim/sphere_direction.hpp"
#include "lim/world_layout.hpp"

namespace Lim
{
namespace
{
constexpr f32 GOLDEN_ANGLE_TURNS = 0.381966f;
constexpr f32 DEPTH_STEP = 0.7548777f;
constexpr f32 MAX_JITTER_SLOTS = 0.5f;

/// A shift of at most MAX_JITTER_SLOTS slots either way, from one placement byte.
[[nodiscard]] f32 ReadJitter(const TKit::Span<const std::byte> record, const usize index)
{
    return MAX_JITTER_SLOTS * (2.f * ReadRecordFraction(record, index) - 1.f);
}
} // namespace

/// The azimuth jitter is a spatial half cell: a cell spans sqrt(4 pi / count) radians, so 1 / sqrt(pi count) turns
f32v3 LatticeSpawn::Place(const TKit::Span<const std::byte> record, const u32 slot, const u32 count,
                          const SpawnBand &band) const
{
    const f32 k = f32(slot);
    const f32 cells = f32(count);
    const f32 height = 1.f - (k + 0.5f + ReadJitter(record, RECORD_SPAWN_Z_BYTE)) / cells;
    const f32 z = 2.f * height - 1.f;
    const f32 ring =
        TKit::Math::Max(TKit::Math::SquareRoot(TKit::Math::Max(1.f - z * z, 0.f)), TKit::Math::SquareRoot(1.f / cells));
    const f32 cellTurns = 1.f / TKit::Math::SquareRoot(TKit::Math::Pi() * cells);
    const f32 turns = k * GOLDEN_ANGLE_TURNS + ReadJitter(record, RECORD_SPAWN_AZIMUTH_BYTE) * cellTurns / ring;
    const f32 depth = TKit::Math::Modulo(0.5f + (k + ReadJitter(record, RECORD_SPAWN_DISTANCE_BYTE)) * DEPTH_STEP, 1.f);
    return ToDirection(turns, height) * band.GetVolumeRadius(depth);
}
} // namespace Lim

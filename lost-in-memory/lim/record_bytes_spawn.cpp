#include "lim/record_bytes_spawn.hpp"

#include "lim/sphere_direction.hpp"
#include "lim/world_layout.hpp"

namespace Lim
{
f32v3 RecordBytesSpawn::Place(const TKit::Span<const std::byte> record, const u32, const u32,
                              const SpawnBand &band) const
{
    const f32 distance =
        TKit::Math::LinearLerp(band.Nearest, band.Farthest, ReadRecordFraction(record, RECORD_SPAWN_DISTANCE_BYTE));
    return ToDirection(ReadRecordFraction(record, RECORD_SPAWN_AZIMUTH_BYTE),
                       ReadRecordFraction(record, RECORD_SPAWN_Z_BYTE)) *
           distance;
}
} // namespace Lim

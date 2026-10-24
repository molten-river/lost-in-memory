#include "lim/scatter_spawn.hpp"

#include "lim/sphere_direction.hpp"

namespace Lim
{
namespace
{
constexpr u64 FNV_OFFSET = 0xCBF29CE484222325ULL;
constexpr u64 FNV_PRIME = 0x100000001B3ULL;
constexpr u64 SPLITMIX_STEP = 0x9E3779B97F4A7C15ULL;
constexpr u64 SPLITMIX_FIRST = 0xBF58476D1CE4E5B9ULL;
constexpr u64 SPLITMIX_SECOND = 0x94D049BB133111EBULL;
constexpr u32 FRACTION_BITS = 24;
constexpr u32 SLOT_BYTES = 4;
constexpr u32 BITS_PER_BYTE = 8;
constexpr u64 BYTE_MASK = 0xFF;
} // namespace

/// FNV-1a over the record and the slot seeds splitmix64, whose top bits give three fractions in a fixed order.
f32v3 ScatterSpawn::Place(const TKit::Span<const std::byte> record, const u32 slot, const u32,
                          const SpawnBand &band) const
{
    u64 state = FNV_OFFSET;
    for (const std::byte byte : record)
        state = (state ^ std::to_integer<u64>(byte)) * FNV_PRIME;
    for (u32 i = 0; i < SLOT_BYTES; ++i)
        state = (state ^ ((u64(slot) >> (i * BITS_PER_BYTE)) & BYTE_MASK)) * FNV_PRIME;

    const auto nextFraction = [&state] {
        state += SPLITMIX_STEP;
        u64 mixed = (state ^ (state >> 30)) * SPLITMIX_FIRST;
        mixed = (mixed ^ (mixed >> 27)) * SPLITMIX_SECOND;
        mixed ^= mixed >> 31;
        return f32(mixed >> (64 - FRACTION_BITS)) / f32(1ULL << FRACTION_BITS);
    };
    const f32 turns = nextFraction();
    const f32 height = nextFraction();
    const f32 depth = nextFraction();
    return ToDirection(turns, height) * band.GetVolumeRadius(depth);
}
} // namespace Lim

#include "lim/world_decoder.hpp"

#include "lim/world_layout.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/preprocessor/system.hpp"
#include <cstddef>

namespace Lim
{
namespace
{
constexpr u32 ANGLE_TABLE_SIZE = 256;
constexpr u32 ANGLE_TABLE_STEPS_PER_NIBBLE = ANGLE_TABLE_SIZE / 16;
constexpr u32 SINE_SERIES_TERMS = 6;

constexpr f64 ComputeSineSeries(const f64 angle)
{
    const f64 square = angle * angle;
    f64 term = angle;
    f64 sum = angle;
    for (u32 i = 1; i <= SINE_SERIES_TERMS; ++i)
    {
        term *= -square / f64((2 * i) * (2 * i + 1));
        sum += term;
    }
    return sum;
}

constexpr f64 ComputeCosineSeries(const f64 angle)
{
    const f64 square = angle * angle;
    f64 term = 1.0;
    f64 sum = 1.0;
    for (u32 i = 1; i <= SINE_SERIES_TERMS; ++i)
    {
        term *= -square / f64((2 * i - 1) * (2 * i));
        sum += term;
    }
    return sum;
}

struct AngleTable
{
    TKit::FixedArray<f32, ANGLE_TABLE_SIZE> Cosine{};
    TKit::FixedArray<f32, ANGLE_TABLE_SIZE> Sine{};
};

/// Rotates a unit vector in steps of one turn fraction, never calling into libm.
constexpr AngleTable BuildAngleTable()
{
    const f64 stepAngle = 2.0 * TKit::Math::Pi<f64>() / f64(ANGLE_TABLE_SIZE);
    const f64 stepCosine = ComputeCosineSeries(stepAngle);
    const f64 stepSine = ComputeSineSeries(stepAngle);

    AngleTable table{};
    f64 cosine = 1.0;
    f64 sine = 0.0;
    for (u32 i = 0; i < ANGLE_TABLE_SIZE; ++i)
    {
        table.Cosine[i] = f32(cosine);
        table.Sine[i] = f32(sine);
        const f64 nextCosine = cosine * stepCosine - sine * stepSine;
        const f64 nextSine = sine * stepCosine + cosine * stepSine;
        cosine = nextCosine;
        sine = nextSine;
    }
    return table;
}

constexpr AngleTable ANGLE_TABLE = BuildAngleTable();

[[nodiscard]] u8 ReadByte(const TKit::Span<const std::byte> bytes, const usize index)
{
    return std::to_integer<u8>(bytes[index]);
}

[[nodiscard]] u8 ReadNibble(const TKit::Span<const std::byte> bytes, const usize baseByte, const usize nibbleIndex)
{
    const u8 byte = ReadByte(bytes, baseByte + nibbleIndex / 2);
    return (nibbleIndex % 2 == 0) ? (byte & 0x0F) : (byte >> 4);
}

[[nodiscard]] f32v3 Direction(const u8 azimuthIndex, const f32 z)
{
    const f32 ring = TKit::Math::SquareRoot(TKit::Math::Max(1.f - z * z, 0.f));
    return f32v3{ring * ANGLE_TABLE.Cosine[azimuthIndex], ring * ANGLE_TABLE.Sine[azimuthIndex], z};
}

[[nodiscard]] u8 NibbleAzimuthIndex(const u8 nibble)
{
    return u8(u32(nibble) * ANGLE_TABLE_STEPS_PER_NIBBLE);
}

[[nodiscard]] f32 NibbleUnitRange(const u8 nibble)
{
    return TKit::Math::LinearLerp(-1.f, 1.f, f32(nibble) / 15.f);
}

[[nodiscard]] f32 ByteUnitRange(const u8 byte)
{
    return TKit::Math::LinearLerp(-1.f, 1.f, f32(byte) / 255.f);
}

[[nodiscard]] f32 ByteFraction(const u8 byte)
{
    return f32(byte) / 255.f;
}

[[nodiscard]] f32 QuantizeAxis(const f32 size, const u8 nibble, const f32 latticeStep)
{
    const f32 fraction = (2.f * f32(nibble) - 15.f) / 15.f;
    return size * TKit::Math::Round(fraction / latticeStep) * latticeStep;
}

[[nodiscard]] f32 ComputeTintFraction(const u8 tag, const u32 lethalTagMax)
{
    if (tag <= lethalTagMax)
        return f32(i32(tag) - 1) / TKit::Math::Max(f32(lethalTagMax) - 1.f, 1.f);
    return f32(i32(tag) - i32(lethalTagMax) - 1) / TKit::Math::Max(254.f - f32(lethalTagMax), 1.f);
}

[[nodiscard]] f32 ComputeSize(const FormKind kind, const RecordParams &records, const u8 sizeByte)
{
    const f32 fraction = ByteFraction(sizeByte);
    switch (kind)
    {
    case FormKind::Enemy:
        return TKit::Math::LinearLerp(records.EnemySizeMin, records.EnemySizeMax, fraction);
    case FormKind::Solid:
        return TKit::Math::LinearLerp(records.SolidSizeMin, records.SolidSizeMax, fraction);
    case FormKind::Goal:
        return records.GoalSize;
    }
    TKIT_UNREACHABLE();
}

[[nodiscard]] f32 CrossProduct(const f32v2 &origin, const f32v2 &a, const f32v2 &b)
{
    return (a[0] - origin[0]) * (b[1] - origin[1]) - (a[1] - origin[1]) * (b[0] - origin[0]);
}

void SortPointsByCoordinate(TKit::FixedArray<f32v2, MAX_PROFILE_VERTICES> &points, const u32 count)
{
    for (u32 i = 1; i < count; ++i)
    {
        const f32v2 key = points[i];
        u32 j = i;
        while (j > 0 && (points[j - 1][0] > key[0] || (points[j - 1][0] == key[0] && points[j - 1][1] > key[1])))
        {
            points[j] = points[j - 1];
            --j;
        }
        points[j] = key;
    }
}

void BuildMonotoneChain(const TKit::FixedArray<f32v2, MAX_PROFILE_VERTICES> &points, const u32 count,
                        const bool descending, TKit::StaticArray<f32v2, MAX_PROFILE_VERTICES> &chain)
{
    for (u32 step = 0; step < count; ++step)
    {
        const f32v2 &point = points[descending ? count - 1 - step : step];
        while (chain.GetSize() >= 2 &&
               CrossProduct(chain[chain.GetSize() - 2], chain[chain.GetSize() - 1], point) <= 0.f)
            chain.Pop();
        chain.Append(point);
    }
}

/// The 2D convex hull of up to MAX_PROFILE_VERTICES points, counter-clockwise, by Andrew's monotone chain.
[[nodiscard]] TKit::StaticArray<f32v2, MAX_PROFILE_VERTICES> BuildProfileHull(
    TKit::FixedArray<f32v2, MAX_PROFILE_VERTICES> points, const u32 count)
{
    SortPointsByCoordinate(points, count);

    TKit::StaticArray<f32v2, MAX_PROFILE_VERTICES> lower{};
    BuildMonotoneChain(points, count, false, lower);
    TKit::StaticArray<f32v2, MAX_PROFILE_VERTICES> upper{};
    BuildMonotoneChain(points, count, true, upper);

    TKit::StaticArray<f32v2, MAX_PROFILE_VERTICES> hull{};
    for (u32 i = 0; i + 1 < lower.GetSize(); ++i)
        hull.Append(lower[i]);
    for (u32 i = 0; i + 1 < upper.GetSize(); ++i)
        hull.Append(upper[i]);
    return hull;
}

[[nodiscard]] TKit::StaticArray<f32v2, MAX_PROFILE_VERTICES> DecodeProfile(const TKit::Span<const std::byte> record,
                                                                           const f32 size, const u32 vertexCount,
                                                                           const RecordParams &records)
{
    TKit::FixedArray<f32v2, MAX_PROFILE_VERTICES> points{};
    for (u32 k = 0; k < vertexCount; ++k)
    {
        const u8 nibble = ReadNibble(record, RECORD_PROFILE_BYTE, k);
        const f32 radius =
            size * TKit::Math::LinearLerp(records.ProfileRadiusMin, records.ProfileRadiusMax, f32(nibble) / 15.f);
        const u32 angleIndex =
            u32(TKit::Math::Round(f32(k) * f32(ANGLE_TABLE_SIZE) / f32(vertexCount))) % ANGLE_TABLE_SIZE;
        points[k] = radius * f32v2{ANGLE_TABLE.Cosine[angleIndex], ANGLE_TABLE.Sine[angleIndex]};
    }
    return BuildProfileHull(points, vertexCount);
}

[[nodiscard]] SectionMotion DecodeSectionMotion(const TKit::Span<const std::byte> record, const usize offset,
                                                const f32 size, const EvolutionParams &evolution)
{
    const u8 periodPhase = ReadByte(record, offset);
    const u8 swayAxis = ReadByte(record, offset + 1);
    const u8 swayPulse = ReadByte(record, offset + 2);

    const f32 period = TKit::Math::LinearLerp(evolution.PeriodMin, evolution.PeriodMax, f32(periodPhase >> 4) / 15.f);
    const f32 phase = f32(periodPhase & 0x0F) / 16.f;

    const f32v3 swayDirection = Direction(NibbleAzimuthIndex(swayAxis >> 4), NibbleUnitRange(swayAxis & 0x0F));

    const f32 sway = size * evolution.SwayMax * f32(swayPulse >> 4) / 15.f;
    const f32 pulse = evolution.PulseMax * f32(swayPulse & 0x0F) / 15.f;
    return SectionMotion{
        .Period = period, .Phase = phase, .SwayDirection = swayDirection, .Sway = sway, .Pulse = pulse};
}

[[nodiscard]] TKit::StaticArray<SectionSlot, RECORD_SECTION_SLOT_COUNT> DecodeSections(
    const TKit::Span<const std::byte> record, const f32 size, const f32 latticeStep, const RecordParams &records,
    const EvolutionParams &evolution)
{
    TKit::StaticArray<SectionSlot, RECORD_SECTION_SLOT_COUNT> sections{};
    for (u32 slot = 0; slot < RECORD_SECTION_SLOT_COUNT; ++slot)
    {
        const usize sectionOffset = RECORD_SECTIONS_BYTE + slot * RECORD_SECTION_SLOT_BYTES;
        const u8 xy = ReadByte(record, sectionOffset);
        const u8 zScale = ReadByte(record, sectionOffset + 1);
        const u8 roll = ReadByte(record, sectionOffset + 2);

        const u32 scaleNibble = zScale & 0x0F;
        if (scaleNibble == 0)
            continue;

        const f32v3 restCenter{QuantizeAxis(size, xy >> 4, latticeStep), QuantizeAxis(size, xy & 0x0F, latticeStep),
                               QuantizeAxis(size, zScale >> 4, latticeStep)};
        const f32 restScale =
            TKit::Math::LinearLerp(records.SectionScaleMin, records.SectionScaleMax, f32(scaleNibble - 1) / 14.f);

        const usize evolutionOffset = RECORD_EVOLUTIONS_BYTE + slot * RECORD_EVOLUTION_SLOT_BYTES;
        sections.Append(SectionSlot{.Slot = u8(slot),
                                    .RestCenter = restCenter,
                                    .RestScale = restScale,
                                    .Roll = f32(roll) / f32(ANGLE_TABLE_SIZE),
                                    .Motion = DecodeSectionMotion(record, evolutionOffset, size, evolution)});
    }
    return sections;
}

[[nodiscard]] FormPlacement DecodePlacement(const TKit::Span<const std::byte> record)
{
    const f32v3 direction =
        Direction(ReadByte(record, RECORD_SPAWN_AZIMUTH_BYTE), ByteUnitRange(ReadByte(record, RECORD_SPAWN_Z_BYTE)));
    const f32 distanceFraction = ByteFraction(ReadByte(record, RECORD_SPAWN_DISTANCE_BYTE));

    const f32v3 heading = Direction(ReadByte(record, RECORD_HEADING_AZIMUTH_BYTE),
                                    ByteUnitRange(ReadByte(record, RECORD_HEADING_Z_BYTE)));
    const f32 speedFraction = ByteFraction(ReadByte(record, RECORD_SPEED_BYTE));

    const u8 tumbleByte = ReadByte(record, RECORD_TUMBLE_AXIS_BYTE);
    const f32v3 tumbleAxis = Direction(NibbleAzimuthIndex(tumbleByte >> 4), NibbleUnitRange(tumbleByte & 0x0F));
    const f32 tumbleScale = ByteFraction(ReadByte(record, RECORD_TUMBLE_SCALE_BYTE));

    return FormPlacement{.Direction = direction,
                         .DistanceFraction = distanceFraction,
                         .Heading = heading,
                         .SpeedFraction = speedFraction,
                         .TumbleAxis = tumbleAxis,
                         .TumbleScale = tumbleScale};
}
} // namespace

WorldRecipe DecodeWorld(const TKit::Span<const std::byte> region, const RecordParams &records,
                        const EvolutionParams &evolution)
{
    TKit::FixedArray<bool, RECORD_COUNT> empty{};
    for (u32 i = 0; i < RECORD_COUNT; ++i)
        empty[i] = ReadByte(region, GENOME_BYTES + i * RECORD_BYTES + RECORD_TAG_BYTE) == 0;

    TKit::Optional<u32> goalRecord = TKit::Optional<u32>::None();
    const u32 seed = u32(ReadByte(region, GENOME_GOAL_BYTE)) % RECORD_COUNT;
    for (u32 step = 0; step < RECORD_COUNT; ++step)
    {
        const u32 index = (seed + step) % RECORD_COUNT;
        if (!empty[index])
        {
            goalRecord = index;
            break;
        }
    }

    WorldRecipe recipe{};
    for (u32 i = 0; i < RECORD_COUNT; ++i)
    {
        if (empty[i])
            continue;

        const TKit::Span<const std::byte> record(region.GetData() + GENOME_BYTES + i * RECORD_BYTES, RECORD_BYTES);
        const bool isGoal = goalRecord && *goalRecord == i;
        const FormKind kind = isGoal                                                           ? FormKind::Goal
                              : u32(ReadByte(record, RECORD_TAG_BYTE)) <= records.LethalTagMax ? FormKind::Enemy
                                                                                               : FormKind::Solid;

        recipe.Forms.Append(DecodeRecord(record, kind, records, evolution));
        recipe.RecordIndices.Append(u8(i));
        if (isGoal)
            recipe.Goal = u32(recipe.Forms.GetSize() - 1);
    }
    return recipe;
}

FormRecipe DecodeRecord(const TKit::Span<const std::byte> record, const FormKind kind, const RecordParams &records,
                        const EvolutionParams &evolution)
{
    const u8 tag = ReadByte(record, RECORD_TAG_BYTE);
    const f32 tintFraction = ComputeTintFraction(tag, records.LethalTagMax);
    const f32 size = ComputeSize(kind, records, ReadByte(record, RECORD_SIZE_BYTE));

    const u8 shapeByte = ReadByte(record, RECORD_SHAPE_BYTE);
    const u32 vertexCount = 3 + u32(TKit::Math::Round(f32(shapeByte & 0x07) * 5.f / 7.f));
    const f32 latticeStep =
        TKit::Math::LinearLerp(records.LatticeStepMin, records.LatticeStepMax, f32((shapeByte >> 3) & 0x1F) / 31.f);

    const u8 reachByte = ReadByte(record, RECORD_REACH_BIAS_BYTE);
    const f32 reachBias = TKit::Math::LinearLerp(records.ReachBiasMin, records.ReachBiasMax, ByteFraction(reachByte));
    const bool closedChain = (reachByte & RECORD_CLOSED_CHAIN_BIT) != 0;

    return FormRecipe{.Kind = kind,
                      .TintFraction = tintFraction,
                      .Size = size,
                      .ReachBias = reachBias,
                      .ClosedChain = closedChain,
                      .Profile = DecodeProfile(record, size, vertexCount, records),
                      .Sections = DecodeSections(record, size, latticeStep, records, evolution),
                      .Placement = DecodePlacement(record)};
}
} // namespace Lim

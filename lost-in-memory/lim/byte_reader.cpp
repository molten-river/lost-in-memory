#include "lim/byte_reader.hpp"
#include "tkit/math/math.hpp"

namespace Lim
{
namespace
{
constexpr u64 STATE_SEED = 0x243F6A8885A308D3ULL;
constexpr u64 MIX_INCREMENT = 0x9E3779B97F4A7C15ULL;
constexpr u64 MIX_MULTIPLIER_A = 0xBF58476D1CE4E5B9ULL;
constexpr u64 MIX_MULTIPLIER_B = 0x94D049BB133111EBULL;
constexpr usize WORD_BYTES = 8;
constexpr f32 UNIT_SCALE = 1.f / 16777216.f;
constexpr f32 TWO_PI = 2.f * TKit::Math::Pi<f32>();
} // namespace

ByteReader::ByteReader(const TKit::Span<const std::byte> bytes) : m_Bytes(bytes), m_State(STATE_SEED)
{
}

u32 ByteReader::NextU32()
{
    return u32(NextWord() >> 32);
}

f32 ByteReader::NextUnit()
{
    return f32(NextU32() >> 8) * UNIT_SCALE;
}

f32 ByteReader::NextRange(const f32 min, const f32 max)
{
    return min + (max - min) * NextUnit();
}

u32 ByteReader::NextIndex(const u32 count)
{
    return u32((u64(NextU32()) * count) >> 32);
}

u32 ByteReader::NextCount(const u32 min, const u32 max)
{
    if (min >= max)
        return min;
    return min + NextIndex(max - min + 1);
}

bool ByteReader::NextChance(const f32 probability)
{
    return NextUnit() < probability;
}

f32v3 ByteReader::NextDirection()
{
    const f32 z = NextRange(-1.f, 1.f);
    const f32 azimuth = NextUnit() * TWO_PI;
    const f32 ring = TKit::Math::SquareRoot(1.f - z * z);
    return f32v3(ring * TKit::Math::Cosine(azimuth), ring * TKit::Math::Sine(azimuth), z);
}

f32q ByteReader::NextRotation()
{
    const f32 first = NextUnit();
    const f32 second = NextUnit() * TWO_PI;
    const f32 third = NextUnit() * TWO_PI;
    const f32 outer = TKit::Math::SquareRoot(1.f - first);
    const f32 inner = TKit::Math::SquareRoot(first);
    return f32q(inner * TKit::Math::Cosine(third), outer * TKit::Math::Sine(second), outer * TKit::Math::Cosine(second),
                inner * TKit::Math::Sine(third));
}

u64 ByteReader::NextWord()
{
    u64 word = 0;
    const usize size = m_Bytes.GetSize();
    for (usize i = 0; size != 0 && i < WORD_BYTES; ++i)
    {
        word |= u64(std::to_integer<u8>(m_Bytes[m_Cursor])) << (8 * i);
        m_Cursor = (m_Cursor + 1) % size;
    }
    m_State ^= word;
    m_State += MIX_INCREMENT;
    u64 mixed = m_State;
    mixed = (mixed ^ (mixed >> 30)) * MIX_MULTIPLIER_A;
    mixed = (mixed ^ (mixed >> 27)) * MIX_MULTIPLIER_B;
    return mixed ^ (mixed >> 31);
}
} // namespace Lim

#include "lim/region.hpp"
#include "syscalls/syscalls.hpp"
#include <random>

namespace Lim
{
namespace
{
constexpr u32 MAX_ATTEMPTS = 64;
constexpr usize MIN_NON_ZERO_PERCENT = 25;
constexpr usize MIN_DISTINCT_VALUES = 32;
constexpr usize BYTE_VALUES = 256;

[[nodiscard]] bool IsVaried(const TKit::Span<const std::byte> bytes)
{
    TKit::FixedArray<bool, BYTE_VALUES> seen{};
    usize nonZero = 0;
    usize distinct = 0;
    for (usize i = 0; i < bytes.GetSize(); ++i)
    {
        const u8 byte = std::to_integer<u8>(bytes[i]);
        nonZero += byte != 0;
        distinct += !seen[byte];
        seen[byte] = true;
    }
    return nonZero * 100 >= bytes.GetSize() * MIN_NON_ZERO_PERCENT && distinct >= MIN_DISTINCT_VALUES;
}

[[nodiscard]] bool IsNonZero(const TKit::Span<const std::byte> bytes)
{
    for (const std::byte byte : bytes)
        if (byte != std::byte{0})
            return true;
    return false;
}

[[nodiscard]] bool PassesFilter(const Region::Filter filter, const TKit::Span<const std::byte> bytes)
{
    switch (filter)
    {
    case Region::Filter::Varied:
        return IsVaried(bytes);
    case Region::Filter::NonZero:
        return IsNonZero(bytes);
    }
    return false;
}
} // namespace

TKit::Optional<Region> Region::Capture(const Filter filter)
{
    std::mt19937_64 engine{std::random_device{}()};
    Region region;
    for (u32 attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
    {
        const TKit::Optional<u64> address =
            Syscalls::ReadProcessMemorySnapshot(TKit::Span<std::byte>(region.m_Bytes), engine());
        if (address && PassesFilter(filter, region.GetBytes()))
        {
            region.m_Address = address.GetValue();
            return region;
        }
    }
    return TKit::Optional<Region>::None();
}

u64 Region::GetAddress() const
{
    return m_Address;
}

TKit::Span<const std::byte> Region::GetBytes() const
{
    return TKit::Span<const std::byte>(m_Bytes.GetData(), REGION_BYTES);
}
} // namespace Lim

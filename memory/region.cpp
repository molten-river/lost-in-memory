#include "memory/region.hpp"
#include "syscalls/syscalls.hpp"
#include <random>

namespace Lim
{
namespace
{
constexpr u32 MAX_ATTEMPTS = 64;

[[nodiscard]] bool IsNonZero(const TKit::Span<const std::byte> bytes)
{
    for (const std::byte byte : bytes)
        if (byte != std::byte{0})
            return true;
    return false;
}
} // namespace

TKit::Optional<Region> Region::Capture()
{
    std::mt19937_64 engine{std::random_device{}()};
    Region region;
    for (u32 attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
    {
        const TKit::Optional<u64> address =
            Syscalls::ReadProcessMemorySnapshot(TKit::Span<std::byte>(region.m_Bytes), engine());
        if (address && IsNonZero(region.GetBytes()))
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

#pragma once

#include "lim/alias.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/utils/optional.hpp"
#include <cstddef>

namespace Lim
{
inline constexpr usize REGION_BYTES = 4096;

class Region
{
  public:
    enum class Filter : u8
    {
        Varied,
        NonZero
    };

    /// Snapshots live process memory, retrying until a region passing the filter is found.
    [[nodiscard]] static TKit::Optional<Region> Capture(Filter filter);

    [[nodiscard]] u64 GetAddress() const;
    [[nodiscard]] TKit::Span<const std::byte> GetBytes() const;

  private:
    u64 m_Address = 0;
    TKit::FixedArray<std::byte, REGION_BYTES> m_Bytes{};
};
} // namespace Lim

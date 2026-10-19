#pragma once

#include "lim/alias.hpp"
#include "tkit/container/span.hpp"
#include "tkit/math/quaternion.hpp"
#include "tkit/math/tensor.hpp"
#include <cstddef>

namespace Lim
{
class ByteReader
{
  public:
    explicit ByteReader(TKit::Span<const std::byte> bytes);

    [[nodiscard]] u32 NextU32();
    [[nodiscard]] f32 NextUnit();
    [[nodiscard]] f32 NextRange(f32 min, f32 max);
    [[nodiscard]] u32 NextIndex(u32 count);
    [[nodiscard]] u32 NextCount(u32 min, u32 max);
    [[nodiscard]] bool NextChance(f32 probability);
    [[nodiscard]] f32v3 NextDirection();
    [[nodiscard]] f32q NextRotation();

  private:
    [[nodiscard]] u64 NextWord();

    TKit::Span<const std::byte> m_Bytes;
    usize m_Cursor = 0;
    u64 m_State;
};
} // namespace Lim

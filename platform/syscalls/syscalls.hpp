#pragma once

#include "tkit/utils/alias.hpp"

#include "tkit/container/span.hpp"
#include "tkit/utils/optional.hpp"

#include <cstddef>

namespace Syscalls
{
using namespace TKit::Alias;

/// Copies buffer.GetSize() bytes from a readable committed region of this process chosen by pick; returns its address.
[[nodiscard]] TKit::Optional<u64> ReadProcessMemorySnapshot(TKit::Span<std::byte> buffer, u64 pick);
} // namespace Syscalls

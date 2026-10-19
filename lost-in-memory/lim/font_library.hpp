#pragma once

#include "lim/alias.hpp"

#include "onyx/handle.hpp"

#include "tkit/utils/optional.hpp"

namespace Lim
{
/// Registers the embedded font with Onyx on first use from the main thread and caches the resource.
class FontLibrary
{
  public:
    [[nodiscard]] Onyx::Resource Get();

  private:
    TKit::Optional<Onyx::Resource> m_Font{};
};
} // namespace Lim

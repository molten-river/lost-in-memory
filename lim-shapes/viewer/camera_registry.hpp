#pragma once

#include "alias/alias.hpp"
#include "viewer/camera_strategy.hpp"
#include "viewer/fly_camera.hpp"
#include "viewer/orbit_camera.hpp"

#include "tkit/container/fixed_array.hpp"

namespace Lim
{
/// Owns one instance of every camera strategy and hands out the one a kind names.
class CameraRegistry
{
  public:
    enum class Kind : u8
    {
        Fly,
        Orbit
    };

    static constexpr u32 KIND_COUNT = u32(Kind::Orbit) + 1;

    CameraRegistry() = default;

    CameraRegistry(const CameraRegistry &) = delete;
    CameraRegistry &operator=(const CameraRegistry &) = delete;

    [[nodiscard]] CameraStrategy &Get(Kind kind);

  private:
    FlyCamera m_Fly{};
    OrbitCamera m_Orbit{};
    TKit::FixedArray<CameraStrategy *, KIND_COUNT> m_Strategies{&m_Fly, &m_Orbit};
};
} // namespace Lim

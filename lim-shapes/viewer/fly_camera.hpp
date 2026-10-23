#pragma once

#include "alias/alias.hpp"
#include "viewer/camera_strategy.hpp"

#include "tkit/math/quaternion.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
/// Free flight with inertia: mouse look on a locked cursor, WASD, Space and LeftControl to move, Q and E to roll.
class FlyCamera final : public CameraStrategy
{
  public:
    void Engage(const Onyx::Transform<Onyx::D3> &view, const CameraFrame &frame) override;
    void Update(const CameraFrame &frame) override;
    void ApplyTo(Onyx::Camera<Onyx::D3> &camera) const override;
    [[nodiscard]] bool LocksCursor() const override;
    [[nodiscard]] TKit::StringView GetName() const override;

  private:
    f32v3 m_Position{0.f};
    f32v3 m_Velocity{0.f};
    f32q m_Orientation = Onyx::RotType<Onyx::D3>::Identity;
    TKit::Optional<f32v2> m_LastCursor{};
};
} // namespace Lim

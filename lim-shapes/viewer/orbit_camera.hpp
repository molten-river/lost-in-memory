#pragma once

#include "alias/alias.hpp"
#include "viewer/camera_strategy.hpp"

#include "tkit/utils/optional.hpp"

namespace Lim
{
/// Circles the focus on a free cursor: a left-button drag turns around it, W and S change the distance.
class OrbitCamera final : public CameraStrategy
{
  public:
    /// The pose that looks at target from distance away, turned by yaw about +Y and then pitch about +X.
    [[nodiscard]] static Onyx::Transform<Onyx::D3> Frame(const f32v3 &target, f32 yaw, f32 pitch, f32 distance);

    void Engage(const Onyx::Transform<Onyx::D3> &view, const CameraFrame &frame) override;
    void Update(const CameraFrame &frame) override;
    void ApplyTo(Onyx::Camera<Onyx::D3> &camera) const override;
    [[nodiscard]] bool LocksCursor() const override;
    [[nodiscard]] TKit::StringView GetName() const override;

  private:
    f32v3 m_Target{0.f};
    f32 m_Yaw = 0.f;
    f32 m_Pitch = 0.f;
    f32 m_Distance = 1.f;
    TKit::Optional<f32v2> m_LastCursor{};
};
} // namespace Lim

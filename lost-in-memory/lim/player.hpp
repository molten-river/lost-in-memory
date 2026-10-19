#pragma once

#include "lim/alias.hpp"

#include "onyx/camera.hpp"
#include "onyx/window.hpp"

#include "tkit/math/quaternion.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
/// A first-person probe flying with inertia in zero gravity, steered by mouse look and thrust keys.
class Player
{
  public:
    static constexpr f32 RADIUS = 0.5f;

    explicit Player(Onyx::Window &window);

    void Reset();
    void SetCursorLocked(bool locked);
    void Update(f32 deltaTime);
    /// Keeps the probe inside the sphere, sliding along its wall by dropping only the outward velocity.
    void ConfineToRadius(f32 maxRadius);
    void ApplyTo(Onyx::Camera<Onyx::D3> &camera) const;

    [[nodiscard]] const f32v3 &GetPosition() const;

  private:
    Onyx::Window *m_Window;
    f32v3 m_Position;
    f32v3 m_Velocity;
    f32q m_Orientation;
    TKit::Optional<f32v2> m_LastCursor{};
    bool m_CursorLocked = false;
};
} // namespace Lim

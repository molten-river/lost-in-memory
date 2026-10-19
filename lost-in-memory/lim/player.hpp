#pragma once

#include "lim/alias.hpp"
#include "lim/player_params.hpp"

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
    explicit Player(Onyx::Window &window);

    void Configure(const PlayerParams &params);
    void Reset();
    void SetCursorLocked(bool locked);
    void Update(f32 deltaTime);
    /// Keeps the probe inside the sphere, sliding along its wall by dropping only the outward velocity.
    void ConfineToRadius(f32 maxRadius);
    /// Moves by normal * depth and slides by dropping the velocity component along -normal.
    void PushOut(const f32v3 &normal, f32 depth);
    void ApplyTo(Onyx::Camera<Onyx::D3> &camera) const;

    [[nodiscard]] const f32v3 &GetPosition() const;
    [[nodiscard]] f32 GetRadius() const;

  private:
    Onyx::Window *m_Window;
    PlayerParams m_Params{};
    f32v3 m_Position;
    f32v3 m_Velocity;
    f32q m_Orientation;
    TKit::Optional<f32v2> m_LastCursor{};
    bool m_CursorLocked = false;
};
} // namespace Lim

#include "lim/player.hpp"

#include "graph/platform.hpp"

#include "tkit/math/math.hpp"
#include "tkit/math/quaternion.hpp"

namespace Lim
{
namespace
{
[[nodiscard]] f32 KeyAxis(const Onyx::Window &window, const Graph::Key positive, const Graph::Key negative)
{
    return f32(window.IsKeyPressed(positive)) - f32(window.IsKeyPressed(negative));
}
} // namespace

Player::Player(Onyx::Window &window) : m_Window(&window)
{
    Reset();
}

void Player::Configure(const PlayerParams &params)
{
    m_Params = params;
}

void Player::Reset()
{
    m_Position = f32v3{0.f};
    m_Velocity = f32v3{0.f};
    m_Orientation = Onyx::RotType<Onyx::D3>::Identity;
}

void Player::SetCursorLocked(const bool locked)
{
    Graph::Window_SetCursorMode(m_Window->GetHandle(), locked ? Graph::CursorMode_Disabled : Graph::CursorMode_Normal);
    m_CursorLocked = locked;
    m_LastCursor = TKit::Optional<f32v2>::None();
}

void Player::Update(const f32 deltaTime)
{
    f32v2 look{0.f};
    if (m_CursorLocked)
    {
        const f32v2 cursor = m_Window->GetMousePosition();
        if (m_LastCursor)
            look = (cursor - *m_LastCursor) * m_Params.MouseRadiansPerPixel;
        m_LastCursor = cursor;
    }
    const f32 roll = KeyAxis(*m_Window, Graph::Key_Q, Graph::Key_E) * m_Params.RollSpeed * deltaTime;
    m_Orientation = TKit::Math::Normalize(m_Orientation * f32q{f32v3{-look[1], -look[0], roll}});

    const f32v3 input{KeyAxis(*m_Window, Graph::Key_D, Graph::Key_A),
                      KeyAxis(*m_Window, Graph::Key_Space, Graph::Key_LeftControl),
                      KeyAxis(*m_Window, Graph::Key_S, Graph::Key_W)};
    if (TKit::Math::NormSquared(input) > 0.f)
        m_Velocity += m_Orientation * TKit::Math::Normalize(input) * (m_Params.Thrust * deltaTime);

    m_Velocity *= TKit::Math::Power(m_Params.VelocityKeptPerSecond, deltaTime);
    const f32 speedSquared = TKit::Math::NormSquared(m_Velocity);
    if (speedSquared > m_Params.MaxSpeed * m_Params.MaxSpeed)
        m_Velocity *= m_Params.MaxSpeed / TKit::Math::SquareRoot(speedSquared);

    m_Position += m_Velocity * deltaTime;
}

void Player::ConfineToRadius(const f32 maxRadius)
{
    const f32 limit = TKit::Math::Max(maxRadius - m_Params.Radius, 0.f);
    const f32 distanceSquared = TKit::Math::NormSquared(m_Position);
    if (distanceSquared <= limit * limit)
        return;

    const f32v3 outward = m_Position / TKit::Math::SquareRoot(distanceSquared);
    m_Position = outward * limit;
    const f32 outwardSpeed = TKit::Math::Dot(m_Velocity, outward);
    if (outwardSpeed > 0.f)
        m_Velocity -= outward * outwardSpeed;
}

void Player::PushOut(const f32v3 &normal, const f32 depth)
{
    m_Position += normal * depth;
    const f32 inwardSpeed = TKit::Math::Dot(m_Velocity, -normal);
    if (inwardSpeed > 0.f)
        m_Velocity += normal * inwardSpeed;
}

void Player::ApplyTo(Onyx::Camera<Onyx::D3> &camera) const
{
    camera.View.Translation = m_Position;
    camera.View.Rotation = m_Orientation;
}

const f32v3 &Player::GetPosition() const
{
    return m_Position;
}

f32 Player::GetRadius() const
{
    return m_Params.Radius;
}
} // namespace Lim

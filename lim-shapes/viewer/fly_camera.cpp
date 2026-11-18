#include "viewer/fly_camera.hpp"

#include "tkit/math/math.hpp"

namespace Lim
{
void FlyCamera::Engage(const Onyx::Transform<Onyx::D3> &view, const CameraFrame &)
{
    m_Position = view.Translation;
    m_Orientation = view.Rotation;
    m_Velocity = f32v3{0.f};
    m_LastCursor = TKit::Optional<f32v2>::None();
}

void FlyCamera::Update(const CameraFrame &frame)
{
    const CameraParams &params = frame.Params;
    const Onyx::Window &window = frame.Window;
    const f32 deltaTime = frame.DeltaTime;

    f32v2 look{0.f};
    f32v3 input{0.f};
    f32 roll = 0.f;
    if (frame.AcceptsInput)
    {
        const f32v2 cursor = window.GetMousePosition();
        if (m_LastCursor)
            look = (cursor - *m_LastCursor) * params.MouseRadiansPerPixel;
        m_LastCursor = cursor;
        roll = KeyAxis(window, Graph::Key_Q, Graph::Key_E) * params.FlyRollSpeed * deltaTime;
        input = f32v3{KeyAxis(window, Graph::Key_D, Graph::Key_A),
                      KeyAxis(window, Graph::Key_Space, Graph::Key_LeftControl),
                      KeyAxis(window, Graph::Key_S, Graph::Key_W)};
    }
    else
        m_LastCursor = TKit::Optional<f32v2>::None();

    m_Orientation = TKit::Math::Normalize(m_Orientation * f32q{f32v3{-look[1], -look[0], roll}});
    if (TKit::Math::NormSquared(input) > 0.f)
        m_Velocity += m_Orientation * TKit::Math::Normalize(input) * (params.FlyThrust * deltaTime);

    m_Velocity *= TKit::Math::Power(params.FlyVelocityKeptPerSecond, deltaTime);
    const f32 speedSquared = TKit::Math::NormSquared(m_Velocity);
    if (speedSquared > params.FlyMaxSpeed * params.FlyMaxSpeed)
        m_Velocity *= params.FlyMaxSpeed / TKit::Math::SquareRoot(speedSquared);

    m_Position += m_Velocity * deltaTime;
}

void FlyCamera::ApplyTo(Onyx::Camera<Onyx::D3> &camera) const
{
    camera.View.Translation = m_Position;
    camera.View.Rotation = m_Orientation;
}

TKit::StringView FlyCamera::GetName() const
{
    return "FLY";
}
} // namespace Lim

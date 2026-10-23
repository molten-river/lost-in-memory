#include "viewer/orbit_camera.hpp"

#include "tkit/math/math.hpp"
#include "tkit/math/quaternion.hpp"

namespace Lim
{
namespace
{
constexpr f32v3 YAW_AXIS{0.f, 1.f, 0.f};
constexpr f32v3 PITCH_AXIS{1.f, 0.f, 0.f};
constexpr f32v3 FORWARD{0.f, 0.f, -1.f};
constexpr f32 PITCH_LIMIT = 1.5f;
constexpr f32 MIN_DISTANCE = 0.25f;
constexpr f32 MAX_DISTANCE = 2000.f;
constexpr f32 DISTANCE_DOUBLING = 2.f;
constexpr f32 MIN_AIM_DISTANCE = 1e-4f;
} // namespace

Onyx::Transform<Onyx::D3> OrbitCamera::Frame(const f32v3 &target, const f32 yaw, const f32 pitch, const f32 distance)
{
    Onyx::Transform<Onyx::D3> view{};
    view.Rotation = f32q::FromAngleAxis(yaw, YAW_AXIS) * f32q::FromAngleAxis(pitch, PITCH_AXIS);
    view.Translation = target + view.Rotation * f32v3{0.f, 0.f, distance};
    return view;
}

void OrbitCamera::Engage(const Onyx::Transform<Onyx::D3> &view, const CameraFrame &frame)
{
    m_Target = frame.Focus.Target;
    const f32v3 offset = m_Target - view.Translation;
    const f32 length = TKit::Math::Norm(offset);
    const f32v3 forward = length > MIN_AIM_DISTANCE ? offset / length : view.Rotation * FORWARD;

    m_Yaw = TKit::Math::AntiTangent(-forward[0], -forward[2]);
    m_Pitch =
        TKit::Math::Clamp(TKit::Math::AntiSine(TKit::Math::Clamp(forward[1], -1.f, 1.f)), -PITCH_LIMIT, PITCH_LIMIT);
    m_Distance = TKit::Math::Clamp(frame.Params.OrbitDistance * frame.Focus.Radius, MIN_DISTANCE, MAX_DISTANCE);
    m_LastCursor = TKit::Optional<f32v2>::None();
}

void OrbitCamera::Update(const CameraFrame &frame)
{
    const CameraParams &params = frame.Params;
    const Onyx::Window &window = frame.Window;
    m_Target = frame.Focus.Target;
    if (!frame.AcceptsInput)
    {
        m_LastCursor = TKit::Optional<f32v2>::None();
        return;
    }

    if (window.IsMousePressed(Onyx::Mouse_ButtonLeft))
    {
        const f32v2 cursor = window.GetAbsoluteMousePosition();
        if (m_LastCursor)
        {
            const f32v2 drag = (cursor - *m_LastCursor) * params.MouseRadiansPerPixel;
            m_Yaw -= drag[0];
            m_Pitch = TKit::Math::Clamp(m_Pitch - drag[1], -PITCH_LIMIT, PITCH_LIMIT);
        }
        m_LastCursor = cursor;
    }
    else
        m_LastCursor = TKit::Optional<f32v2>::None();

    const f32 zoom = KeyAxis(window, Onyx::Key_S, Onyx::Key_W) * params.OrbitSpeed * frame.DeltaTime;
    m_Distance = TKit::Math::Clamp(m_Distance * TKit::Math::Power(DISTANCE_DOUBLING, zoom), MIN_DISTANCE, MAX_DISTANCE);
}

void OrbitCamera::ApplyTo(Onyx::Camera<Onyx::D3> &camera) const
{
    camera.View = Frame(m_Target, m_Yaw, m_Pitch, m_Distance);
}

TKit::StringView OrbitCamera::GetName() const
{
    return "ORBIT";
}
} // namespace Lim

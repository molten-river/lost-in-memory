#include "viewer/frame_timings.hpp"

#include "tkit/math/math.hpp"

namespace Lim
{
namespace
{
constexpr f32 SMOOTHING_SECONDS = 1.f;
} // namespace

void FrameTimings::Record(const Sample &sample)
{
    const f32 share = TKit::Math::Clamp(sample.Frame.AsSeconds() / SMOOTHING_SECONDS, 0.f, 1.f);
    const auto smooth = [share](f32 &average, const TKit::Timespan elapsed) {
        average = TKit::Math::LinearLerp(average, elapsed.AsMilliseconds(), share);
    };
    smooth(m_Frame, sample.Frame);
    smooth(m_Scene, sample.Scene);
    smooth(m_Rewrite, sample.Rewrite);
    smooth(m_Panel, sample.Panel);
    smooth(m_Render, sample.Render);
}

FrameTimings::Line FrameTimings::Describe() const
{
    return Line::Format("FRAME {:.1f} MS  SCENE {:.1f}  REWRITE {:.1f}  PANEL {:.1f}  RENDER {:.1f}", m_Frame, m_Scene,
                        m_Rewrite, m_Panel, m_Render);
}
} // namespace Lim

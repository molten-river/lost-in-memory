#include "viewer/playback_clock.hpp"

#include "tkit/math/math.hpp"

namespace Lim
{
namespace
{
constexpr f32 MIN_SCALE = 1.f / 64.f;
constexpr f32 MAX_SCALE = 64.f;
} // namespace

void PlaybackClock::Advance(const f32 deltaTime)
{
    if (!m_Paused)
        m_Seconds += f64(deltaTime) * f64(m_Scale);
}

void PlaybackClock::TogglePause()
{
    m_Paused = !m_Paused;
}

void PlaybackClock::ScaleBy(const f32 factor)
{
    m_Scale = TKit::Math::Clamp(m_Scale * factor, MIN_SCALE, MAX_SCALE);
}

void PlaybackClock::Scrub(const f32 seconds)
{
    m_Seconds = TKit::Math::Max(m_Seconds + f64(seconds), 0.0);
}

void PlaybackClock::Reset()
{
    m_Seconds = 0.0;
}

f32 PlaybackClock::GetSeconds() const
{
    return f32(m_Seconds);
}

f32 PlaybackClock::GetScale() const
{
    return m_Scale;
}

bool PlaybackClock::IsPaused() const
{
    return m_Paused;
}
} // namespace Lim

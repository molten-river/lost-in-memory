#pragma once

#include "alias/alias.hpp"

namespace Lim
{
/// The watch stage: absolute shape time with pause, a power-of-two time scale, scrubbing and reset.
class PlaybackClock
{
  public:
    void Advance(f32 deltaTime);
    void TogglePause();
    void ScaleBy(f32 factor);
    /// Moves time by seconds, never below zero.
    void Scrub(f32 seconds);
    void Reset();

    [[nodiscard]] f32 GetSeconds() const;
    [[nodiscard]] f32 GetScale() const;
    [[nodiscard]] bool IsPaused() const;

  private:
    f64 m_Seconds = 0.0;
    f32 m_Scale = 1.f;
    bool m_Paused = false;
};
} // namespace Lim

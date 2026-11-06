#pragma once

#include "alias/alias.hpp"

#include "tkit/container/static_array.hpp"
#include "tkit/profiling/timespan.hpp"

namespace Lim
{
/// Per-stage frame costs, each smoothed over about a second, for the panel header.
class FrameTimings
{
  public:
    using Line = TKit::StaticString<80>;

    /// Frame is one whole loop; Scene is the interval between the scene window's presents, as Onyx paces it.
    struct Sample
    {
        TKit::Timespan Frame;
        TKit::Timespan Scene;
        TKit::Timespan Evolve;
        TKit::Timespan Panel;
        TKit::Timespan Render;
    };

    void Record(const Sample &sample);
    [[nodiscard]] Line Describe() const;

  private:
    f32 m_Frame = 0.f;
    f32 m_Scene = 0.f;
    f32 m_Evolve = 0.f;
    f32 m_Panel = 0.f;
    f32 m_Render = 0.f;
};
} // namespace Lim

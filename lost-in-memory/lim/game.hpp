#pragma once

#include "lim/alias.hpp"
#include "lim/font_library.hpp"
#include "lim/level.hpp"
#include "lim/level_params.hpp"
#include "lim/player.hpp"
#include "lim/region.hpp"
#include "lim/sandbox_screen.hpp"

#include "onyx/camera.hpp"
#include "onyx/context.hpp"
#include "onyx/view.hpp"
#include "onyx/window.hpp"

#include "tkit/utils/optional.hpp"

namespace Lim
{
/// Owns the window and every module, cycling fade-in, play, fade-out and sandbox until Escape quits.
class Game
{
  public:
    Game();

    Game(const Game &) = delete;
    Game &operator=(const Game &) = delete;

    void Run();

  private:
    enum class State : u8
    {
        FadingIn,
        Playing,
        FadingOut,
        Sandbox
    };

    struct Fade
    {
        f32 Brightness;
        f32 Opacity;
    };

    void Restart();
    void Rebuild(SandboxScreen::Action action);
    [[nodiscard]] Fade Step(f32 deltaTime);
    void Enter(State state);

    Onyx::Window *m_Window;
    Onyx::Camera<Onyx::D3> m_Camera{};
    Onyx::RenderView<Onyx::D3> *m_View;
    Onyx::RenderContext<Onyx::D3> *m_Context;
    FontLibrary m_Fonts{};
    LevelParams m_Params;
    SandboxScreen m_Sandbox;
    Player m_Player;
    TKit::Optional<Region> m_Region{};
    State m_State = State::FadingIn;
    f32 m_StateSeconds = 0.f;
    SandboxScreen::Action m_PendingAction = SandboxScreen::Action::None;
    Level m_Level;
};
} // namespace Lim

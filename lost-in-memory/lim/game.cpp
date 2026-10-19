#include "lim/game.hpp"

#include "lim/palette.hpp"

#include "onyx/onyx.hpp"
#include "onyx/platform.hpp"

#include "tkit/math/math.hpp"
#include "tkit/utils/logging.hpp"

#include <utility>

namespace Lim
{
using Onyx::D3;

namespace
{
constexpr const char *WINDOW_TITLE = "Lost in Memory";
constexpr Onyx::WindowFlags BORDERLESS_FLAGS = Onyx::WindowFlag_Visible | Onyx::WindowFlag_Focused |
                                               Onyx::WindowFlag_FocusOnShow | Onyx::WindowFlag_InstallCallbacks;

constexpr f32 CAMERA_FAR = 250.f;
constexpr f32 FADE_SECONDS = 0.5f;
constexpr f32 MAX_FRAME_SECONDS = 0.1f;
constexpr u32 CAPTURE_ATTEMPTS = 3;

/// A borderless window covering the primary monitor, or Onyx's default decorated window when no monitor is reported.
[[nodiscard]] Onyx::WindowSpecs MakeWindowSpecs()
{
    Onyx::WindowSpecs specs{.Title = WINDOW_TITLE, .PresentMode = Onyx::PresentMode_VSync};
    Onyx_MonitorHandle *const monitor = Onyx::Platform::GetMainMonitor();
    if (!monitor)
        return specs;
    const Onyx::Platform::VideoMode mode = Onyx::Platform::GetVideoMode(monitor);
    if (mode.Dimensions[0] == 0 || mode.Dimensions[1] == 0)
        return specs;
    specs.Position = Onyx::i32v2{0, 0};
    specs.Dimensions = mode.Dimensions;
    specs.Flags = BORDERLESS_FLAGS;
    return specs;
}
} // namespace

Game::Game()
    : m_Window(Onyx::OpenWindow({.Window = MakeWindowSpecs()})),
      m_View(m_Window->CreateRenderView<D3>(&m_Camera, Onyx::RenderViewFlag_NormalizedCoordinates)),
      m_Context(Onyx::CreateRenderContext<D3>()), m_Params(LoadLevelParams()), m_Sandbox(*m_Window, m_Fonts),
      m_Player(*m_Window)
{
    m_Camera.PerspParameters.Far = CAMERA_FAR;
    m_Window->ClearColor = VOID_BLACK;
    m_View->ClearColor = VOID_BLACK;
    m_Context->AddTarget(m_View);
}

void Game::Run()
{
    for (u32 attempt = 0; attempt < CAPTURE_ATTEMPTS && !m_Region; ++attempt)
        m_Region = Region::Capture();
    if (!m_Region)
    {
        TKIT_LOG_ERROR("[LIM][GAME] Could not capture a memory region after {} attempts", CAPTURE_ATTEMPTS);
        return;
    }
    Restart();

    while (Onyx::Running())
    {
        if (m_PendingAction != SandboxScreen::Action::None)
            Rebuild(std::exchange(m_PendingAction, SandboxScreen::Action::None));
        if (m_Window->IsKeyPressed(Onyx::Key_Escape))
            Onyx::Quit(Onyx::QuitFlag_DestroyWindows);

        const Fade fade = Step(TKit::Math::Min(Onyx::GetDeltaTime(m_Window).AsSeconds(), MAX_FRAME_SECONDS));
        m_Player.ApplyTo(m_Camera);

        m_Context->Flush();
        m_Level.Draw(*m_Context, fade.Brightness);
        const SandboxScreen::Action action =
            m_Sandbox.Draw(m_Params, m_Level.GetOutcome() == Level::Outcome::Won, m_Region->GetAddress(), fade.Opacity);
        if (m_State == State::Sandbox && fade.Opacity >= 1.f)
            m_PendingAction = action;

        Onyx::Transfer();
        Onyx::Render();
    }
}

void Game::Restart()
{
    m_Level.Generate(*m_Region, m_Params);
    m_Player.Reset();
    m_Player.SetCursorLocked(true);
    Enter(State::FadingIn);
}

void Game::Rebuild(const SandboxScreen::Action action)
{
    if (!SaveLevelParams(m_Params))
        TKIT_LOG_ERROR("[LIM][GAME] Could not save the level params to defaults.yaml");

    if (action == SandboxScreen::Action::NewRegion)
    {
        const TKit::Optional<Region> fresh = Region::Capture();
        if (fresh)
            m_Region = *fresh;
        else
            TKIT_LOG_ERROR("[LIM][GAME] Could not capture a new memory region, keeping 0x{:016X}",
                           m_Region->GetAddress());
    }
    Restart();
}

Game::Fade Game::Step(const f32 deltaTime)
{
    m_StateSeconds += deltaTime;
    const f32 linear = TKit::Math::Min(m_StateSeconds / FADE_SECONDS, 1.f);
    const f32 progress = linear * linear * (3.f - 2.f * linear);

    switch (m_State)
    {
    case State::FadingIn:
        if (m_StateSeconds >= FADE_SECONDS)
            Enter(State::Playing);
        return Fade{.Brightness = progress, .Opacity = 0.f};
    case State::Playing:
        m_Player.Update(deltaTime);
        m_Level.Update(deltaTime, m_Player);
        if (m_Level.GetOutcome() != Level::Outcome::Playing)
            Enter(State::FadingOut);
        return Fade{.Brightness = 1.f, .Opacity = 0.f};
    case State::FadingOut:
        if (m_StateSeconds < FADE_SECONDS)
            return Fade{.Brightness = 1.f - progress, .Opacity = 0.f};
        m_Player.SetCursorLocked(false);
        Enter(State::Sandbox);
        return Fade{.Brightness = 0.f, .Opacity = 0.f};
    case State::Sandbox:
        return Fade{.Brightness = 0.f, .Opacity = progress};
    }
    return Fade{.Brightness = 0.f, .Opacity = 0.f};
}

void Game::Enter(const State state)
{
    m_State = state;
    m_StateSeconds = 0.f;
}
} // namespace Lim

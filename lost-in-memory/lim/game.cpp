#include "lim/game.hpp"

#include "lim/debug_tools.hpp"
#include "lim/params_reflection.hpp"
#include "look/window_specs.hpp"
#include "shapes/shape_kit_file.hpp"
#include "tuning/schema_rules.hpp"

#include "onyx/onyx.hpp"

#include "tkit/math/math.hpp"
#include "tkit/utils/logging.hpp"

#include <utility>

namespace Lim
{
using Onyx::D3;

namespace
{
constexpr const char *WINDOW_TITLE = "Lost in Memory";

constexpr f32 CAMERA_FAR_PER_SHELL_RADIUS = 4.f;
constexpr f32 MAX_FRAME_SECONDS = 0.1f;
constexpr u32 CAPTURE_ATTEMPTS = 3;

[[nodiscard]] SandboxScreen::Title TitleFromOutcome(const Level::Outcome outcome)
{
    return outcome == Level::Outcome::Won ? SandboxScreen::Title::Cleared : SandboxScreen::Title::Terminated;
}
} // namespace

Game::Game(const PlayMode mode)
    : m_Window(Onyx::OpenWindow({.Window = MakeBorderlessWindowSpecs(WINDOW_TITLE)})),
      m_View(m_Window->CreateRenderView<D3>(&m_Camera, Onyx::RenderViewFlag_NormalizedCoordinates)),
      m_Context(Onyx::CreateRenderContext<D3>()), m_Params(Sanitize(LoadLevelParams())), m_Kit(LoadShapeKitParams()),
      m_Sandbox(*m_Window, m_Fonts), m_Player(*m_Window), m_Mode(mode)
{
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

        bool enterTriggered = false;
        if constexpr (DEBUG_TOOLS)
        {
            const bool enterPressed = m_Window->IsKeyPressed(Onyx::Key_Enter);
            enterTriggered = enterPressed && !m_EnterWasPressed;
            m_EnterWasPressed = enterPressed;
        }

        const Fade fade =
            Step(TKit::Math::Min(Onyx::GetDeltaTime(m_Window).AsSeconds(), MAX_FRAME_SECONDS), enterTriggered);
        m_Player.ApplyTo(m_Camera);

        m_Context->Flush();
        if (m_State != State::Sandbox)
            m_Level.Draw(*m_Context, fade.Brightness);
        const SandboxScreen::Action action =
            m_Sandbox.Draw(m_Params, m_Kit, m_Mode, m_Title, m_Region->GetAddress(), fade.Opacity);
        if (m_State == State::Sandbox && fade.Opacity >= 1.f)
            m_PendingAction = action;

        Onyx::Transfer();
        Onyx::Render();
    }
}

void Game::Restart()
{
    m_Window->ClearColor = m_Params.LookFeel.Colors.VoidBlack;
    m_View->ClearColor = m_Params.LookFeel.Colors.VoidBlack;
    m_Camera.PerspParameters.Far = CAMERA_FAR_PER_SHELL_RADIUS * m_Params.Mechanics.Shell.Radius;
    m_Player.Configure(m_Params.LookFeel.Player);
    m_Level.Generate(*m_Region, m_Params, m_Kit, m_Mode);
    m_Player.Reset();
    m_Player.SetCursorLocked(true);
    m_View->RemoveFlags(Onyx::RenderViewFlag_Hidden);
    Enter(State::FadingIn);
}

/// Resets both roots on request, saves both files, then restarts from the kept bytes or a fresh capture.
void Game::Rebuild(const SandboxScreen::Action action)
{
    if (action == SandboxScreen::Action::Reset)
    {
        m_Params = LevelParams{};
        m_Kit = ShapeKitParams{};
    }
    m_Params = Sanitize(m_Params);
    ApplySchema(m_Kit);
    if (!SaveLevelParams(m_Params))
        TKIT_LOG_ERROR("[LIM][GAME] Could not save the level params to {}", LIM_DEFAULTS_PATH);
    if (!SaveShapeKitParams(m_Kit))
        TKIT_LOG_ERROR("[LIM][GAME] Could not save the shape kit params to {}", LIM_SHAPE_KIT_DEFAULTS_PATH);

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

Game::Fade Game::Step(const f32 deltaTime, const bool enterTriggered)
{
    m_StateSeconds += deltaTime;
    const f32 fadeSeconds = m_Params.LookFeel.FadeSeconds;
    const f32 linear = TKit::Math::Min(m_StateSeconds / fadeSeconds, 1.f);
    const f32 progress = linear * linear * (3.f - 2.f * linear);

    switch (m_State)
    {
    case State::FadingIn:
        if (enterTriggered)
        {
            Interrupt();
            return Fade{.Brightness = 1.f, .Opacity = 0.f};
        }
        if (m_StateSeconds >= fadeSeconds)
            Enter(State::Playing);
        return Fade{.Brightness = progress, .Opacity = 0.f};
    case State::Playing:
        m_Player.Update(deltaTime);
        m_Level.Update(deltaTime, m_Player);
        if (enterTriggered)
        {
            Interrupt();
            return Fade{.Brightness = 1.f, .Opacity = 0.f};
        }
        if (m_Level.GetOutcome() != Level::Outcome::Playing)
        {
            m_Title = TitleFromOutcome(m_Level.GetOutcome());
            Enter(State::FadingOut);
        }
        return Fade{.Brightness = 1.f, .Opacity = 0.f};
    case State::FadingOut:
        if (m_StateSeconds < fadeSeconds)
            return Fade{.Brightness = 1.f - progress, .Opacity = 0.f};
        m_Player.SetCursorLocked(false);
        m_View->AddFlags(Onyx::RenderViewFlag_Hidden);
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

void Game::Interrupt()
{
    m_Title = SandboxScreen::Title::Interrupted;
    Enter(State::FadingOut);
}
} // namespace Lim

#pragma once

#include "alias/alias.hpp"
#include "look/font_library.hpp"
#include "shapes/shape_renderer.hpp"
#include "tuning/param_panel.hpp"
#include "viewer/camera_registry.hpp"
#include "viewer/camera_strategy.hpp"
#include "viewer/playback_clock.hpp"
#include "viewer/record_gallery.hpp"
#include "viewer/record_hud.hpp"
#include "viewer/viewer_params.hpp"

#include "onyx/camera.hpp"
#include "onyx/context.hpp"
#include "onyx/handle.hpp"
#include "onyx/view.hpp"
#include "onyx/window.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
/// Owns the window and every viewer module, and runs the frame: input, watch, pose, draw, panel and HUD.
class Viewer
{
  public:
    Viewer();

    Viewer(const Viewer &) = delete;
    Viewer &operator=(const Viewer &) = delete;

    void Run();

  private:
    enum class Rebake : u8
    {
        SameBytes,
        NewRegion
    };

    struct KeyBinding
    {
        Onyx::Key Key;
        void (*Command)(Viewer &viewer);
    };

    static constexpr u32 KEY_BINDING_COUNT = 12;

    void Home();
    void EngageCamera();
    [[nodiscard]] CameraFrame MakeCameraFrame(f32 deltaTime) const;
    void RefreshCursor();
    void HandleKeys();
    void Rebuild(Rebake rebake);
    void DrawScene();
    [[nodiscard]] TKit::Optional<Rebake> DrawPanel();

    Onyx::Window *m_Window;
    Onyx::Camera<Onyx::D3> m_Camera{};
    Onyx::RenderView<Onyx::D3> *m_View;
    Onyx::RenderContext<Onyx::D3> *m_Context;
    Onyx::Resource m_Material;
    FontLibrary m_Fonts{};
    ViewerParams m_Params;
    const ViewerParams m_Defaults{};
    ParamPanel m_Panel;
    RecordHud m_Hud;
    ShapeRenderer m_Renderer{};
    RecordGallery m_Gallery{};
    CameraRegistry m_Cameras{};
    PlaybackClock m_Clock{};
    CameraRegistry::Kind m_CameraKind = CameraRegistry::Kind::Fly;
    TKit::Optional<Rebake> m_PendingRebake{};
    TKit::FixedArray<bool, KEY_BINDING_COUNT> m_KeyWasPressed{};
    bool m_PanelOpen = false;
};
} // namespace Lim

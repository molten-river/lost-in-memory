#pragma once

#include "alias/alias.hpp"
#include "look/font_library.hpp"
#include "shapes/shape_kit_params.hpp"
#include "shapes/shape_renderer.hpp"
#include "tuning/param_panel.hpp"
#include "viewer/camera_registry.hpp"
#include "viewer/camera_strategy.hpp"
#include "viewer/frame_timings.hpp"
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
#include "tkit/profiling/clock.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
/// Owns the fullscreen scene window, the tool window with the panel, and every viewer module, and runs the frame.
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
        NewRegion,
        ResetParams
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
    void SetCameraActive(bool active);
    void HandleSceneEvents();
    [[nodiscard]] bool IsKeyDown(Onyx::Key key) const;
    void HandleKeys();
    void StepSelection(i32 offset);
    void SettleShapes();
    void Rebuild(Rebake rebake);
    void MarkShapesBaked();
    void Persist();
    void DrawScene();
    [[nodiscard]] TKit::Optional<Rebake> DrawPanel();

    Onyx::Window *m_Window;
    Onyx::Window *m_ToolWindow;
    Onyx::Camera<Onyx::D3> m_Camera{};
    Onyx::RenderView<Onyx::D3> *m_View;
    Onyx::RenderContext<Onyx::D3> *m_Context;
    Onyx::Resource m_Material;
    FontLibrary m_Fonts{};
    ViewerParams m_Params;
    const ViewerParams m_Defaults{};
    ShapeKitParams m_Kit;
    const ShapeKitParams m_KitDefaults{};
    ParamPanel m_Panel;
    RecordHud m_Hud{};
    ShapeRenderer m_Renderer{};
    RecordGallery m_Gallery{};
    CameraRegistry m_Cameras{};
    PlaybackClock m_Clock{};
    FrameTimings m_Timings{};
    TKit::Clock m_FrameClock{};
    CameraRegistry::Kind m_CameraKind = CameraRegistry::Kind::Fly;
    TKit::Optional<Rebake> m_PendingRebake{};
    TKit::FixedArray<bool, KEY_BINDING_COUNT> m_KeyWasPressed{};
    ShapeKitParams m_BakedShapes{};
    ShapeKitParams m_SeenShapes{};
    TKit::Clock m_ShapesSettle{};
    bool m_CameraActive = false;
    bool m_AwaitingClickRelease = false;
};
} // namespace Lim

#include "viewer/viewer.hpp"
#include "viewer/orbit_camera.hpp"
#include "viewer/params_reflection.hpp"
#include "viewer/viewer_visibility.hpp"

#include "memory/region.hpp"
#include "tuning/params_file.hpp"
#include "tuning/schema_rules.hpp"

#include "onyx/onyx.hpp"
#include "onyx/resources.hpp"

#include "tkit/container/span.hpp"
#include "tkit/math/math.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/utils/logging.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Lim
{
using Onyx::D3;

namespace
{
constexpr const char *WINDOW_TITLE = "lim shapes";
constexpr u32v2 WINDOW_SIZE{1600, 900};

constexpr f32 CAMERA_FAR = 2000.f;
constexpr f32 MAX_FRAME_SECONDS = 0.1f;
constexpr u32 CAPTURE_ATTEMPTS = 3;

constexpr f32 HOME_YAW = 0.45f;
constexpr f32 HOME_PITCH = -0.3f;
constexpr f32 HOME_DISTANCE_SCALE = 1.6f;

constexpr f32 MATERIAL_ROUGHNESS = 0.9f;
constexpr f32v3 LIGHT_DIRECTION{-0.45f, -0.8f, -0.4f};
constexpr f32 LIGHT_EXTENT = 5000.f;

constexpr f32 SCALE_STEP = 2.f;
constexpr f32 SCRUB_SECONDS = 0.5f;

constexpr TKit::FixedArray<PanelTab, 4> TABS{{
    {"Look", "Look"},
    {"Camera", "Camera"},
    {"Gallery", "Gallery"},
    {"Shapes", "Shape kit"},
}};

constexpr TKit::FixedArray<TKit::StringView, 2> HEADER_LINES{"LOOK, CAMERA AND GALLERY CHANGE LIVE",
                                                             "SHAPE KIT CHANGES REBAKE ON APPLY"};
constexpr TKit::FixedArray<TKit::StringView, 2> BUTTON_LABELS{"APPLY", "NEW REGION"};

/// Whether a reflected top-level member of Params is itself a struct, named name.
template <typename Params> [[nodiscard]] constexpr bool IsStructMember(const TKit::StringView name)
{
    bool found = false;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
            found = found || TKit::StringView{field.Name} == name;
    });
    return found;
}

[[nodiscard]] constexpr bool AreTabsKnown()
{
    for (const PanelTab &tab : TABS)
        if (!IsStructMember<ViewerParams>(tab.Field))
            return false;
    return true;
}

static_assert(IsSchemaSound<ViewerParams>(), "Every viewer tuning field needs exactly one ParamSpec in its schema");
static_assert(AreTabsKnown(), "Every tab must name a struct member of ViewerParams");

[[nodiscard]] ViewerParams LoadViewerParams()
{
    TKIT_ASSERT(AreDefaultsWithinSchema<ViewerParams>(), "[LIM][VIEWER] A default breaks its pair order or bounds");
    ViewerParams params = LoadParams<ViewerParams>(LIM_SHAPES_DEFAULTS_PATH);
    ApplySchema(params);
    return params;
}
} // namespace

Viewer::Viewer()
    : m_Window(Onyx::OpenWindow(
          {.Window = {.Title = WINDOW_TITLE, .Dimensions = WINDOW_SIZE, .PresentMode = Onyx::PresentMode_VSync}})),
      m_View(m_Window->CreateRenderView<D3>(&m_Camera, Onyx::RenderViewFlag_NormalizedCoordinates)),
      m_Context(Onyx::CreateRenderContext<D3>()),
      m_Material(Onyx::Resources::RegisterMaterial<D3>(Onyx::MaterialData<D3>{.RoughnessFactor = MATERIAL_ROUGHNESS})),
      m_Params(LoadViewerParams()), m_Panel(*m_Window, m_Fonts), m_Hud(*m_Window, m_Fonts)
{
    m_Camera.PerspParameters.Far = CAMERA_FAR;
    m_Context->AddTarget(m_View);
    Onyx::Resources::Sync(Onyx::SyncFlag_Materials);
}

void Viewer::Run()
{
    TKit::Optional<Region> region{};
    for (u32 attempt = 0; attempt < CAPTURE_ATTEMPTS && !region; ++attempt)
        region = Region::Capture(Region::Filter::NonZero);
    if (!region)
    {
        TKIT_LOG_ERROR("[LIM][VIEWER] Could not capture a memory region after {} attempts", CAPTURE_ATTEMPTS);
        return;
    }
    m_Gallery.Bake(*region, m_Params.Shapes);
    Home();

    while (Onyx::Running())
    {
        if (m_PendingRebake)
        {
            Rebuild(*m_PendingRebake);
            m_PendingRebake = TKit::Optional<Rebake>::None();
        }
        if (m_Window->IsKeyPressed(Onyx::Key_Escape))
            Onyx::Quit(Onyx::QuitFlag_DestroyWindows);
        HandleKeys();

        const f32 deltaTime = TKit::Math::Min(Onyx::GetDeltaTime(m_Window).AsSeconds(), MAX_FRAME_SECONDS);
        m_Clock.Advance(deltaTime);
        CameraStrategy &camera = m_Cameras.Get(m_CameraKind);
        camera.Update(MakeCameraFrame(deltaTime));
        camera.ApplyTo(m_Camera);
        m_Gallery.Rewrite(m_Renderer, m_Clock.GetSeconds(), m_Params.Look.Colors, m_Params.Gallery);

        m_Context->Flush();
        DrawScene();
        const u32 record = m_Gallery.GetSelection();
        m_Hud.Draw(m_Params.Look, RecordHud::Readout{.Record = record,
                                                     .Bytes = m_Gallery.GetRecord(record),
                                                     .Source = m_Gallery.GetSource(),
                                                     .GalleryMode = m_Gallery.GetModeName(),
                                                     .CameraMode = camera.GetName(),
                                                     .Seconds = m_Clock.GetSeconds(),
                                                     .TimeScale = m_Clock.GetScale(),
                                                     .Paused = m_Clock.IsPaused()});
        m_PendingRebake = DrawPanel();

        Onyx::Transfer();
        Onyx::Render();
    }
}

/// Frames everything the gallery shows from a fixed three-quarter angle, then hands the pose to the active camera.
void Viewer::Home()
{
    const CameraFocus scene = m_Gallery.GetSceneFocus(m_Params.Gallery);
    m_Camera.View = OrbitCamera::Frame(scene.Target, HOME_YAW, HOME_PITCH, HOME_DISTANCE_SCALE * scene.Radius);
    EngageCamera();
}

void Viewer::EngageCamera()
{
    m_Cameras.Get(m_CameraKind).Engage(m_Camera.View, MakeCameraFrame(0.f));
    RefreshCursor();
}

CameraFrame Viewer::MakeCameraFrame(const f32 deltaTime) const
{
    return CameraFrame{.Window = *m_Window,
                       .Params = m_Params.Camera,
                       .Focus = m_Gallery.GetFocus(m_Params.Gallery),
                       .DeltaTime = deltaTime,
                       .AcceptsInput = !m_PanelOpen};
}

void Viewer::RefreshCursor()
{
    const bool locked = !m_PanelOpen && m_Cameras.Get(m_CameraKind).LocksCursor();
    GLFWwindow *const handle = m_Window->GetHandle();
    glfwSetInputMode(handle, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(handle, GLFW_RAW_MOUSE_MOTION, locked ? GLFW_TRUE : GLFW_FALSE);
}

/// Runs each bound command once per press, because Onyx reports whether a key is held, not when it went down.
void Viewer::HandleKeys()
{
    static constexpr TKit::FixedArray<KeyBinding, KEY_BINDING_COUNT> BINDINGS{{
        {Onyx::Key_Tab,
         [](Viewer &viewer) {
             viewer.m_PanelOpen = !viewer.m_PanelOpen;
             viewer.RefreshCursor();
         }},
        {Onyx::Key_G,
         [](Viewer &viewer) {
             viewer.m_Gallery.ToggleMode();
             viewer.Home();
         }},
        {Onyx::Key_Left, [](Viewer &viewer) { viewer.m_Gallery.MoveSelection(-1); }},
        {Onyx::Key_Right, [](Viewer &viewer) { viewer.m_Gallery.MoveSelection(1); }},
        {Onyx::Key_O,
         [](Viewer &viewer) {
             viewer.m_CameraKind = CameraRegistry::Kind((u32(viewer.m_CameraKind) + 1) % CameraRegistry::KIND_COUNT);
             viewer.EngageCamera();
         }},
        {Onyx::Key_P, [](Viewer &viewer) { viewer.m_Clock.TogglePause(); }},
        {Onyx::Key_LeftBracket, [](Viewer &viewer) { viewer.m_Clock.ScaleBy(1.f / SCALE_STEP); }},
        {Onyx::Key_RightBracket, [](Viewer &viewer) { viewer.m_Clock.ScaleBy(SCALE_STEP); }},
        {Onyx::Key_Comma, [](Viewer &viewer) { viewer.m_Clock.Scrub(-SCRUB_SECONDS); }},
        {Onyx::Key_Period, [](Viewer &viewer) { viewer.m_Clock.Scrub(SCRUB_SECONDS); }},
        {Onyx::Key_Backspace, [](Viewer &viewer) { viewer.m_Clock.Reset(); }},
        {Onyx::Key_R, [](Viewer &viewer) { viewer.Rebuild(Rebake::NewRegion); }},
    }};

    for (u32 i = 0; i < KEY_BINDING_COUNT; ++i)
    {
        const bool pressed = m_Window->IsKeyPressed(BINDINGS[i].Key);
        const bool triggered = pressed && !m_KeyWasPressed[i];
        m_KeyWasPressed[i] = pressed;
        if (triggered)
            BINDINGS[i].Command(*this);
    }
}

/// Sanitizes and saves the params, then rebakes every record from the kept bytes or from a fresh capture.
void Viewer::Rebuild(const Rebake rebake)
{
    ApplySchema(m_Params);
    if (!SaveParams(LIM_SHAPES_DEFAULTS_PATH, m_Params))
        TKIT_LOG_ERROR("[LIM][VIEWER] Could not save the viewer params to defaults.yaml");

    if (rebake == Rebake::NewRegion)
    {
        const TKit::Optional<Region> fresh = Region::Capture(Region::Filter::NonZero);
        if (fresh)
        {
            m_Gallery.Bake(*fresh, m_Params.Shapes);
            return;
        }
        TKIT_LOG_ERROR("[LIM][VIEWER] Could not capture a new memory region, keeping 0x{:016X}", m_Gallery.GetSource());
    }
    m_Gallery.Rebake(m_Params.Shapes);
}

void Viewer::DrawScene()
{
    const ViewerLook &look = m_Params.Look;
    m_Window->ClearColor = look.Colors.Background;
    m_View->ClearColor = look.Colors.Background;

    m_Context->Material(m_Material);
    m_Context->RenderFlags(Onyx::RenderModeFlag_Shaded);
    m_Context->SetAmbientLight(look.Colors.Ambient);
    m_Context->SetAmbientIntensity(look.AmbientIntensity);
    m_Context->DirectionalLight(Onyx::DirectionalLightParameters<D3>{.Direction = LIGHT_DIRECTION,
                                                                     .Extent = f32v2{LIGHT_EXTENT},
                                                                     .Tint = look.Colors.Light,
                                                                     .Intensity = look.LightIntensity});
    m_Renderer.Draw(*m_Context);
}

/// Edits the params in place every frame; a pressed button becomes a rebake run at the top of the next frame.
TKit::Optional<Viewer::Rebake> Viewer::DrawPanel()
{
    static constexpr TKit::FixedArray<Rebake, BUTTON_LABELS.GetSize()> BUTTON_REBAKES{Rebake::SameBytes,
                                                                                      Rebake::NewRegion};

    TKit::Optional<Rebake> requested{};
    if (m_Panel.Begin(ToUiTheme(m_Params.Look.Colors), m_Params.Look.TextSize, m_PanelOpen ? 1.f : 0.f))
    {
        const TKit::StaticString32 source = TKit::StaticString32::Format("SOURCE 0x{:016X}", m_Gallery.GetSource());
        m_Panel.Header("LIM SHAPES", HEADER_LINES, source);
        m_Panel.Tabs(m_Params, m_Defaults, ViewerVisibility{}, TABS);
        const TKit::Optional<u32> pressed = m_Panel.Buttons(BUTTON_LABELS);
        if (pressed)
            requested = BUTTON_REBAKES[*pressed];
    }
    m_Panel.End();
    return requested;
}
} // namespace Lim

#include "viewer/viewer.hpp"
#include "viewer/orbit_camera.hpp"
#include "viewer/params_reflection.hpp"
#include "viewer/viewer_visibility.hpp"

#include "look/window_specs.hpp"
#include "memory/region.hpp"
#include "shapes/shape_kit_file.hpp"
#include "tuning/params_file.hpp"
#include "tuning/schema_rules.hpp"

#include "onyx/onyx.hpp"
#include "onyx/resources.hpp"

#include "tkit/container/span.hpp"
#include "tkit/math/math.hpp"
#include "tkit/reflection/reflect.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/utils/logging.hpp"

#include "graph/platform.hpp"

namespace Lim
{
using Onyx::D3;

namespace
{
constexpr const char *WINDOW_TITLE = "lim shapes";
constexpr const char *TOOL_WINDOW_TITLE = "lim shapes - tuning";
constexpr u32v2 TOOL_WINDOW_SIZE{700U, 1000U};
constexpr i32 TOOL_WINDOW_EDGE = 16;
constexpr i32 TOOL_WINDOW_TOP = 48;
constexpr i32 TOOL_WINDOW_VERTICAL_ROOM = 3 * TOOL_WINDOW_TOP;
constexpr i32 TOOL_WINDOW_MIN_HEIGHT = 480;
constexpr Onyx::WindowFlags TOOL_WINDOW_FLAGS = Onyx::WindowFlag_Floating | Onyx::WindowFlag_NotFocused;
constexpr Onyx::OpenWindowFlags OPEN_FLAGS = Onyx::OpenWindowFlag_DoNotDestroyOnShouldClose;

constexpr f32 CAMERA_FAR = 2000.f;
constexpr f32 MAX_FRAME_SECONDS = 0.1f;
constexpr u32 CAPTURE_ATTEMPTS = 3;

constexpr f32 HOME_YAW = 0.45f;
constexpr f32 HOME_PITCH = -0.3f;
constexpr f32 HOME_DISTANCE_SCALE = 1.6f;

constexpr f32 MATERIAL_ROUGHNESS = 0.9f;
constexpr f32v3 LIGHT_DIRECTION{-0.45f, -0.8f, -0.4f};
constexpr f32 LIGHT_EXTENT = 5000.f;

constexpr f32 SHAPES_SETTLE_SECONDS = 0.1f;

constexpr f32 SCALE_STEP = 2.f;
constexpr f32 SCRUB_SECONDS = 0.5f;

constexpr TKit::FixedArray<PanelTab, 3> VIEWER_TABS{{
    {"Look", "Look"},
    {"Camera", "Camera"},
    {"Gallery", "Gallery"},
}};
constexpr TKit::FixedArray<PanelTab, 1> KIT_TABS{PanelTab{"WalkMass", "Shape kit"}};

constexpr TKit::FixedArray<TKit::StringView, 2> BUTTON_LABELS{"SAME BYTES", "NEW REGION"};

/// Whether a reflected top-level member of Params is itself a struct, named name.
template <typename Params> [[nodiscard]] constexpr bool IsStructMember(const TKit::StringView name)
{
    bool found = false;
    ForEachMemberFieldConstexpr<Params>([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
            found = found || TKit::StringView{field.Name} == name;
    });
    return found;
}

template <typename Root> [[nodiscard]] constexpr bool AreTabsKnown(const TKit::Span<const PanelTab> tabs)
{
    for (const PanelTab &tab : tabs)
        if (!IsStructMember<Root>(tab.Field))
            return false;
    return true;
}

/// Whether every reflected leaf of the two structs holds the same value.
template <typename Params> [[nodiscard]] bool AreParamsEqual(const Params &left, const Params &right)
{
    bool equal = true;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
            equal = equal && AreParamsEqual(field.Get(left), field.Get(right));
        else
            equal = equal && field.Get(left) == field.Get(right);
    });
    return equal;
}

static_assert(IsSchemaSound<ViewerParams>(), "Every viewer tuning field needs exactly one ParamSpec in its schema");
static_assert(AreTabsKnown<ViewerParams>(VIEWER_TABS) && AreTabsKnown<ShapeKitParams>(KIT_TABS),
              "Every tab must name a struct member of its root");

[[nodiscard]] ViewerParams LoadViewerParams()
{
    TKIT_ASSERT(AreDefaultsWithinSchema<ViewerParams>(), "[LIM][VIEWER] A default breaks its pair order or bounds");
    ViewerParams params = LoadParams<ViewerParams>(LIM_SHAPES_DEFAULTS_PATH);
    ApplySchema(params);
    return params;
}

/// Floats top-right over the scene without taking focus; presents without vsync, so hiding it never stalls the loop.
[[nodiscard]] Onyx::WindowSpecs MakeToolWindowSpecs()
{
    Onyx::WindowSpecs specs{.Title = TOOL_WINDOW_TITLE,
                            .ScreenSize = TOOL_WINDOW_SIZE,
                            .PresentMode = Onyx::PresentMode_Immediate,
                            .Flags = TOOL_WINDOW_FLAGS};
    const TKit::Optional<u32v2> screen = GetMainScreenSize();
    if (!screen)
        return specs;

    const i32 height = TKit::Math::Clamp(i32((*screen)[1]) - TOOL_WINDOW_VERTICAL_ROOM, TOOL_WINDOW_MIN_HEIGHT,
                                         i32(TOOL_WINDOW_SIZE[1]));
    specs.ScreenSize = u32v2{TOOL_WINDOW_SIZE[0], u32(height)};
    specs.Position =
        i32v2{TKit::Math::Max(i32((*screen)[0]) - i32(TOOL_WINDOW_SIZE[0]) - TOOL_WINDOW_EDGE, 0), TOOL_WINDOW_TOP};
    return specs;
}
} // namespace

Viewer::Viewer()
    : m_Window(Onyx::OpenWindow({.Window = MakeBorderlessWindowSpecs(WINDOW_TITLE), .Flags = OPEN_FLAGS})),
      m_ToolWindow(Onyx::OpenWindow({.Window = MakeToolWindowSpecs(), .Flags = OPEN_FLAGS})),
      m_View(m_Window->CreateRenderView<D3>(&m_Camera)), m_Context(Onyx::CreateRenderContext<D3>()),
      m_Material(Onyx::Material_Register<D3>(Onyx::MaterialData<D3>{.RoughnessFactor = MATERIAL_ROUGHNESS})),
      m_Params(LoadViewerParams()), m_Kit(LoadShapeKitParams()), m_Panel(*m_ToolWindow, m_Fonts)
{
    m_Camera.PerspParameters.Far = CAMERA_FAR;
    m_Context->AddTarget(m_View);
    Onyx::Resources_Sync(Onyx::SyncFlag_Materials);
}

void Viewer::Run()
{
    TKit::Optional<Region> region{};
    for (u32 attempt = 0; attempt < CAPTURE_ATTEMPTS && !region; ++attempt)
        region = Region::Capture();
    if (!region)
    {
        TKIT_LOG_ERROR("[LIM][VIEWER] Could not capture a memory region after {} attempts", CAPTURE_ATTEMPTS);
        return;
    }
    m_Gallery.Bake(*region, m_Kit);
    MarkShapesBaked();
    Home();
    SetCameraActive(true);
    m_FrameClock.Restart();

    while (Onyx::Running())
    {
        if (m_PendingRebake)
        {
            Rebuild(*m_PendingRebake);
            m_PendingRebake = TKit::Optional<Rebake>::None();
        }
        SettleShapes();
        if (IsKeyDown(Onyx::Key_Escape) || m_Window->ShouldClose() || m_ToolWindow->ShouldClose())
            Onyx::Quit(Onyx::QuitFlag_DestroyWindows);
        HandleSceneEvents();
        HandleKeys();

        const f32 deltaTime = TKit::Math::Min(Onyx::GetDeltaTime(m_Window).AsSeconds(), MAX_FRAME_SECONDS);
        m_Clock.Advance(deltaTime);
        CameraStrategy &camera = m_Cameras.Get(m_CameraKind);
        camera.Update(MakeCameraFrame(deltaTime));
        camera.ApplyTo(m_Camera);

        TKit::Clock stage{};
        m_Gallery.Rewrite(m_Renderer, m_Clock.GetSeconds(), m_Params.Look.Colors, m_Params.Gallery);
        const TKit::Timespan rewrite = stage.Restart();

        m_Context->Flush();
        DrawScene();
        stage.Restart();
        m_PendingRebake = DrawPanel();
        const TKit::Timespan panel = stage.Restart();

        Onyx::SetTargetDeltaTime(m_Window, TKit::Timespan{});
        Onyx::Transfer();
        Onyx::Render();
        m_Timings.Record(FrameTimings::Sample{.Frame = m_FrameClock.Restart(),
                                              .Scene = Onyx::GetDeltaTime(m_Window),
                                              .Rewrite = rewrite,
                                              .Panel = panel,
                                              .Render = stage.Restart()});
    }
    Persist();
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
}

CameraFrame Viewer::MakeCameraFrame(const f32 deltaTime) const
{
    return CameraFrame{.Window = *m_Window,
                       .Params = m_Params.Camera,
                       .Focus = m_Gallery.GetFocus(m_Params.Gallery),
                       .DeltaTime = deltaTime,
                       .AcceptsInput = m_CameraActive && !m_AwaitingClickRelease};
}

/// Locks the scene cursor exactly while the camera takes input; a free cursor can reach the tool window.
void Viewer::SetCameraActive(const bool active)
{
    m_CameraActive = active;
    Graph::Window_SetCursorMode(m_Window->GetHandle(), active ? Graph::CursorMode_Disabled : Graph::CursorMode_Normal);
}

/// Losing focus or changing size releases the lock; a left click in the scene takes it again.
void Viewer::HandleSceneEvents()
{
    for (const Onyx::Event &event : m_Window->GetNewEvents())
    {
        const bool lost = event.Type == Onyx::Event_WindowUnfocused || event.Type == Onyx::Event_WindowResized;
        const bool clicked = event.Type == Onyx::Event_MousePressed && event.Mouse.Button == Onyx::Mouse_Button1;
        if (lost && m_CameraActive)
            SetCameraActive(false);
        else if (clicked && !m_CameraActive)
        {
            SetCameraActive(true);
            m_AwaitingClickRelease = true;
        }
    }
    if (!m_Window->IsMousePressed(Onyx::Mouse_ButtonLeft))
        m_AwaitingClickRelease = false;
}

/// GLFW reports a key only to the focused window, so a hotkey counts when either window holds it.
bool Viewer::IsKeyDown(const Onyx::Key key) const
{
    return m_Window->IsKeyPressed(key) || m_ToolWindow->IsKeyPressed(key);
}

/// Runs each bound command once per press, because Onyx reports whether a key is held, not when it went down.
void Viewer::HandleKeys()
{
    static constexpr TKit::FixedArray<KeyBinding, KEY_BINDING_COUNT> BINDINGS{{
        {Onyx::Key_Tab,
         [](Viewer &viewer) {
             const bool focused = (viewer.m_Window->GetFlags() & Onyx::WindowFlag_NotFocused) == 0;
             viewer.SetCameraActive(!viewer.m_CameraActive && focused);
         }},
        {Onyx::Key_G,
         [](Viewer &viewer) {
             viewer.m_Gallery.ToggleMode();
             viewer.Home();
         }},
        {Onyx::Key_Left, [](Viewer &viewer) { viewer.StepSelection(-1); }},
        {Onyx::Key_Right, [](Viewer &viewer) { viewer.StepSelection(1); }},
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
        const bool pressed = IsKeyDown(BINDINGS[i].Key);
        const bool triggered = pressed && !m_KeyWasPressed[i];
        m_KeyWasPressed[i] = pressed;
        if (triggered)
            BINDINGS[i].Command(*this);
    }
}

/// Moving onto another grid page frames that page, as G does; moves within a page keep the camera.
void Viewer::StepSelection(const i32 offset)
{
    if (m_Gallery.MoveSelection(offset, m_Params.Gallery))
        Home();
}

/// Rebakes once the shape kit params have stayed unchanged for a moment, so a drag rebakes once it settles.
void Viewer::SettleShapes()
{
    if (!AreParamsEqual(m_Kit, m_SeenShapes))
    {
        m_SeenShapes = m_Kit;
        m_ShapesSettle.Restart();
        return;
    }
    if (AreParamsEqual(m_Kit, m_BakedShapes) || m_ShapesSettle.GetElapsed().AsSeconds() < SHAPES_SETTLE_SECONDS)
        return;

    ApplySchema(m_Kit);
    m_Gallery.Rebake(m_Kit);
    MarkShapesBaked();
}

/// Resets the params on request, then saves them and rebakes from the kept bytes or a fresh capture.
void Viewer::Rebuild(const Rebake rebake)
{
    if (rebake == Rebake::ResetParams)
    {
        m_Params = ViewerParams{};
        m_Kit = ShapeKitParams{};
    }
    Persist();

    if (rebake == Rebake::NewRegion)
    {
        const TKit::Optional<Region> fresh = Region::Capture();
        if (fresh)
            m_Gallery.Bake(*fresh, m_Kit);
        else
        {
            TKIT_LOG_ERROR("[LIM][VIEWER] Could not capture a new memory region, keeping 0x{:016X}",
                           m_Gallery.GetSource());
            m_Gallery.Rebake(m_Kit);
        }
    }
    else
        m_Gallery.Rebake(m_Kit);

    if (rebake == Rebake::SameBytes)
        m_Clock.Reset();
    MarkShapesBaked();
}

void Viewer::MarkShapesBaked()
{
    m_BakedShapes = m_Kit;
    m_SeenShapes = m_Kit;
}

/// Sanitizes both roots and writes the viewer's and the kit's tuning files.
void Viewer::Persist()
{
    ApplySchema(m_Params);
    ApplySchema(m_Kit);
    if (!SaveParams(LIM_SHAPES_DEFAULTS_PATH, m_Params))
        TKIT_LOG_ERROR("[LIM][VIEWER] Could not save the viewer params to {}", LIM_SHAPES_DEFAULTS_PATH);
    if (!SaveShapeKitParams(m_Kit))
        TKIT_LOG_ERROR("[LIM][VIEWER] Could not save the shape kit params to {}", LIM_SHAPE_KIT_DEFAULTS_PATH);
}

void Viewer::DrawScene()
{
    const ViewerLook &look = m_Params.Look;
    m_Window->ClearColor = look.Colors.Background;
    m_View->ClearColor = look.Colors.Background;
    m_ToolWindow->ClearColor = look.Colors.UiBackground;

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
    if (m_Panel.Begin(ToUiTheme(m_Params.Look.Colors), m_Params.Look.TextSize, 1.f))
    {
        const u32 record = m_Gallery.GetSelection();
        const FrameTimings::Line timings = m_Timings.Describe();
        const RecordGallery::Page page = m_Gallery.GetPage(m_Params.Gallery);
        m_Hud.Draw(m_Panel, RecordHud::Readout{.Record = record,
                                               .Bytes = m_Gallery.GetRecord(record),
                                               .Source = m_Gallery.GetSource(),
                                               .GalleryMode = m_Gallery.GetModeName(),
                                               .CameraMode = m_Cameras.Get(m_CameraKind).GetName(),
                                               .Page = page.Index,
                                               .PageCount = page.Count,
                                               .Seconds = m_Clock.GetSeconds(),
                                               .TimeScale = m_Clock.GetScale(),
                                               .Paused = m_Clock.IsPaused(),
                                               .Timings = timings});
        if (m_Panel.CornerButton("RESET"))
            requested = Rebake::ResetParams;
        m_Panel.BeginTabs();
        m_Panel.Tabs(m_Params, m_Defaults, ViewerVisibility{}, VIEWER_TABS);
        m_Panel.Tabs(m_Kit, m_KitDefaults, ViewerVisibility{}, KIT_TABS);
        m_Panel.EndTabs();
        const TKit::Optional<u32> pressed = m_Panel.Buttons(BUTTON_LABELS);
        if (pressed)
            requested = BUTTON_REBAKES[*pressed];
    }
    m_Panel.End();
    return requested;
}
} // namespace Lim

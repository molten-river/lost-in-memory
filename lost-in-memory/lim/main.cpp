#include "onyx/onyx.hpp"
#include "onyx/platform.hpp"

using Onyx::D3;
using namespace TKit::Alias;

namespace
{
constexpr const char *WINDOW_TITLE = "Lost in Memory";
constexpr Onyx::WindowFlags BORDERLESS_FLAGS = Onyx::WindowFlag_Visible | Onyx::WindowFlag_Focused |
                                               Onyx::WindowFlag_FocusOnShow | Onyx::WindowFlag_InstallCallbacks;

/// A borderless window covering the primary monitor, or Onyx's default decorated window when no monitor is reported.
Onyx::WindowSpecs MakeWindowSpecs()
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

int main()
{
    Onyx::Initialize();

    const auto &defaultResources = Onyx::Resources::CreateDefaultResources();
    auto sphereMesh = Onyx::CreateSphereMeshData(128, 256);
    sphereMesh.Flags &= ~Onyx::MeshDataFlag_BackCulled;
    const auto sphereHandle = Onyx::Resources::RegisterMesh(defaultResources.GetStaticPool<D3>(), sphereMesh);
    const auto materialHandle = Onyx::Resources::RegisterMaterial<D3>();

    Onyx::Resources::Sync(Onyx::SyncFlag_StaticMeshes | Onyx::SyncFlag_Materials);

    Onyx::Window *const window = Onyx::OpenWindow({.Window = MakeWindowSpecs()});
    Onyx::Camera<D3> camera;
    Onyx::RenderView<D3> *const renderView =
        window->CreateRenderView<D3>(&camera, Onyx::RenderViewFlag_NormalizedCoordinates);
    Onyx::RenderContext<D3> *const renderContext = Onyx::CreateRenderContext<D3>();
    renderContext->AddTarget(renderView);
    while (Onyx::Running())
    {
        if (window->IsKeyPressed(Onyx::Key_Escape))
            Onyx::Quit(Onyx::QuitFlag_DestroyWindows);

        window->ControlCamera(Onyx::GetDeltaTime(window), &camera);
        renderContext->Flush();

        renderContext->Material(materialHandle);
        renderContext->RenderFlags(Onyx::RenderModeFlag_Shaded);
        renderContext->DirectionalLight();
        renderContext->Scale(5.f);
        renderContext->StaticMesh(sphereHandle);

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}

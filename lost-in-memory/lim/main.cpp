#include "onyx/onyx.hpp"

using Onyx::D3;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();

    // Resources
    const auto &defResources = Onyx::Resources::CreateDefaultResources();
    auto sphereMesh = Onyx::CreateSphereMeshData(128, 256);
    sphereMesh.Flags &= ~Onyx::MeshDataFlag_BackCulled;
    const auto sphereHandle = Onyx::Resources::RegisterMesh(defResources.GetStaticPool<D3>(), sphereMesh);
    const auto materialHandle = Onyx::Resources::RegisterMaterial<D3>();

    Onyx::Resources::Sync(Onyx::SyncFlag_StaticMeshes | Onyx::SyncFlag_Materials);

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});
    Onyx::Camera<D3> cam;
    Onyx::RenderView<D3> *r3 = win->CreateRenderView<D3>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);
    Onyx::RenderContext<D3> *rc3 = Onyx::CreateRenderContext<D3>();
    rc3->AddTarget(r3);
    while (Onyx::Running())
    {
        win->ControlCamera(Onyx::GetDeltaTime(win), &cam);
        rc3->Flush();

        rc3->Material(materialHandle);
        rc3->RenderFlags(Onyx::RenderModeFlag_Shaded);
        rc3->DirectionalLight();
        rc3->Scale(5.f);
        rc3->StaticMesh(sphereHandle);

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}

#include "onyx/onyx.hpp"

using Onyx::D2;
using Onyx::D3;
using namespace TKit::Alias;

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();

    Onyx::Window *win = Onyx::OpenWindow({.Window = {.PresentMode = Onyx::PresentMode_VSync}});

    Onyx::Camera<D2> cam{};
    Onyx::RenderView<D2> *view = win->CreateRenderView<D2>(&cam, Onyx::RenderViewFlag_NormalizedCoordinates);

    Onyx::RenderContext<D2> *ctx = Onyx::CreateRenderContext<D2>();
    ctx->AddTarget(view);

    f32 time = 0.f;
    while (Onyx::Running())
    {
        time += Onyx::GetDeltaTime(win).AsSeconds();
        ctx->Flush();
        ctx->Text("This wont work");

        Onyx::Transfer();
        Onyx::Render();
    }

    Onyx::Terminate();
}

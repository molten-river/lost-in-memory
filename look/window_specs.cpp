#include "look/window_specs.hpp"

#include "graph/platform.hpp"
#include "onyx/core.hpp"

namespace Lim
{
namespace
{
constexpr Onyx::WindowFlags BORDERLESS_FLAGS =
    Onyx::WindowFlag_NoResize | Onyx::WindowFlag_NoDecoration | Onyx::WindowFlag_FocusOnShow;
} // namespace

TKit::Optional<u32v2> GetMainScreenSize()
{
    const Graph::Monitor monitor = Graph::Monitor_GetPrimary();
    if (!Graph::Monitor_IsHandleValid(monitor))
        return TKit::Optional<u32v2>::None();
    const Graph::VideoMode mode = Graph::Monitor_GetVideoMode(monitor);
    if (mode.Size[0] == 0 || mode.Size[1] == 0)
        return TKit::Optional<u32v2>::None();
    return mode.Size;
}

Onyx::WindowSpecs MakeBorderlessWindowSpecs(const char *const title)
{
    Onyx::WindowSpecs specs{.Title = title, .PresentMode = Onyx::PresentMode_VSync};
    const TKit::Optional<u32v2> screen = GetMainScreenSize();
    if (!screen)
        return specs;
    specs.Position = i32v2{0, 0};
    specs.ScreenSize = *screen;
    specs.Flags = BORDERLESS_FLAGS;
    return specs;
}
} // namespace Lim

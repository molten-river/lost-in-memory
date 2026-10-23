#include "look/window_specs.hpp"

#include "onyx/platform.hpp"

namespace Lim
{
namespace
{
constexpr Onyx::WindowFlags BORDERLESS_FLAGS = Onyx::WindowFlag_Visible | Onyx::WindowFlag_Focused |
                                               Onyx::WindowFlag_FocusOnShow | Onyx::WindowFlag_InstallCallbacks;
} // namespace

TKit::Optional<u32v2> GetMainScreenSize()
{
    Onyx_MonitorHandle *const monitor = Onyx::Platform::GetMainMonitor();
    if (!monitor)
        return TKit::Optional<u32v2>::None();
    const Onyx::Platform::VideoMode mode = Onyx::Platform::GetVideoMode(monitor);
    if (mode.Dimensions[0] == 0 || mode.Dimensions[1] == 0)
        return TKit::Optional<u32v2>::None();
    return mode.Dimensions;
}

Onyx::WindowSpecs MakeBorderlessWindowSpecs(const char *const title)
{
    Onyx::WindowSpecs specs{.Title = title, .PresentMode = Onyx::PresentMode_VSync};
    const TKit::Optional<u32v2> screen = GetMainScreenSize();
    if (!screen)
        return specs;
    specs.Position = i32v2{0, 0};
    specs.Dimensions = *screen;
    specs.Flags = BORDERLESS_FLAGS;
    return specs;
}
} // namespace Lim

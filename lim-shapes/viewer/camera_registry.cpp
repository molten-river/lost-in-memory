#include "viewer/camera_registry.hpp"

namespace Lim
{
CameraStrategy &CameraRegistry::Get(const Kind kind)
{
    return *m_Strategies[u32(kind)];
}
} // namespace Lim

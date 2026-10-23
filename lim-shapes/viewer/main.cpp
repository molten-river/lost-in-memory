#include "viewer/viewer.hpp"

#include "onyx/core.hpp"
#include "onyx/resources.hpp"

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();
    {
        Lim::Viewer viewer{};
        viewer.Run();
    }
    Onyx::Terminate();
}

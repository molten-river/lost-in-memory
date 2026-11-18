#include "look/engine_specs.hpp"
#include "shapes/shape_renderer.hpp"
#include "viewer/viewer.hpp"

#include "onyx/core.hpp"
#include "onyx/resources.hpp"

int main()
{
    Onyx::Initialize(Lim::MakeEngineSpecs(Lim::EngineBudget{
        .StaticMeshes = Lim::ShapeRenderer::MAX_MESHES, .StaticPoolVertices = Lim::ShapeRenderer::MAX_POOL_VERTICES}));
    Onyx::Resources_CreateDefault();
    {
        Lim::Viewer viewer{};
        viewer.Run();
    }
    Onyx::Terminate();
}

#include "lim/debug_tools.hpp"
#include "lim/game.hpp"
#include "lim/play_mode.hpp"
#include "lim/shell.hpp"
#include "look/engine_specs.hpp"
#include "shapes/shape_renderer.hpp"

#include "onyx/core.hpp"
#include "onyx/resources.hpp"

#include "tkit/container/span.hpp"
#include "tkit/utils/logging.hpp"

namespace
{
[[nodiscard]] Lim::PlayMode ParseMode([[maybe_unused]] const int argc, [[maybe_unused]] char **const argv)
{
    if constexpr (!Lim::DEBUG_TOOLS)
        return Lim::PlayMode::Main;

    bool testShapes = false;
    bool testSphere = false;
    for (int i = 1; i < argc; ++i)
    {
        const TKit::StringView arg{argv[i]};
        testShapes = testShapes || arg == "--TESTSHAPES";
        testSphere = testSphere || arg == "--TESTSPHERE";
    }
    if (testShapes && testSphere)
    {
        TKIT_LOG_ERROR("[LIM][MAIN] --TESTSHAPES and --TESTSPHERE given together, running Main");
        return Lim::PlayMode::Main;
    }
    if (testShapes)
        return Lim::PlayMode::TestShapes;
    if (testSphere)
        return Lim::PlayMode::TestSphere;
    return Lim::PlayMode::Main;
}
} // namespace

int main(int argc, char **argv)
{
    const Lim::PlayMode mode = ParseMode(argc, argv);

    Onyx::Initialize(Lim::MakeEngineSpecs(Lim::EngineBudget{.StaticMeshes = Lim::ShapeRenderer::MAX_MESHES,
                                                            .StaticPoolVertices = Lim::ShapeRenderer::MAX_VERTICES,
                                                            .DynamicMeshes = 1,
                                                            .DynamicVertices = Lim::Shell::MAX_VERTEX_COUNT}));
    Onyx::Resources_CreateDefault();
    {
        Lim::Game game{mode};
        game.Run();
    }
    Onyx::Terminate();
}

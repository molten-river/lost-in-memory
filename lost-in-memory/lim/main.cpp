#include "lim/game.hpp"

#include "onyx/core.hpp"
#include "onyx/resources.hpp"

int main()
{
    Onyx::Initialize();
    Onyx::Resources::CreateDefaultResources();
    {
        Lim::Game game;
        game.Run();
    }
    Onyx::Terminate();
}

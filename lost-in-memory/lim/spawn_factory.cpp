#include "lim/spawn_factory.hpp"
#include "lim/lattice_spawn.hpp"
#include "lim/record_bytes_spawn.hpp"
#include "lim/scatter_spawn.hpp"

#include "tkit/container/fixed_array.hpp"

namespace Lim
{
namespace
{
const LatticeSpawn s_Lattice{};
const RecordBytesSpawn s_RecordBytes{};
const ScatterSpawn s_Scatter{};

const TKit::FixedArray<const SpawnStrategy *, SPAWN_KIND_COUNT> s_Strategies{&s_Lattice, &s_RecordBytes, &s_Scatter};

static_assert(u32(SpawnKind::Lattice) == 0 && u32(SpawnKind::RecordBytes) == 1 && u32(SpawnKind::Scatter) == 2,
              "s_Strategies lists one strategy per SpawnKind, in order");
} // namespace

const SpawnStrategy &SpawnFactory::Get(const SpawnKind kind)
{
    return *s_Strategies[u32(kind)];
}
} // namespace Lim

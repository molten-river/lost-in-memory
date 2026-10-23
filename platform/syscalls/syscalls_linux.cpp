#include "syscalls/syscalls.hpp"

#include "tkit/container/fixed_array.hpp"

#include <cstring>
#include <fstream>

#include <sys/uio.h>
#include <unistd.h>

namespace Syscalls
{
namespace
{
constexpr const char *MAPS_PATH = "/proc/self/maps";
constexpr usize LINE_CAPACITY = 4352;
constexpr u32 OFFSET_SHIFT = 32;
constexpr TKit::FixedArray<const char *, 3> SKIPPED_MAPPINGS{"[vvar]", "[vvar_vclock]", "[vsyscall]"};

struct Mapping
{
    u64 Begin;
    u64 End;
};

bool IsSkipped(const char *line)
{
    for (const char *skipped : SKIPPED_MAPPINGS)
        if (std::strstr(line, skipped) != nullptr)
            return true;
    return false;
}

bool NextCandidate(std::ifstream &maps, const usz minimumSize, Mapping &mapping)
{
    TKit::FixedArray<char, LINE_CAPACITY> rest;
    char dash = 0;
    char readFlag = 0;
    while (maps >> std::hex >> mapping.Begin >> dash >> mapping.End >> readFlag)
    {
        maps.getline(rest.GetData(), LINE_CAPACITY);
        if (readFlag == 'r' && mapping.End - mapping.Begin >= minimumSize && !IsSkipped(rest.GetData()))
            return true;
    }
    return false;
}
} // namespace

TKit::Optional<u64> ReadProcessMemorySnapshot(const TKit::Span<std::byte> buffer, const u64 pick)
{
    const usz size = buffer.GetSize();
    if (size == 0)
        return TKit::Optional<u64>::None();

    Mapping mapping;
    u64 count = 0;
    {
        std::ifstream maps{MAPS_PATH};
        while (NextCandidate(maps, size, mapping))
            ++count;
    }
    if (count == 0)
        return TKit::Optional<u64>::None();

    std::ifstream maps{MAPS_PATH};
    const u64 target = pick % count;
    for (u64 index = 0; index <= target; ++index)
        if (!NextCandidate(maps, size, mapping))
            return TKit::Optional<u64>::None();

    const u64 slack = mapping.End - mapping.Begin - size + 1;
    const u64 address = mapping.Begin + (pick >> OFFSET_SHIFT) % slack;
    iovec local{buffer.GetData(), size};
    iovec remote{rcast<void *>(address), size};
    const ssize_t copied = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);
    if (copied != scast<ssize_t>(size))
        return TKit::Optional<u64>::None();
    return address;
}
} // namespace Syscalls

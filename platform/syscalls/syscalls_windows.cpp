#include "syscalls/syscalls.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace Syscalls
{
namespace
{
constexpr DWORD READABLE_PROTECTIONS = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                                       PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
constexpr DWORD UNREADABLE_PROTECTIONS = PAGE_GUARD | PAGE_NOACCESS;
constexpr u32 OFFSET_SHIFT = 32;

bool IsCandidate(const MEMORY_BASIC_INFORMATION &info, const usz minimumSize)
{
    return info.State == MEM_COMMIT && (info.Protect & READABLE_PROTECTIONS) != 0 &&
           (info.Protect & UNREADABLE_PROTECTIONS) == 0 && info.RegionSize >= minimumSize;
}

bool NextCandidate(uptr &cursor, const uptr end, const usz minimumSize, MEMORY_BASIC_INFORMATION &info)
{
    while (cursor < end && VirtualQuery(rcast<LPCVOID>(cursor), &info, sizeof(info)) == sizeof(info))
    {
        cursor = rcast<uptr>(info.BaseAddress) + info.RegionSize;
        if (IsCandidate(info, minimumSize))
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

    SYSTEM_INFO system;
    GetSystemInfo(&system);
    const uptr begin = rcast<uptr>(system.lpMinimumApplicationAddress);
    const uptr end = rcast<uptr>(system.lpMaximumApplicationAddress);

    MEMORY_BASIC_INFORMATION info;
    uptr cursor = begin;
    u64 count = 0;
    while (NextCandidate(cursor, end, size, info))
        ++count;
    if (count == 0)
        return TKit::Optional<u64>::None();

    cursor = begin;
    const u64 target = pick % count;
    for (u64 index = 0; index <= target; ++index)
        if (!NextCandidate(cursor, end, size, info))
            return TKit::Optional<u64>::None();

    const usz slack = info.RegionSize - size + 1;
    const u64 address = rcast<uptr>(info.BaseAddress) + (pick >> OFFSET_SHIFT) % slack;
    SIZE_T copied = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), rcast<LPCVOID>(address), buffer.GetData(), size, &copied) ||
        copied != size)
        return TKit::Optional<u64>::None();
    return address;
}
} // namespace Syscalls

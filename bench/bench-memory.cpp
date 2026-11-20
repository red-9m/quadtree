// Global operator new/delete replacement counting allocations and live bytes.
// Kept in its own translation unit so callers cannot inline it.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "bench-memory.h"

namespace
{
std::size_t gAllocations = 0;
std::size_t gLiveBytes = 0;
std::size_t gPeakBytes = 0;
constexpr std::size_t kHeader = alignof(std::max_align_t);
} // namespace

namespace bench::memory
{
std::size_t allocations() noexcept { return gAllocations; }
std::size_t liveBytes() noexcept { return gLiveBytes; }
std::size_t peakBytes() noexcept { return gPeakBytes; }
void resetPeak() noexcept { gPeakBytes = gLiveBytes; }

// Layout: [raw ... | raw pointer | size][aligned block]
void* allocateAligned(std::size_t size, std::size_t alignment) noexcept
{
    constexpr std::size_t header = 2 * sizeof(std::size_t);
    alignment = std::max(alignment, alignof(std::max_align_t));
    void* raw = std::malloc(size + header + alignment);
    if (!raw)
        return nullptr;
    const auto start = reinterpret_cast<std::uintptr_t>(raw) + header;
    const std::uintptr_t aligned = (start + alignment - 1) & ~(std::uintptr_t(alignment) - 1);
    unsigned char* block = reinterpret_cast<unsigned char*>(aligned);
    std::memcpy(block - header, &raw, sizeof(raw));
    std::memcpy(block - sizeof(std::size_t), &size, sizeof(size));
    ++gAllocations;
    gLiveBytes += size;
    gPeakBytes = std::max(gPeakBytes, gLiveBytes);
    return block;
}

void freeAligned(void* ptr) noexcept
{
    if (!ptr)
        return;
    unsigned char* block = static_cast<unsigned char*>(ptr);
    void* raw = nullptr;
    std::size_t size = 0;
    std::memcpy(&raw, block - 2 * sizeof(std::size_t), sizeof(raw));
    std::memcpy(&size, block - sizeof(std::size_t), sizeof(size));
    gLiveBytes -= size;
    std::free(raw);
}
} // namespace bench::memory

void* operator new(std::size_t size)
{
    void* raw = std::malloc(size + kHeader);
    if (!raw)
        throw std::bad_alloc();
    std::memcpy(raw, &size, sizeof(size));
    ++gAllocations;
    gLiveBytes += size;
    gPeakBytes = std::max(gPeakBytes, gLiveBytes);
    return static_cast<unsigned char*>(raw) + kHeader;
}

void operator delete(void* ptr) noexcept
{
    if (!ptr)
        return;
    unsigned char* raw = static_cast<unsigned char*>(ptr) - kHeader;
    std::size_t size = 0;
    std::memcpy(&size, raw, sizeof(size));
    gLiveBytes -= size;
    std::free(raw);
}

void* operator new[](std::size_t size) { return operator new(size); }
void operator delete[](void* ptr) noexcept { operator delete(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { operator delete(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { operator delete(ptr); }

// The nothrow forms must share the header layout: sanitizers replace the
// default nothrow operator new, which would bypass operator new(size) and
// pair foreign blocks with the operator delete above (std::stable_sort's
// temporary buffer, for example).
void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    void* raw = std::malloc(size + kHeader);
    if (!raw)
        return nullptr;
    std::memcpy(raw, &size, sizeof(size));
    ++gAllocations;
    gLiveBytes += size;
    gPeakBytes = std::max(gPeakBytes, gLiveBytes);
    return static_cast<unsigned char*>(raw) + kHeader;
}

void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept { return operator new(size, tag); }
void operator delete(void* ptr, const std::nothrow_t&) noexcept { operator delete(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept { operator delete(ptr); }

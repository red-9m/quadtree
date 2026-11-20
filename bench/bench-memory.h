#pragma once

#include <cstddef>

// Counters of the global operator new/delete replacement (bench-memory.cpp).
// Single-threaded use only. Aligned operator new is not counted.
namespace bench::memory
{
std::size_t allocations() noexcept;
std::size_t liveBytes() noexcept;
std::size_t peakBytes() noexcept;
void resetPeak() noexcept;

// Counted allocation with power-of-two alignment (Box2D allocator hook).
void* allocateAligned(std::size_t size, std::size_t alignment) noexcept;
void freeAligned(void* ptr) noexcept;
} // namespace bench::memory

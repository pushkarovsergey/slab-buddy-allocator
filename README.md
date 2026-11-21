# Slab + Buddy Memory Allocator (C++20)

A custom two-tier memory allocator written in modern C++20, modeled after the Linux kernel memory management architecture (Page-level **Buddy Allocator** + Object-level **Slab Cache**). Designed to operate over a caller-provided memory arena (`std::span<std::byte>`) with zero hidden OS heap allocations.

## Architecture

The allocator combines two complementary strategies behind a unified `MemoryPool` facade:

```text
                         MemoryPool::allocate(bytes)
                                      │
                   ┌──────────────────┴──────────────────┐
                   │ bytes <= 1024 B                     │ bytes > 1024 B
                   ▼                                     ▼
        O(1) Size-Class Routing                   Direct Page Allocation
    (align_to_power_two + countr_zero)                   │
                   │                                     │
                   ▼                                     │
      SlabCache[0..6] (16 B .. 1024 B)                   │
      • Intrusive FreeSlot lists                         │
      • 32-byte SlabHeader per 4 KiB page                │
      • Lazy page acquisition & automatic return         │
                   │                                     │
                   └──────────────────┬──────────────────┘
                                      ▼
                        BuddyAllocator (4 KiB Pages)
                        • Power-of-two block splitting
                        • O(1) XOR-based buddy coalescing
                        • Fixed backing arena (std::span<std::byte>)
```
## Core Components
1. **`BuddyAllocator`** (`buddy_allocator.hpp/.cpp`)
   * Manages a contiguous memory arena divided into `4096`-byte pages.
   * Handles power-of-two multi-page allocations (`order 0` to `order 10`) using segregated doubly-linked free lists.
   * Performs binary block splitting on allocation and $O(1)$ XOR-based neighbor coalescing (`page_index ^ (1ULL << order)`) on deallocation to prevent external fragmentation.

2. **`SlabCache`** (`slab_cache.hpp/.cpp`)
   * Allocates fixed-size objects from `4096`-byte pages requested lazily from `BuddyAllocator`.
   * Embeds a `32`-byte `SlabHeader` at the start of each page and stores an intrusive singly-linked `FreeSlot` list directly inside unused slots (zero per-object metadata overhead).
   * Tracks page lifecycle across `partial_slabs_` and `full_slabs_` lists; automatically returns completely empty pages (`in_use == 0`) back to `BuddyAllocator`.
   * Includes pointer validation: rejects out-of-range pointers, header addresses, misaligned pointers, and double-free attempts.

3. **`MemoryPool`** (`memory_pool.hpp/.cpp`)
   * Exposes a single `allocate(bytes)` / `deallocate(ptr)` interface over 7 slab size classes (`16, 32, 64, 128, 256, 512, 1024` bytes) plus direct `BuddyAllocator` fallback for requests `> 1024` bytes.
   * Uses C++20 `<bit>` utilities (`std::bit_ceil`, `std::countr_zero`) for branchless $O(1)$ size-class lookup.

## Project Structure

```text
slab-buddy-allocator/
├── include/
│   ├── common.hpp            # Page constants and C++20 bit utilities
│   ├── buddy_allocator.hpp   # Page-level buddy allocator interface
│   ├── slab_cache.hpp        # Fixed-size slab cache interface
│   └── memory_pool.hpp       # Top-level hybrid allocator facade
├── src/
│   ├── buddy_allocator.cpp
│   ├── slab_cache.cpp
│   ├── memory_pool.cpp
│   └── main.cpp              # Interactive memory layout & placement-new demo
├── tests/
│   ├── test_buddy.cpp        # Unit tests for BuddyAllocator
│   ├── test_slab.cpp         # Unit tests for SlabCache
│   └── test_memory_pool.cpp  # Integration & edge-case tests for MemoryPool
└── benchmarks/
    └── bench_allocator.cpp   # Throughput comparison vs ::operator new/delete
```

## Build & Run

### Prerequisites
* C++20 compatible compiler (tested with GCC on MSYS2 UCRT64)
* CMake 3.20+
* Ninja (or Make)

### Build (Release)
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run Unit Tests (GoogleTest)
```bash
ctest --test-dir build --output-on-failure
```

The test suite covers 20 test cases across all three layers: power-of-two splitting/coalescing, slab state transitions (`partial` ↔ `full` ↔ `Buddy`), out-of-memory handling and recovery, move semantics, misaligned pointers, double-free protection, and slab-header address safety.

### Run Demo & Benchmarks

```bash
./build/src/demo_allocator
./build/benchmarks/bench_allocator
```

## Usage Example

```Cpp
#include <cstddef>
#include <new>
#include <vector>
#include "memory_pool.hpp"

struct Session {
    std::uint64_t id;
    double balance;
};

int main() {
    // Pre-allocate a 64 KiB backing arena (16 pages)
    std::vector<std::byte> arena(16 * allocator::PAGE_SIZE);
    allocator::MemoryPool pool(arena);

    // Small allocation (routes to 16-byte SlabCache) + placement new
    void* raw = pool.allocate(sizeof(Session));
    auto* session = new (raw) Session{42, 1500.0};

    // Large allocation (routes directly to BuddyAllocator)
    void* buffer = pool.allocate(12288); // Rounded up to 16 KiB (4 pages)

    // Cleanup
    session->~Session();
    pool.deallocate(session);
    pool.deallocate(buffer);
}
```

## Benchmarks

Measured on Windows (GCC UCRT64, `-O3 -DNDEBUG`) with a 16 MiB pre-allocated `MemoryPool` against standard `::operator new` / `::operator delete`:

| Workload | `MemoryPool` | `::operator new/delete` | Speedup |
| :--- | ---: | ---: | ---: |
| **Small Objects** (1M ops, 64 B batches) | ~31.1 ms | ~36.8 ms | **~1.18x** |
| **Large Pages** (256K ops, 8 KiB batches) | ~4.9 ms | 40.3 – 135.8 ms | **8.2x – 29.2x** |
| **Mixed Sizes** (512K ops, 16 B – 4 KiB) | ~19.8 ms | ~12.5 ms | **~0.63x** |

### Performance Analysis & Engineering Trade-offs

* **Deterministic Large-Block Performance (8x–29x speedup):** `BuddyAllocator` operates strictly in $O(1)$ without mutexes or OS syscalls, keeping latency rock-steady (~4.9 ms), whereas the OS heap exhibits significant latency jitter (40–135 ms).
* **Deallocation Overhead in Mixed/Small Workloads:** While `allocate()` runs in $O(1)$, `deallocate()` currently trades raw throughput for strict runtime safety without external hash tables:
  1. `MemoryPool::deallocate` probes active `SlabCache` instances sequentially before falling back to `BuddyAllocator`.
  2. `SlabCache::is_valid_slot_ptr` linearly scans `slab->free_list` to catch double-free errors even in Release builds.
* **Internal Fragmentation:** Power-of-two sizing eliminates external fragmentation and per-object headers at the cost of internal fragmentation (most noticeable just above `MAX_SLAB_SIZE`, where a 1025 B request consumes a 4096 B page).

## Planned Improvements

* **O(1) Deallocation Routing:** Store an owner tag / cache index in `BuddyAllocator`'s per-page metadata array (`pages_[page_index]`) to eliminate linear cache probing in `MemoryPool::deallocate`.
* **O(1) Double-Free Detection:** Replace the linear `free_list` scan with a per-slab occupancy bitmask (or guard the scan behind `#ifndef NDEBUG`).
* **Empty-Slab Caching (Hysteresis):** Retain one empty slab per cache instead of immediately returning pages to `BuddyAllocator` on `in_use == 0` to avoid page thrashing under bursty workloads.
* **STL Integration:** Add a `std::pmr::memory_resource` adapter for seamless use with `std::pmr::vector`, `std::pmr::list`, and `std::pmr::unordered_map`.
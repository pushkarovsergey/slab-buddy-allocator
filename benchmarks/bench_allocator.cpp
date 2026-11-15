#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <new>
#include <vector>

#include "memory_pool.hpp"

namespace {
    using Clock = std::chrono::high_resolution_clock;

    void print_result(const char* name, double pool_ms, double std_ms) {
        const double speedup = std_ms / pool_ms;
        std::cout << std::left << std::setw(36) << name
                  << " | MemoryPool: " << std::right << std::setw(8) << std::fixed << std::setprecision(2) << pool_ms << " ms"
                  << " | std::new/delete: " << std::setw(8) << std_ms << " ms"
                  << " | Speedup: " << std::setprecision(2) << speedup << "x\n";
    }

    void bench_small_objects(allocator::MemoryPool& pool) {
        constexpr std::size_t ITERATIONS = 1000;
        constexpr std::size_t BATCH_SIZE = 1000;
        constexpr std::size_t OBJ_SIZE = 64;

        std::vector<void*> ptrs(BATCH_SIZE);

        auto start_pool = Clock::now();
        for(std::size_t it = 0; it < ITERATIONS; ++it) {
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                ptrs[i] = pool.allocate(OBJ_SIZE);
            }
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                pool.deallocate(ptrs[i]);
            }
        }
        auto end_pool = Clock::now();

        auto start_std = Clock::now();
        for (std::size_t it = 0; it < ITERATIONS; ++it) {
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                ptrs[i] = ::operator new(OBJ_SIZE);
            }
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                ::operator delete(ptrs[i]);
            }
        }
        auto end_std = Clock::now();

        double pool_ms = std::chrono::duration<double, std::milli>(end_pool - start_pool).count();
        double std_ms = std::chrono::duration<double, std::milli>(end_std - start_std).count();
        print_result("1. Small Objects (1M ops, 64B)", pool_ms, std_ms);
    }

    void bench_large_objects(allocator::MemoryPool& pool) {
        constexpr std::size_t ITERATIONS = 2000;
        constexpr std::size_t BATCH_SIZE = 128;
        constexpr std::size_t OBJ_SIZE = 4096;

        std::vector<void*> ptrs(BATCH_SIZE);

        auto start_pool = Clock::now();
        for(std::size_t it = 0; it < ITERATIONS; ++it) {
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                ptrs[i] = pool.allocate(OBJ_SIZE);
            }
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                pool.deallocate(ptrs[i]);
            }
        }
        auto end_pool = Clock::now();

        auto start_std = Clock::now();
        for (std::size_t it = 0; it < ITERATIONS; ++it) {
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                ptrs[i] = ::operator new(OBJ_SIZE);
            }
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                ::operator delete(ptrs[i]);
            }
        }
        auto end_std = Clock::now();

        double pool_ms = std::chrono::duration<double, std::milli>(end_pool - start_pool).count();
        double std_ms = std::chrono::duration<double, std::milli>(end_std - start_std).count();
        print_result("2. Large Pages (256K ops, 8KiB)", pool_ms, std_ms);
    }

    void bench_mixed_workload(allocator::MemoryPool& pool) {
        constexpr std::size_t ITERATIONS = 2000;
        constexpr std::size_t BATCH_SIZE = 128;
        constexpr std::size_t SIZES[] = {16, 32, 64, 128, 256, 512, 1024, 4096};

        std::vector<void*> ptrs(BATCH_SIZE);

        auto start_pool = Clock::now();
        for(std::size_t it = 0; it < ITERATIONS; ++it) {
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                ptrs[i] = pool.allocate(SIZES[i & 7]);
            }
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                pool.deallocate(ptrs[i]);
            }
        }
        auto end_pool = Clock::now();

        auto start_std = Clock::now();
        for (std::size_t it = 0; it < ITERATIONS; ++it) {
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                ptrs[i] = ::operator new(SIZES[i & 7]);
            }
            for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
                ::operator delete(ptrs[i]);
            }
        }
        auto end_std = Clock::now();

        double pool_ms = std::chrono::duration<double, std::milli>(end_pool - start_pool).count();
        double std_ms = std::chrono::duration<double, std::milli>(end_std - start_std).count();
        print_result("3. Mixed Sizes (512K ops, 16B-4KiB)", pool_ms, std_ms);
    }
}

int main() {
    constexpr std::size_t POOL_SIZE = 4096 * allocator::PAGE_SIZE;\
    std::vector<std::byte> buffer(POOL_SIZE);\
    allocator::MemoryPool pool(buffer);

    bench_small_objects(pool);
    bench_large_objects(pool);
    bench_mixed_workload(pool);

    return 0;
}
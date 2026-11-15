#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "memory_pool.hpp"

namespace allocator {

class MemoryPoolTest : public ::testing::Test {
protected:
    static constexpr std::size_t POOL_PAGES = 64;
    static constexpr std::size_t POOL_SIZE = POOL_PAGES * PAGE_SIZE;

    void SetUp() override {
        buffer_.resize(POOL_SIZE);
    }

    std::vector<std::byte> buffer_;
};

TEST_F(MemoryPoolTest, SlabIndexMappindIsCorrect) {
    EXPECT_EQ(MemoryPool::slab_index(1), 0);
    EXPECT_EQ(MemoryPool::slab_index(8), 0);
    EXPECT_EQ(MemoryPool::slab_index(16), 0);

    EXPECT_EQ(MemoryPool::slab_index(17), 1);
    EXPECT_EQ(MemoryPool::slab_index(32), 1);

    EXPECT_EQ(MemoryPool::slab_index(33), 2);
    EXPECT_EQ(MemoryPool::slab_index(64), 2);

    EXPECT_EQ(MemoryPool::slab_index(65), 3);
    EXPECT_EQ(MemoryPool::slab_index(128), 3);

    EXPECT_EQ(MemoryPool::slab_index(129), 4);
    EXPECT_EQ(MemoryPool::slab_index(256), 4);

    EXPECT_EQ(MemoryPool::slab_index(257), 5);
    EXPECT_EQ(MemoryPool::slab_index(512), 5);

    EXPECT_EQ(MemoryPool::slab_index(513), 6);
    EXPECT_EQ(MemoryPool::slab_index(1024), 6);

}

TEST_F(MemoryPoolTest, SmallAllocationsRouteToCorrectSlabCache) {
    MemoryPool pool(buffer_);

    const std::size_t initial_free = pool.free_bytes();

    void* p32 = pool.allocate(24);
    ASSERT_NE(p32, nullptr);
    EXPECT_EQ(pool.slab_allocated_objects(1), 1);
    EXPECT_EQ(pool.total_slab_allocated_objects(), 1);
    EXPECT_EQ(pool.free_bytes(), initial_free - PAGE_SIZE);

    void* p32_2 = pool.allocate(30);
    ASSERT_NE(p32_2, nullptr);
    EXPECT_NE(p32, p32_2);
    EXPECT_EQ(pool.slab_allocated_objects(1), 2);
    EXPECT_EQ(pool.free_bytes(), initial_free - PAGE_SIZE);

    void* p1024 =   pool.allocate(1000);
    ASSERT_NE(p1024, nullptr);
    EXPECT_EQ(pool.slab_allocated_objects(6), 1);
    EXPECT_EQ(pool.total_slab_allocated_objects(), 3);
    EXPECT_EQ(pool.free_bytes(), initial_free - 2 * PAGE_SIZE);

    pool.deallocate(p32);
    EXPECT_EQ(pool.slab_allocated_objects(1), 1);

    pool.deallocate(p32_2);
    EXPECT_EQ(pool.slab_allocated_objects(1), 0);
    EXPECT_EQ(pool.free_bytes(), initial_free - PAGE_SIZE);

    pool.deallocate(p1024);
    EXPECT_EQ(pool.slab_allocated_objects(6), 0);
    EXPECT_EQ(pool.total_slab_allocated_objects(), 0);
    EXPECT_EQ(pool.free_bytes(), initial_free);
}

TEST_F(MemoryPoolTest, AllSevenSlabClassesAllocateAndDeallocateCleanly) {
    MemoryPool pool(buffer_);

    std::vector<void*> ptrs;
    const std::size_t request_sizes[] = {7, 25, 60, 100, 200, 400, 900};

    for (std::size_t i = 0; i < MemoryPool::SLAB_CLASS_COUNT; ++i) {
        void* p = pool.allocate(request_sizes[i]);
        ASSERT_NE(p, nullptr);
        std::memset(p, static_cast<int>(i + 1), request_sizes[i]); 
        EXPECT_EQ(pool.slab_allocated_objects(i), 1);
        ptrs.push_back(p);
    }

    EXPECT_EQ(pool.total_slab_allocated_objects(), MemoryPool::SLAB_CLASS_COUNT);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE - MemoryPool::SLAB_CLASS_COUNT * PAGE_SIZE);

    for (void* p : ptrs) {
        pool.deallocate(p);
    }

    EXPECT_EQ(pool.total_slab_allocated_objects(), 0);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE);
}

TEST_F(MemoryPoolTest, LargeAllocationsRouteDirecltlyToBuddyAllocator) {
    MemoryPool pool(buffer_);

    void* large1 = pool.allocate(1025);
    ASSERT_NE(large1, nullptr);
    EXPECT_EQ(pool.total_slab_allocated_objects(), 0);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE - PAGE_SIZE);

    void* large2 = pool.allocate(2048);
    ASSERT_NE(large2, nullptr);
    EXPECT_EQ(pool.total_slab_allocated_objects(), 0);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE - 2 * PAGE_SIZE);

    void* large3 = pool.allocate(10000);
    ASSERT_NE(large3, nullptr);
    EXPECT_EQ(pool.total_slab_allocated_objects(), 0);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE - 6 * PAGE_SIZE);

    pool.deallocate(large1);
    pool.deallocate(large2);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE - 4 * PAGE_SIZE);

    pool.deallocate(large3);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE);
}

TEST_F(MemoryPoolTest, MixedSmallAndLargeAllocationsCoalesceCompletely) {
    MemoryPool pool(buffer_);

    std::vector<void*> small_ptrs;
    for (size_t i = 0; i < 100; ++i) {
        void* p = pool.allocate(48);
        ASSERT_NE(p, nullptr);
        small_ptrs.push_back(p);
    }

    void* large_block = pool.allocate(32 * PAGE_SIZE); // 128 KiB
    ASSERT_NE(large_block, nullptr);

    for(void* p : small_ptrs) {
        pool.deallocate(p);
    }

    pool.deallocate(large_block);

    EXPECT_EQ(pool.total_slab_allocated_objects(), 0);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE);

    void* full_pool_block = pool.allocate(POOL_SIZE);
    ASSERT_NE(full_pool_block, nullptr);
    EXPECT_EQ(pool.free_bytes(), 0);

    pool.deallocate(full_pool_block);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE);
}

TEST_F(MemoryPoolTest, EdgeCasesAndInvalidPointersAreIgnoredSafely) {
    MemoryPool pool(buffer_);

    EXPECT_EQ(pool.allocate(0), nullptr);
    EXPECT_EQ(pool.allocate(POOL_SIZE + 1), nullptr);

    pool.deallocate(nullptr);
    int stack_var = 42;
    pool.deallocate(&stack_var);

    void* p = pool.allocate(128);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(pool.slab_allocated_objects(3), 1);

    auto* misaligned = static_cast<std::byte*>(p) + 3;
    pool.deallocate(misaligned);
    EXPECT_EQ(pool.slab_allocated_objects(3), 1);

    pool.deallocate(p);
    EXPECT_EQ(pool.slab_allocated_objects(3), 0);

    pool.deallocate(p);
    EXPECT_EQ(pool.slab_allocated_objects(3), 0);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE);
}

TEST_F(MemoryPoolTest, PassingSlabPageBaseAddressDoesNotCorruptSlabInBuddy) {
    MemoryPool pool(buffer_);

    void* slot1 = pool.allocate(64);
    void* slot2 = pool.allocate(64);
    ASSERT_NE(slot1, nullptr);
    ASSERT_NE(slot2, nullptr);

    const auto offset = static_cast<std::size_t>(static_cast<std::byte*>(slot1) - buffer_.data());
    void* slab_page_start = buffer_.data() + (offset / PAGE_SIZE) * PAGE_SIZE; 

    pool.deallocate(slab_page_start);

    EXPECT_EQ(pool.slab_allocated_objects(2), 2);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE - PAGE_SIZE);

    pool.deallocate(slot1);
    pool.deallocate(slot2);
    EXPECT_EQ(pool.slab_allocated_objects(2), 0);
    EXPECT_EQ(pool.free_bytes(), POOL_SIZE);
}

TEST_F(MemoryPoolTest, OutOFMemoryAndRecovery) {
    MemoryPool pool(buffer_);

    std::vector<void*> pages;
    while(void* p = pool.allocate(PAGE_SIZE)) {
        pages.push_back(p);
    }

    EXPECT_EQ(pages.size(), POOL_PAGES);
    EXPECT_EQ(pool.free_bytes(), 0);

    EXPECT_EQ(pool.allocate(16), nullptr);
    EXPECT_EQ(pool.allocate(4096), nullptr);

    pool.deallocate(pages.back());
    pages.pop_back();

    void* small_obj = pool.allocate(32);
    ASSERT_NE(small_obj, nullptr);

    pool.deallocate(small_obj);
    for(void* p : pages) {
        pool.deallocate(p);
    }

    EXPECT_EQ(pool.free_bytes(), POOL_SIZE);
}
 
}
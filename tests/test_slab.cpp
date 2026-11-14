#include <gtest/gtest.h>
#include <vector>
#include <cstddef>

#include "buddy_allocator.hpp"
#include "slab_cache.hpp"

namespace allocator::test {

class SlabCacheTest : public ::testing::Test {
protected:
    static constexpr std::size_t TEST_POOL_PAGES = 16;
    static constexpr std::size_t TEST_POOL_SIZE = TEST_POOL_PAGES * PAGE_SIZE;

    std::vector<std::byte> memory_pool_{TEST_POOL_SIZE};
    BuddyAllocator buddy_{memory_pool_};
};

    TEST_F(SlabCacheTest, InitialStateAndSize) {
        SlabCache cache64(buddy_, 64);
        EXPECT_EQ(cache64.object_size(), 64);

        EXPECT_EQ(cache64.objects_per_slab(), 63);
        EXPECT_EQ(cache64.allocated_objects(), 0);
        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE);

        SlabCache cache_small(buddy_, 4);
        EXPECT_EQ(cache_small.object_size(), sizeof(void*));

        SlabCache cache_zero(buddy_, 0);
        EXPECT_EQ(cache_zero.object_size(), 0);
        EXPECT_EQ(cache_zero.allocate(), nullptr);

        SlabCache cache_huge(buddy_, PAGE_SIZE);
        EXPECT_EQ(cache_huge.object_size(), 0);
        EXPECT_EQ(cache_huge.allocate(), nullptr);
    }

    TEST_F(SlabCacheTest, AllocateAndDeallocateSinglSlot) {
        SlabCache cache(buddy_, 64);

        void* ptr = cache.allocate();
        ASSERT_NE(ptr, nullptr);
        EXPECT_TRUE(cache.owns(ptr));
        EXPECT_EQ(cache.allocated_objects(), 1);
        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE - PAGE_SIZE);
    
        cache.deallocate(ptr);
        EXPECT_EQ(cache.allocated_objects(), 0);
        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE);
    }

    TEST_F(SlabCacheTest, FullSlabTransitionAndMultipleSlabs) {
        SlabCache cache(buddy_, 1024);
        ASSERT_EQ(cache.objects_per_slab(), 3);

        void* p1 = cache.allocate();
        void* p2 = cache.allocate();
        void* p3 = cache.allocate();
        ASSERT_NE(p1, nullptr);
        ASSERT_NE(p2, nullptr);
        ASSERT_NE(p3, nullptr);
        EXPECT_EQ(cache.allocated_objects(), 3);
        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE - PAGE_SIZE);

        void* p4 = cache.allocate();
        ASSERT_NE(p4, nullptr);
        EXPECT_EQ(cache.allocated_objects(), 4);
        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE - 2 * PAGE_SIZE);

        cache.deallocate(p2);
        EXPECT_EQ(cache.allocated_objects(), 3);

        cache.deallocate(p1);
        cache.deallocate(p3);

        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE - PAGE_SIZE);

        cache.deallocate(p4);
        EXPECT_EQ(cache.allocated_objects(), 0);
        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE);
    }

    TEST_F(SlabCacheTest, OutOfMemory) {
        SlabCache cache(buddy_, 1024);
        const std::size_t max_total_objects = TEST_POOL_PAGES * cache.objects_per_slab();

        std::vector<void*> allocated;
        for (std::size_t i = 0; i < max_total_objects; ++i) {
            void* ptr = cache.allocate();
            ASSERT_NE(ptr, nullptr);
            allocated.push_back(ptr);
        }

        EXPECT_EQ(buddy_.free_bytes(), 0);
        EXPECT_EQ(cache.allocate(), nullptr);

        for (void* ptr : allocated) {
            cache.deallocate(ptr);
        }

        EXPECT_EQ(cache.allocated_objects(), 0);
        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE);
    }

    TEST_F(SlabCacheTest, InvalidPointersAndDoubleFree) {
        SlabCache cache(buddy_, 64);

        cache.deallocate(nullptr);
        int stack_var = 123;
        EXPECT_FALSE(cache.owns(&stack_var));
        cache.deallocate(&stack_var);

        auto* p1 = static_cast<std::byte*>(cache.allocate());
        auto* p2 = static_cast<std::byte*>(cache.allocate());
        ASSERT_NE(p1, nullptr);
        ASSERT_NE(p2, nullptr);
        EXPECT_EQ(cache.allocated_objects(), 2);

        cache.deallocate(p1 + 8);
        EXPECT_EQ(cache.allocated_objects(), 2);

        cache.deallocate(p1 - 32);
        EXPECT_EQ(cache.allocated_objects(), 2);

        cache.deallocate(p1);
        EXPECT_EQ(cache.allocated_objects(), 1);

        cache.deallocate(p1);
        EXPECT_EQ(cache.allocated_objects(), 1);

        cache.deallocate(p2);
        EXPECT_EQ(cache.allocated_objects(), 0);
        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE);
    }

    TEST_F(SlabCacheTest, MoveSemanticsAndDestructorCleanup) {
        
        {
            SlabCache temp_cache(buddy_, 256);
            void* ptr = temp_cache.allocate();
            ASSERT_NE(ptr, nullptr);
            EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE - PAGE_SIZE);
        }
        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE);

        SlabCache cache1(buddy_, 128);
        void* ptr = cache1.allocate();
        ASSERT_NE(ptr, nullptr);

        SlabCache cache2(std::move(cache1));
        EXPECT_EQ(cache1.object_size(), 0);
        EXPECT_EQ(cache1.allocated_objects(), 0);
        EXPECT_EQ(cache2.object_size(), 128);
        EXPECT_EQ(cache2.allocated_objects(), 1);
        EXPECT_TRUE(cache2.owns(ptr));

        SlabCache cache3;
        cache3 = std::move(cache2);
        EXPECT_EQ(cache2.allocated_objects(), 0);
        EXPECT_EQ(cache3.allocated_objects(), 1);

        cache3.deallocate(ptr);
        EXPECT_EQ(cache3.allocated_objects(), 0);
        EXPECT_EQ(buddy_.free_bytes(), TEST_POOL_SIZE);
    }
}
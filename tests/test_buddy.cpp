#include <gtest/gtest.h>
#include <vector>
#include <cstddef>

#include "buddy_allocator.hpp"

// ctest --test-dir build --output-on-failure

namespace allocator::test {

class BuddyAllocatorTest : public ::testing::Test {
protected:
    static constexpr std::size_t TEST_POOL_PAGES = 16;
    static constexpr std::size_t TEST_POOL_SIZE = TEST_POOL_PAGES * PAGE_SIZE;

    std::vector<std::byte> memory_pool_{TEST_POOL_SIZE};
    BuddyAllocator alloc_{memory_pool_};
};

TEST_F(BuddyAllocatorTest, InitialState){
    EXPECT_EQ(alloc_.total_bytes(), TEST_POOL_SIZE);
    EXPECT_EQ(alloc_.free_bytes(), TEST_POOL_SIZE);
    EXPECT_EQ(alloc_.total_pages(), TEST_POOL_PAGES);
}

TEST_F(BuddyAllocatorTest, AllocateAndDeallocateSinglePage) {
    void* ptr = alloc_.allocate(100);
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(alloc_.free_bytes(), TEST_POOL_SIZE - PAGE_SIZE);
    alloc_.deallocate(ptr);
    ASSERT_EQ(alloc_.free_bytes(), TEST_POOL_SIZE);
}

TEST_F(BuddyAllocatorTest, SplitAndCoalesce) {
    auto* ptr1 = alloc_.allocate_pages(0);
    auto* ptr2 = alloc_.allocate_pages(0);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_NE(ptr1, ptr2);
    ASSERT_EQ(alloc_.allocate_pages(4), nullptr);
    alloc_.deallocate_pages(ptr1, 0);
    alloc_.deallocate_pages(ptr2, 0);
    ASSERT_NE(alloc_.allocate_pages(4), nullptr);
}

TEST_F(BuddyAllocatorTest, OutOfMemory) {
    for (std::size_t i = 0; i < TEST_POOL_PAGES; ++i){
        ASSERT_NE(alloc_.allocate_pages(0), nullptr);
    }
    EXPECT_EQ(alloc_.free_bytes(), 0);
    ASSERT_EQ(alloc_.allocate_pages(0), nullptr);
}

TEST_F(BuddyAllocatorTest, InvalidInputsAndDoubleFree) {
    int local_var = 42;
    alloc_.deallocate(&local_var);

    auto* ptr = static_cast<std::byte*>(alloc_.allocate(100));
    ASSERT_NE(ptr, nullptr);
    alloc_.deallocate(ptr + 16);
    EXPECT_EQ(alloc_.free_bytes(), TEST_POOL_SIZE - PAGE_SIZE);
    alloc_.deallocate(ptr);
    alloc_.deallocate(ptr);
    ASSERT_EQ(alloc_.free_bytes(), TEST_POOL_SIZE);
    
    ASSERT_EQ(alloc_.allocate(0), nullptr);
    ASSERT_EQ(alloc_.allocate_pages(MAX_ORDER + 1), nullptr);
}

TEST_F(BuddyAllocatorTest, MoveSemantics) {
    void* ptr = alloc_.allocate_pages(0);
    ASSERT_NE(ptr, nullptr);

    BuddyAllocator moved_ctor_alloc_(std::move(alloc_));
    EXPECT_EQ(alloc_.total_bytes(), 0);
    EXPECT_EQ(moved_ctor_alloc_.free_bytes(), TEST_POOL_SIZE - PAGE_SIZE);

    BuddyAllocator moved_assign_alloc_(std::span<std::byte>{} );
    moved_assign_alloc_ = std::move(moved_ctor_alloc_);
    EXPECT_EQ(moved_ctor_alloc_.total_bytes(), 0);
    EXPECT_EQ(moved_assign_alloc_.free_bytes(), TEST_POOL_SIZE - PAGE_SIZE);

    moved_assign_alloc_.deallocate_pages(ptr, 0);
    EXPECT_EQ(moved_assign_alloc_.free_bytes(), TEST_POOL_SIZE);
}

}
#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <span>

#include "common.hpp"

namespace allocator {

class BuddyAllocator {
public:
    explicit BuddyAllocator(std::span<std::byte> pool);

    ~BuddyAllocator() noexcept = default;

    BuddyAllocator(const BuddyAllocator&) = delete;
    BuddyAllocator& operator=(const BuddyAllocator&) = delete;

    BuddyAllocator(BuddyAllocator&&) noexcept;
    BuddyAllocator& operator=(BuddyAllocator&&) noexcept;

    [[nodiscard]] void* allocate(std::size_t bytes) noexcept;
    void deallocate(void* ptr) noexcept;

    [[nodiscard]] void* allocate_pages(std::size_t order) noexcept;
    void deallocate_pages(void* ptr, std::size_t order) noexcept;

    [[nodiscard]] std::size_t total_bytes() const noexcept;
    [[nodiscard]] std::size_t free_bytes() const noexcept;
    [[nodiscard]] std::size_t total_pages() const noexcept;



private:
    struct FreeNode {
        FreeNode* prev{nullptr};
        FreeNode* next{nullptr};
    };

    enum class PageState : std::uint8_t {
        Free,
        Allocated,
        SplitTail
    };

    struct Page {
        PageState state{PageState::Free};
        std::uint8_t order{0};
    };

    [[nodiscard]] FreeNode* split_block(std::size_t current_order, std::size_t target_order) noexcept;
    void coalesce_block(std::size_t page_index, std::size_t order) noexcept;

    void list_push_front(std::size_t order, FreeNode* ptr) noexcept;
    void list_remove(std::size_t order, FreeNode* ptr) noexcept;
    [[nodiscard]] FreeNode* list_pop_front(std::size_t order) noexcept;


    [[nodiscard]] std::size_t ptr_to_page_index(const void* ptr) const noexcept;
    [[nodiscard]] std::byte* page_index_to_ptr(std::size_t index) const noexcept;
    [[nodiscard]] std::size_t get_buddy_index(std::size_t page_index, std::size_t order) const noexcept;


    std::byte* pool_start_{nullptr};
    std::byte* pool_end_{nullptr};
    std::size_t pool_size_{0};

    std::array<FreeNode*, MAX_ORDER + 1> free_lists_{};
    std::vector<Page> pages_;
};

}
#pragma once

#include "common.hpp"
#include "buddy_allocator.hpp"

namespace allocator {

class SlabCache{
public:
    explicit SlabCache(BuddyAllocator& buddy, std::size_t object_size) noexcept;
    ~SlabCache() noexcept;

    SlabCache(const SlabCache& other) = delete;
    SlabCache& operator=(SlabCache& other) = delete;

    SlabCache(SlabCache&& other) noexcept;
    SlabCache& operator=(SlabCache&& other) noexcept;

    [[nodiscard]] void* allocate() noexcept;
    void deallocate(void* ptr) noexcept;

    [[nodiscard]] std::size_t object_size() const noexcept;
    [[nodiscard]] std::size_t objects_per_slab() const noexcept;
    [[nodiscard]] std::size_t allocated_objects() const noexcept;
    [[nodiscard]] bool owns(const void* ptr) const noexcept;



private:
    struct FreeSlot{
        FreeSlot* next{nullptr};
    };
    struct SlabHeader{
        SlabHeader* prev{nullptr};
        SlabHeader* next{nullptr};

        FreeSlot* free_list = nullptr;
        std::size_t in_use = 0;
    };    

    [[nodiscard]] SlabHeader* allocate_slab() noexcept;
    [[nodiscard]] SlabHeader* find_owning_slab(const void* ptr) const noexcept;
    [[nodiscard]] bool is_valid_slot_ptr(const SlabHeader* slab, const void* ptr) const noexcept;

    BuddyAllocator* buddy_{nullptr};
    std::size_t object_size_{0};

    SlabHeader* partial_slabs_{nullptr};
    SlabHeader* full_slabs_{nullptr};
};

}
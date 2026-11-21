#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "common.hpp"
#include "buddy_allocator.hpp"
#include "slab_cache.hpp"

namespace allocator {

class MemoryPool{
public:
    static constexpr std::size_t MIN_SLAB_SIZE = 16;
    static constexpr std::size_t MAX_SLAB_SIZE = 1024;
    static constexpr std::size_t SLAB_CLASS_COUNT = 7;
    
    static constexpr std::array<std::size_t, SLAB_CLASS_COUNT> SLAB_SIZES{
        16, 32, 64, 128, 256, 512, 1024
    };

    explicit MemoryPool(std::span<std::byte> pool) noexcept;
    ~MemoryPool() noexcept = default;

    MemoryPool(const MemoryPool& other) = delete;
    MemoryPool& operator=(MemoryPool& other) = delete;

    MemoryPool(MemoryPool&& other) = delete;
    MemoryPool& operator=(MemoryPool&& other) = delete;

    [[nodiscard]] void* allocate(std::size_t bytes) noexcept;
    void deallocate(void* ptr) noexcept;

    [[nodiscard]] std::size_t total_bytes() const noexcept;
    [[nodiscard]] std::size_t free_bytes() const noexcept;
    [[nodiscard]] std::size_t total_pages() const noexcept;

    [[nodiscard]] std::size_t slab_allocated_objects(std::size_t class_index) const noexcept;
    [[nodiscard]] std::size_t total_slab_allocated_objects() const noexcept;

    [[nodiscard]] static constexpr std::size_t slab_index(std::size_t bytes) noexcept {
        if (bytes <= MIN_SLAB_SIZE) {
            return 0;
        }
        const std::size_t aligned = detail::align_to_power_two(bytes);
        constexpr std::size_t min_shift = static_cast<std::size_t>(std::countr_zero(MIN_SLAB_SIZE));
        return static_cast<std::size_t>(std::countr_zero(aligned)) - min_shift;
    }
private:
    BuddyAllocator buddy_;
    std::array<SlabCache, SLAB_CLASS_COUNT> caches_{};
};
}
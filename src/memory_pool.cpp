#include "memory_pool.hpp"

namespace allocator {

MemoryPool::MemoryPool(std::span<std::byte> pool) noexcept
    : buddy_(pool)
    {
        for (std::size_t i = 0; i < SLAB_CLASS_COUNT; ++i) {
            caches_[i] = SlabCache(buddy_, SLAB_SIZES[i]);
        }
    }

    void* MemoryPool::allocate(std::size_t bytes) noexcept {
        if (bytes == 0) {
            return nullptr;
        }

        if (bytes <= MAX_SLAB_SIZE) {
            const std::size_t index = slab_index(bytes);
            return caches_[index].allocate();
        }

        return buddy_.allocate(bytes);
    }

    void MemoryPool::deallocate(void* ptr) noexcept {
        if (ptr == nullptr) {
            return;
        }

        for (auto& cache : caches_) {
            if (cache.try_deallocate(ptr)) {
                return;
            }
        }
        buddy_.deallocate(ptr);
    }

    std::size_t MemoryPool::total_bytes() const noexcept {
        return buddy_.total_bytes();
    }

    std::size_t MemoryPool::free_bytes() const noexcept {
        return buddy_.free_bytes();
    }

    std::size_t MemoryPool::total_pages() const noexcept {
        return buddy_.total_pages();
    }

    std::size_t MemoryPool::slab_allocated_objects(std::size_t class_index) const noexcept {
        if (class_index >= SLAB_CLASS_COUNT) {
            return 0;
        }
        return caches_[class_index].allocated_objects();
    }

    std::size_t MemoryPool::total_slab_allocated_objects() const noexcept {
        std::size_t total = 0;
        for (const auto& cache : caches_) {
            total += cache.allocated_objects();
        }
        return total;
    }
}


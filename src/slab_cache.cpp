#include "slab_cache.hpp"

#include <algorithm>
#include <utility>

namespace allocator {
    SlabCache::SlabCache(BuddyAllocator& buddy, std::size_t object_size) noexcept
    : buddy_(&buddy),
      object_size_(object_size > 0 ? std::max(object_size, sizeof(FreeSlot)) : 0) 
      {
        if (object_size > PAGE_SIZE - sizeof(SlabHeader)) {
            object_size_ = 0;
        }
      }

    SlabCache::~SlabCache() noexcept {
        release_slabs();
    }

    SlabCache::SlabCache(SlabCache&& other) noexcept 
    : buddy_(std::exchange(other.buddy_, nullptr)),
      object_size_(std::exchange(other.object_size_, 0)),
      partial_slabs_(std::exchange(other.partial_slabs_, nullptr)),
      full_slabs_(std::exchange(other.full_slabs_, nullptr)) {}

    SlabCache& SlabCache::operator=(SlabCache&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        release_slabs();

        buddy_ = std::exchange(other.buddy_, nullptr);
        object_size_ = std::exchange(other.object_size_, 0);
        partial_slabs_ = std::exchange(other.partial_slabs_, nullptr);
        full_slabs_ = std::exchange(other.full_slabs_, nullptr);

        return *this;   
    }

    void* SlabCache::allocate() noexcept {
        if (buddy_ == nullptr || object_size_ == 0) {
            return nullptr;
        }
        if (partial_slabs_ == nullptr) {
            if (allocate_slab() == nullptr) {
                return nullptr;
            }
        }

        SlabHeader* slab = partial_slabs_;
        FreeSlot* slot = slab->free_list;
        slab->free_list = slot->next;
        ++slab->in_use;
        if (slab->free_list == nullptr) {
            list_remove(partial_slabs_, slab);
            list_push_front(full_slabs_, slab);
        }  
        return slot;
    }

    void SlabCache::deallocate(void* ptr) noexcept { 
        SlabHeader* slab = find_owning_slab(ptr);
        if (slab == nullptr || !is_valid_slot_ptr(slab, ptr)) {
            return;
        }
        const bool was_full = (slab->free_list == nullptr);

        auto* slot = static_cast<FreeSlot*>(ptr);
        slot->next = slab->free_list;
        slab->free_list = slot;
        --slab->in_use;

        if (was_full) {
            list_remove(full_slabs_, slab);
            list_push_front(partial_slabs_, slab);
        }

        if (slab->in_use == 0) {
            list_remove(partial_slabs_, slab); 
            buddy_->deallocate_pages(slab, 0);
        }
    }

    std::size_t SlabCache::object_size() const noexcept {
        return object_size_;
    }

    std::size_t SlabCache::objects_per_slab() const noexcept {
        if (object_size_ == 0) {
            return 0;
        }
        return (PAGE_SIZE - sizeof(SlabHeader)) / object_size_;
    }

    std::size_t SlabCache::allocated_objects() const noexcept {
        std::size_t total = 0;
        for (const auto* slab = partial_slabs_; slab != nullptr; slab = slab->next) {
            total += slab->in_use;
        }
        for (const auto* slab = full_slabs_; slab != nullptr; slab = slab->next) {
            total += slab->in_use;
        }

        return total;
    }

    bool SlabCache::owns(const void* ptr) const noexcept {
        return find_owning_slab(ptr) != nullptr;
    }

    void SlabCache::release_slabs() noexcept {
        if (buddy_ == nullptr) {
            return;
        }

        SlabHeader* curr = partial_slabs_;
        while (curr != nullptr) {
            SlabHeader* next = curr->next;
            buddy_->deallocate_pages(curr, 0);
            curr = next;
        }

        curr = full_slabs_;
        while (curr != nullptr) {
            SlabHeader* next = curr->next;
            buddy_->deallocate_pages(curr, 0);
            curr = next;
        }
        partial_slabs_ = nullptr;
        full_slabs_ = nullptr;
    }

    SlabCache::SlabHeader* SlabCache::allocate_slab() noexcept {
        void* raw_page = buddy_->allocate_pages(0);
        if (raw_page == nullptr) {
            return nullptr;
        }
        auto* slab = static_cast<SlabHeader*>(raw_page);
        slab->prev = nullptr;
        slab->next = nullptr;
        slab->in_use = 0;

        auto* first_slot_bytes = static_cast<std::byte*>(raw_page) + sizeof(SlabHeader);
        slab->free_list = reinterpret_cast<FreeSlot*>(first_slot_bytes);

        const std::size_t slot_count = objects_per_slab();
        
        for (std::size_t i = 0; i + 1 < slot_count; ++i){
            auto* current_slot = reinterpret_cast<FreeSlot*>(first_slot_bytes + i * object_size_);
            auto* next_slot = reinterpret_cast<FreeSlot*>(first_slot_bytes + (i + 1) * object_size_);
            current_slot->next = next_slot;
        }
        
        auto* last_slot = reinterpret_cast<FreeSlot*>(first_slot_bytes + (slot_count - 1) * object_size_);
        last_slot->next = nullptr;

        list_push_front(partial_slabs_, slab);
        return slab;
    }

    SlabCache::SlabHeader* SlabCache::find_owning_slab(const void* ptr) const noexcept {
        if (ptr == nullptr) {
            return nullptr;
        }

        const auto* byte_ptr = static_cast<const std::byte*>(ptr);

        for (SlabHeader* slab = partial_slabs_; slab != nullptr; slab = slab->next) {
            const auto* slab_start = reinterpret_cast<const std::byte*>(slab);
            if (byte_ptr >= slab_start && byte_ptr < slab_start + PAGE_SIZE) {
                return slab;
            }
        }

        for (SlabHeader* slab = full_slabs_; slab != nullptr; slab = slab->next) {
            const auto* slab_start = reinterpret_cast<const std::byte*>(slab);
            if (byte_ptr >= slab_start && byte_ptr < slab_start + PAGE_SIZE) {
                return slab;
            }
        }

        return nullptr;
    }

    bool SlabCache::is_valid_slot_ptr(const SlabHeader* slab, const void* ptr) const noexcept {
        const auto* slab_bytes = reinterpret_cast<const std::byte*>(slab);
        const auto* first_slot = slab_bytes + sizeof(SlabHeader);
        const auto* slot_bytes = static_cast<const std::byte*>(ptr);

        if (slot_bytes < first_slot || slot_bytes + object_size_ > slab_bytes + PAGE_SIZE) {
            return false;
        }

        const std::size_t offset = static_cast<std::size_t>(slot_bytes - first_slot);
        if (offset % object_size_ != 0) {
            return false;
        }                                              

        for (const FreeSlot* free_node = slab->free_list; free_node != nullptr; free_node = free_node->next) {
            if (free_node == ptr) {
                return false;
            }
        }
        return true;
    }

    void SlabCache::list_push_front(SlabHeader*& head, SlabHeader* slab) noexcept {
        slab->prev = nullptr;
        slab->next = head;
        if (head != nullptr) {
            head->prev = slab;
        }
        head = slab;
    }

    void SlabCache::list_remove(SlabHeader*& head, SlabHeader* slab) noexcept {
        if (slab->prev != nullptr) {
            slab->prev->next = slab->next;
        } else {
            head = slab->next;
        }

        if (slab->next != nullptr) {
            slab->next->prev = slab->prev;
        } 

        slab->prev = nullptr;
        slab->next = nullptr;
    }

};

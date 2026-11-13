#include "buddy_allocator.hpp"
#include <utility>
#include <algorithm>

namespace allocator{
    BuddyAllocator::BuddyAllocator(std::span<std::byte> pool)
    : pool_start_(pool.data()),
      pool_end_(pool.data() + pool.size()),
      pool_size_(pool.size())
    {
        std::size_t total_pages = pool_size_ / PAGE_SIZE;
        pages_.resize(total_pages);

        std::byte* current_ptr = pool_start_;
        std::size_t remaining_bytes = pool_size_;

        std::size_t order = MAX_ORDER;
        
        while (remaining_bytes >= PAGE_SIZE) {
            while (order > 0 && detail::order_to_size(order) > remaining_bytes){
                order--;
            }
            std::size_t block_size = detail::order_to_size(order); 
            auto* node = reinterpret_cast<FreeNode*>(current_ptr);
            list_push_front(order, node);

            std::size_t index = ptr_to_page_index(current_ptr);
            pages_[index].order = static_cast<std::uint8_t>(order);
            pages_[index].state = PageState::Free;

            current_ptr += block_size;
            remaining_bytes -= block_size;
        }
    }

    BuddyAllocator::BuddyAllocator(BuddyAllocator&& other) noexcept 
    : pool_start_(std::exchange(other.pool_start_, nullptr)),
      pool_end_(std::exchange(other.pool_end_, nullptr)),
      pool_size_(std::exchange(other.pool_size_, 0)),
      free_lists_(other.free_lists_),
      pages_(std::move(other.pages_))
    {
        other.free_lists_.fill(nullptr);
    }

    BuddyAllocator& BuddyAllocator::operator=(BuddyAllocator&& other) noexcept
    {
        if(this == &other) {
            return *this;
        }

        pool_start_ = std::exchange(other.pool_start_, nullptr);
        pool_end_ = std::exchange(other.pool_end_, nullptr);
        pool_size_ = std::exchange(other.pool_size_, 0);

        free_lists_ = other.free_lists_;
        other.free_lists_.fill(nullptr);

        pages_ = std::move(other.pages_);
        return *this;
    }
    
    void* BuddyAllocator::allocate_pages(std::size_t order) noexcept {
        if (order > MAX_ORDER || pool_start_ == nullptr) {
            return nullptr;
        }

        std::size_t current_order = order;
        while (current_order <= MAX_ORDER && free_lists_[current_order] == nullptr) {
            current_order++;
        }
        if (current_order > MAX_ORDER) {
            return nullptr;
        }
        FreeNode* block_ptr = (current_order == order
            ? list_pop_front(current_order)
            : split_block(current_order, order));
        
        std::size_t index = ptr_to_page_index(block_ptr);
        pages_[index].state = PageState::Allocated;        
        pages_[index].order = static_cast<std::uint8_t>(order);
        
        return block_ptr;
    }

    void* BuddyAllocator::allocate(std::size_t bytes) noexcept {
        if (bytes == 0) {
            return nullptr;
        }

        std::size_t order = detail::size_to_order(bytes);

        return allocate_pages(order);
    }

    void BuddyAllocator::deallocate_pages(void* ptr, std::size_t order) noexcept {
        if (order > MAX_ORDER) {
            return;
        }    
        std::size_t page_index = validate_allocated_ptr(ptr);

        if (page_index == pages_.size() || pages_[page_index].order != order) {
            return;
        }
        coalesce_block(page_index, order);
    }

    void BuddyAllocator::deallocate(void* ptr) noexcept {
        std::size_t page_index = validate_allocated_ptr(ptr);

        if (page_index == pages_.size()) {
            return;
        }       
        
        coalesce_block(page_index, pages_[page_index].order);
    }

    std::size_t BuddyAllocator::total_bytes() const noexcept {
        return pool_size_;
    }

    std::size_t BuddyAllocator::free_bytes() const noexcept {
        std::size_t total_free = 0;
        for (std::size_t order = 0; order <= MAX_ORDER; ++order) {
            std::size_t node_count = 0;
            const FreeNode* node = free_lists_[order];
            while (node != nullptr) {
                ++node_count;
                node = node->next;
            }
            total_free += node_count * detail::order_to_size(order);

        }
        return total_free;
    }

    std::size_t BuddyAllocator::total_pages() const noexcept {
        return pages_.size();
    }

    BuddyAllocator::FreeNode* BuddyAllocator::split_block(std::size_t current_order, std::size_t target_order) noexcept {
        FreeNode* block_node = list_pop_front(current_order);
        if (block_node == nullptr)
            return nullptr;

        auto* block_ptr = reinterpret_cast<std::byte*>(block_node);
        
        std::size_t block_index = ptr_to_page_index(block_node);

        while (current_order > target_order) {
            current_order--;

            std::byte* buddy_ptr = block_ptr + detail::order_to_size(current_order);
            auto* buddy_node = reinterpret_cast<FreeNode*>(buddy_ptr    );
            list_push_front(current_order, buddy_node);

            std::size_t buddy_index = block_index + (1ULL << current_order);
            pages_[buddy_index].order = static_cast<std::uint8_t>(current_order);
            pages_[buddy_index].state = PageState::Free;

        }

        return block_node;
    }


        void BuddyAllocator::coalesce_block(std::size_t page_index, std::size_t order) noexcept {
            while (order < MAX_ORDER) {
                std::size_t buddy_index = get_buddy_index(page_index, order);

                if (buddy_index >= pages_.size() || 
                    pages_[buddy_index].state != PageState::Free ||
                    pages_[buddy_index].order != order) {
                    break;
                }

                auto* buddy_node = reinterpret_cast<FreeNode*>(page_index_to_ptr(buddy_index));
                list_remove(order, buddy_node);

                page_index = std::min(page_index, buddy_index);
                order++;
            }
            auto* merged_node = reinterpret_cast<FreeNode*>(page_index_to_ptr(page_index));
            pages_[page_index].order = static_cast<std::uint8_t>(order);
            pages_[page_index].state = PageState::Free;
            list_push_front(order, merged_node);
        }





    BuddyAllocator::FreeNode* BuddyAllocator::list_pop_front(std::size_t order) noexcept {
        FreeNode* head = free_lists_[order];
        if (head == nullptr)
        return nullptr;

        free_lists_[order] = head->next;

        if (free_lists_[order] != nullptr)
        free_lists_[order]->prev = nullptr;

        head->prev = nullptr;
        head->next = nullptr;

        return head;
    }

    void BuddyAllocator::list_remove(std::size_t order, FreeNode* node) noexcept {
        if (node->prev != nullptr) {
            node->prev->next = node->next;
        } else {
            free_lists_[order] = node->next;
        }

        if (node->next != nullptr) {
            node->next->prev = node->prev;
        }

        node->prev = nullptr;
        node->next = nullptr;
    }


    void BuddyAllocator::list_push_front(std::size_t order, FreeNode* node) noexcept
    {
        node->prev = nullptr;
        node->next = free_lists_[order];

        if (free_lists_[order] != nullptr) {
            free_lists_[order]->prev = node;
        }
        
        free_lists_[order] = node;
    }

    std::size_t BuddyAllocator::ptr_to_page_index(const void* ptr) const noexcept {
        const auto* byte_ptr = static_cast<const std::byte*>(ptr);

        std::size_t byte_offset = static_cast<std::size_t>(byte_ptr - pool_start_);

        return byte_offset / PAGE_SIZE;
    }

    std::byte* BuddyAllocator::page_index_to_ptr(std::size_t index) const noexcept {
        return pool_start_ + (index * PAGE_SIZE);
    }

    std::size_t BuddyAllocator::get_buddy_index(std::size_t page_index, std::size_t order) const noexcept {
        return page_index ^ (1ULL << order);
    }

    std::size_t BuddyAllocator::validate_allocated_ptr(const void* ptr) const noexcept {
        if (ptr == nullptr) {
            return pages_.size();
        }
        const auto* byte_ptr = static_cast<const std::byte*>(ptr);
        if (byte_ptr < pool_start_ ||
            byte_ptr >= pool_end_ ||
            (byte_ptr - pool_start_) % PAGE_SIZE != 0) {
                return pages_.size();
        }
        
        std::size_t page_index = ptr_to_page_index(ptr);

        if (pages_[page_index].state != PageState::Allocated) {
            return pages_.size();
        }
        return page_index;
    }
}
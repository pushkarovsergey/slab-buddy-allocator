#pragma once

#include <bit>
#include <cstddef>

namespace allocator
{
    constexpr std::size_t PAGE_SIZE = 4096;

    constexpr std::size_t ALIGNMENT = alignof(std::max_align_t);

    constexpr std::size_t MIN_ORDER = 0;

    constexpr std::size_t MAX_ORDER = 10;


    namespace detail
    {
        [[nodiscard]] constexpr bool is_power_of_two(std::size_t value) noexcept {
            return std::has_single_bit(value);
        }

        [[nodiscard]] constexpr std::size_t align_to_power_two(std::size_t value) noexcept{
            if(value <= 1)
            return 1;
            return std::bit_ceil(value);
        }

        [[nodiscard]] constexpr size_t size_to_order(std::size_t bytes) noexcept {
            if(bytes <= PAGE_SIZE) 
            return MIN_ORDER;       
            constexpr std::size_t PAGE_SHIFT = std::countr_zero(PAGE_SIZE);
            return std::countr_zero(std::bit_ceil(bytes)) - PAGE_SHIFT;     
        }

        [[nodiscard]] constexpr size_t order_to_size(std::size_t order) noexcept {
            return PAGE_SIZE << order;
        }

        [[nodiscard]] constexpr size_t align_up(std::size_t address, std::size_t alignment) noexcept {
            return (address + alignment - 1) & ~(alignment - 1);
        }

        [[nodiscard]] constexpr bool is_alignment(const size_t addr, std::size_t alignment) noexcept {
            return (addr & (alignment - 1)) == 0;
        }

        [[nodiscard]] constexpr std::size_t get_buddy_offset(std::size_t offset, std::size_t block_size) noexcept {
            return offset ^ block_size;
        }
    }
}

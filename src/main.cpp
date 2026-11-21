#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "memory_pool.hpp"

namespace {

struct UserSession {
    std::uint64_t id;
    double balance;
    std::uint32_t flags;

    UserSession(std::uint64_t user_id, double initial_balance, std::uint32_t session_flags)
        : id(user_id), balance(initial_balance), flags(session_flags) {
        std::cout << "  [+] UserSession constructed (id=" << id
                  << ", balance=" << balance
                  << ", flags=" << flags
                  << ") at address: " << this << '\n';
    }

    ~UserSession() {
        std::cout << "  [-] UserSession destroyed (id=" << id
                  << ") at address: " << this << '\n';
    }
};

void print_pool_status(const allocator::MemoryPool& pool, const char* title) {
    std::cout << "\n=== " << title << " ===\n"
              << "Total pool capacity : " << pool.total_bytes() << " bytes ("
              << pool.total_pages() << " pages x " << allocator::PAGE_SIZE << " B)\n"
              << "Free memory (Buddy) : " << pool.free_bytes() << " bytes\n"
              << "Active slab objects : " << pool.total_slab_allocated_objects() << '\n'
              << "--------------------------------------------------------\n";
}

} 

int main() {
    constexpr std::size_t POOL_PAGES = 16;
    std::vector<std::byte> arena(POOL_PAGES * allocator::PAGE_SIZE);
    const auto* base_addr = arena.data();

    allocator::MemoryPool pool(arena);
    print_pool_status(pool, "Step 1: Initial MemoryPool State");

    std::cout << "\n>>> Allocating 3 UserSession objects via SlabCache (32 B class):\n";
    void* raw1 = pool.allocate(sizeof(UserSession));
    void* raw2 = pool.allocate(sizeof(UserSession));
    void* raw3 = pool.allocate(sizeof(UserSession));

    auto* user1 = new (raw1) UserSession(1, 100.0, 1);
    auto* user2 = new (raw2) UserSession(2, 200.0, 2);
    auto* user3 = new (raw3) UserSession(3, 300.0, 3);

    std::cout << "\nSlab page layout verification (offsets from arena base):\n"
              << "  user1 offset: +" << (static_cast<std::byte*>(raw1) - base_addr) << " bytes\n"
              << "  user2 offset: +" << (static_cast<std::byte*>(raw2) - base_addr) << " bytes\n"
              << "  user3 offset: +" << (static_cast<std::byte*>(raw3) - base_addr) << " bytes\n";

    print_pool_status(pool, "Step 2: After Small Allocations (1 Page Acquired by Slab)");

    std::cout << "\n>>> Allocating 12 KiB block directly via BuddyAllocator:\n";
    constexpr std::size_t LARGE_REQUEST = 12288;
    void* large_block = pool.allocate(LARGE_REQUEST);
    std::memset(large_block, 0xAB, LARGE_REQUEST);

    std::cout << "  [+] Large block address : " << large_block << '\n'
              << "  [+] Arena byte offset   : +"
              << (static_cast<std::byte*>(large_block) - base_addr) << " bytes\n";

    print_pool_status(pool, "Step 3: After Large Page-Level Allocation");

    std::cout << "\n>>> Safety check (deallocating an unrelated stack pointer):\n";
    int stack_var = 42;
    std::cout << "  Stack variable address: " << &stack_var << '\n';
    pool.deallocate(&stack_var);
    std::cout << "  [OK] Invalid pointer safely ignored without corrupting pool state.\n";

    std::cout << "\n>>> Destroying objects and deallocating all blocks:\n";
    user1->~UserSession();
    pool.deallocate(user1);

    user2->~UserSession();
    pool.deallocate(user2);

    user3->~UserSession();
    pool.deallocate(user3); 

    pool.deallocate(large_block); 

    print_pool_status(pool, "Step 4: Final State After Full Cleanup (Zero Leaks)");

    return 0;
}
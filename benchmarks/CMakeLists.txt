add_executable(bench_allocator
    bench_allocator.cpp
    ${CMAKE_SOURCE_DIR}/src/buddy_allocator.cpp
    ${CMAKE_SOURCE_DIR}/src/slab_cache.cpp
    ${CMAKE_SOURCE_DIR}/src/memory_pool.cpp
)

target_include_directories(bench_allocator PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(bench_allocator PRIVATE -O3)
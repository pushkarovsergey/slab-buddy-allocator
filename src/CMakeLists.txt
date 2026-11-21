add_library(
    allocator STATIC buddy_allocator.cpp
    slab_cache.cpp
    memory_pool.cpp
)

add_compile_options(
    -Werror

    -Wall
    -Wextra
    -Wpedantic

    -Wcast-align
    -Wcast-qual
    -Wconversion
    -Wctor-dtor-privacy
    -Wenum-compare
    -Wfloat-equal
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Woverloaded-virtual
    -Wredundant-decls
    -Wsign-conversion
    -Wsign-promo
)

target_compile_features(allocator PUBLIC cxx_std_20)

target_include_directories(allocator PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_executable(demo_allocator main.cpp)
target_link_libraries(demo_allocator PRIVATE allocator)
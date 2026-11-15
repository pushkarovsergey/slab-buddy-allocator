include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(test_buddy
    test_buddy.cpp
    test_slab.cpp
    test_memory_pool.cpp
    ${CMAKE_SOURCE_DIR}/src/buddy_allocator.cpp
    ${CMAKE_SOURCE_DIR}/src/slab_cache.cpp
)
target_link_libraries(test_buddy PRIVATE allocator GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_buddy)
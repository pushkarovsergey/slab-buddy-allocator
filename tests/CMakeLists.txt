add_executable(allocator_tests test_buddy.cpp)

target_link_libraries(allocator_tests PRIVATE allocator)
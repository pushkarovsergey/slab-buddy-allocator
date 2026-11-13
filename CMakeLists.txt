cmake_minimum_required(VERSION 3.20)

project(
  allocator
  VERSION 1.0
  LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_EXTENSIONS OFF)

get_directory_property(IS_SUBPROJECT PARENT_DIRECTORY)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

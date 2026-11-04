cmake_minimum_required(VERSION 3.30)

project(miniexec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(miniexec INTERFACE)
target_include_directories(miniexec INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(miniexec INTERFACE cxx_std_26)

add_executable(miniexec_tests tests/main.cpp)
target_link_libraries(miniexec_tests PRIVATE miniexec Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(miniexec_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME miniexec_tests COMMAND miniexec_tests)

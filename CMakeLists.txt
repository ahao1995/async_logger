cmake_minimum_required(VERSION 3.15)

project(async_logger CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

add_compile_options(-Wall -O3 -std=c++17)

add_executable(log_test ./test/test.cc)

target_include_directories(log_test PUBLIC ./include)
target_link_libraries(log_test gtest pthread fmt::fmt-header-only)
add_subdirectory(fmt)


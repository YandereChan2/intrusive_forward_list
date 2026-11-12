cmake_minimum_required(VERSION 3.14)
project(intrusive_forward_list LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(intrusive_forward_list INTERFACE)
target_include_directories(intrusive_forward_list INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_intrusive_forward_list test_intrusive_forward_list.cpp)
target_link_libraries(test_intrusive_forward_list PRIVATE intrusive_forward_list)
add_test(NAME test_intrusive_forward_list COMMAND test_intrusive_forward_list)

if(MSVC)
    target_compile_options(test_intrusive_forward_list PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(test_intrusive_forward_list PRIVATE -Wall -Wextra)
endif()

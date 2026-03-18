cmake_minimum_required(VERSION 3.20)
project(order_system_AB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(order_system_demo
    demo/main.cpp
)

target_include_directories(order_system_demo PRIVATE include)

if (MSVC)
    target_compile_options(order_system_demo PRIVATE /W4 /EHsc)
else()
    target_compile_options(order_system_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

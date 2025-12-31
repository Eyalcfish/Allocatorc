cmake_minimum_required(VERSION 3.10)
project(Allocatorc)

add_executable(Allocatorc
    source/main.cpp
    source/allocator.cpp
)

target_include_directories(Allocatorc PRIVATE source)
target_include_directories(Allocatorc PRIVATE headers)


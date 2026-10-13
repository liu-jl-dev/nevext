cmake_minimum_required(VERSION 3.10)

project(Nevext)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_executable(demo01
    demo/demo01.cpp
)


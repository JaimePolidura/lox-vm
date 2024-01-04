cmake_minimum_required(VERSION 3.27)
project(c C)

set(CMAKE_C_STANDARD 23)

include_directories(src)
add_subdirectory(src)
add_subdirectory(tst)
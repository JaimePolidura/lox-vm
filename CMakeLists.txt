cmake_minimum_required(VERSION 3.27)
project(c C)

set(CMAKE_C_STANDARD 23)

# USING_MS_GC_ALG | USING_GEN_GC_ALG
add_compile_definitions(USING_GEN_GC_ALG)
add_compile_definitions(VM_TEST)

include_directories(src)
add_subdirectory(src)
add_subdirectory(tst)
add_subdirectory(playground)
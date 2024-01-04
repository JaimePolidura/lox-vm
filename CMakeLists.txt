cmake_minimum_required(VERSION 3.27)
project(c C)

set(CMAKE_C_STANDARD 23)

add_executable(c main.c
        src/shared.h
        src/bytecode.h
        src/chunk/chunk.h
        src/chunk/chunk.c
        src/memory/memory.h
        src/memory/memory.c
        src/chunk/chunk_disassembler.h
        src/chunk/chunk_disassembler.c
        src/types/types.h
        src/vm/vm.h
        src/vm/vm.c
        src/compiler/scanner.c
        src/compiler/scanner.h
        src/compiler/compiler.h
        src/compiler/compiler.c
        src/types/strings/string_object.h
        src/types/strings/string_object.c
        src/table/table.h
        src/table/table.c
        tst/test.h
        src/types/array.c
        src/types/array.h
        src/types/strings/string_pool.c
        src/types/strings/string_pool.h
        src/shared.c)

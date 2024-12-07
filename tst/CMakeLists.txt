set(BINARY ${CMAKE_PROJECT_NAME})

file(GLOB_RECURSE SOURCES LIST_DIRECTORIES true *.h *.cpp *.c)

set(SOURCES ${SOURCES}
        scanner_test.h
        ssa/ssa_creation_test.h
        hash_table_test.h
        vm_test.h
        trie_test.h
        stack_test.h
        utils_test.h
        package_test.h
        u64_hash_table_test.h
        vm_inline_test.h
        mark_bitmap_test.h
        u8_set_test.h
        u8_hash_table.h
)

include_directories(${PROJECT_SOURCE_DIR}/src)

add_executable(${BINARY}_tst ${SOURCES})

target_link_libraries(${BINARY}_tst PRIVATE ${BINARY}_lib)

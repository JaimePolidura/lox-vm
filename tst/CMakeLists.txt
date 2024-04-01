set(BINARY ${CMAKE_PROJECT_NAME})

file(GLOB_RECURSE SOURCES LIST_DIRECTORIES true *.h *.cpp *.c)

set(SOURCES ${SOURCES}
        scanner_test.h
        hash_table_test.h
        vm_test.h
        trie_test.h
        stack_test.h
        utils_test.h
        package_test.h
        x64_jit_compiler_test.h
)

include_directories(${PROJECT_SOURCE_DIR}/src)

add_executable(${BINARY}_pg ${SOURCES})

target_link_libraries(${BINARY}_pg PRIVATE ${BINARY}_lib)

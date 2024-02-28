set(BINARY ${CMAKE_PROJECT_NAME})

file(GLOB_RECURSE SOURCES LIST_DIRECTORIES true *.h *.c)

add_executable(${BINARY}_run ${SOURCES}
        main.c)

add_library(${BINARY}_lib STATIC ${SOURCES}
        main.c
        types/native_object.c
        ../tst/main.c)

target_link_libraries(${BINARY}_run PRIVATE ${BINARY}_lib)

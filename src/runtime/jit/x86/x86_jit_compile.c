#include "shared/types/function_object.h"
#include "shared/os/os_utils.h"
#include "compiler/bytecode.h"

#define READ_BYTECODE(pc) (*pc++)
#define READ_U16(pc) \
    (pc += 2, (uint16_t)((pc[-2] << 8) | pc[-1]))

jit_compiled jit_compile(struct function_object * function) {
    uint8_t * executable = allocate_executable(function->chunk.capacity * 2);
    uint8_t * pc = function->chunk.code;

    for(;;){
        switch (READ_BYTECODE(pc)) {
        }
    }

    return NULL;
}
#include "shared.h"
#include "shared/types/types.h"
#include "shared/string_pool.h"
#include "runtime/threads/vm_thread.h"

extern __thread struct vm_thread * self_thread;

lox_value_t addition_lox(lox_value_t a, lox_value_t b) {
    if(IS_NUMBER(a) && IS_NUMBER(b)) {
        return TO_LOX_VALUE_NUMBER(a + b);
    }

    char * a_chars = to_string(a);
    char * b_chars = to_string(b);
    size_t a_length = strlen(a_chars);
    size_t b_length = strlen(b_chars);

    size_t new_length = a_length + b_length; //Include \0
    char * concatenated = malloc(sizeof(char) * (new_length + 1));
    memcpy(concatenated, a_chars, a_length);
    memcpy(concatenated + a_length, b_chars, b_length);
    concatenated[new_length] = '\0';

    if(IS_NUMBER(a)){
        free(a_chars);
    }
    if(IS_NUMBER(b)){
        free(b_chars);
    }

    struct string_pool_add_result add_result = add_to_global_string_pool(concatenated, new_length);

    if(add_result.created_new) {
        add_object_to_heap(self_thread->gc_info, &add_result.string_object->object);
    } else {
        free(concatenated);
    }

    return TO_LOX_VALUE_OBJECT(add_result.string_object);
}
#include "shared/types/types.h"
#include "shared/types/function_object.h"
#include "shared/types/string_object.h"
#include "runtime/vm.h"

extern struct vm current_vm;

void print_lox_value(lox_value_t value) {
#ifdef VM_TEST
    current_vm.log[current_vm.log_entries_in_use++] = to_string(value);
#else
    #ifdef NAN_BOXING
        if(IS_NIL(value_node)) {
            printf("nil");
        } else if(IS_OBJECT(value_node)) {
            struct object * object = AS_OBJECT(value_node);

            switch (AS_OBJECT(value_node)->type) {
                case OBJ_STRING: printf("%s", ((struct string_object *) AS_OBJECT(value_node))->chars); break;
                case OBJ_FUNCTION: printf("fun %s",  ((struct function_object *) AS_OBJECT(value_node))->name->chars); break;
            }
        } else if(IS_BOOL(value_node)) {
            printf("%s", AS_OBJECT(value_node) ? "true" : "false");
        } else if(IS_NUMBER(value_node)) {
            printf("%g", AS_NUMBER(value_node));
        }
    #else
        switch (value_node.type) {
            case VAL_NIL: printf("nil"); break;
            case VAL_NUMBER: printf("%g", value_node.as.immediate); break;
            case VAL_BOOL: printf(value_node.as.boolean ? "true" : "false"); break;
            case VAL_OBJ:
                switch (value_node.as.object->type) {
                    case OBJ_STRING: printf("%s", AS_STRING_CHARS_OBJECT(value_node));
                    case OBJ_FUNCTION: printf("fun %s", ((struct function_object *) value_node.as.object)->name->chars);
                }
        }
    #endif
#endif
}
#include "types.h"
#include "string_object.h"
#include "array_object.h"

extern void * alloc_gc_vm_info_alg();
extern void runtime_panic(char * format, ...);

bool cast_to_boolean(lox_value_t value) {
#ifdef NAN_BOXING
    return AS_BOOL(value);
#else
    return value_node.value_as.immediate != 0;
#endif
}

void init_object(struct object * object, object_type_t type) {
    object->type = type;
    object->gc_info = alloc_gc_vm_info_alg();
}

struct object * allocate_object(size_t size, object_type_t type) {
    struct object * object = (struct object *) NATIVE_LOX_MALLOC(size);
    init_object(object, type);

    return object;
}

//This current_function should be only used for testing since it may leak memory when value_node is a immediate
char * lox_value_to_string(lox_value_t value) {
#ifdef NAN_BOXING
    if(IS_NIL(value)) {
        return "nil";
    } else if(IS_OBJECT(value)) {
        struct object * object = AS_OBJECT(value);

        switch (AS_OBJECT(value)->type) {
            case OBJ_STRING: {
                return ((struct string_object *) AS_OBJECT(value))->chars;
            }
        }
    } else if(IS_BOOL(value)) {
        return AS_OBJECT(value) ? "true" : "false";
    } else if(IS_NUMBER(value)) {
        char * string_as_double = NATIVE_LOX_MALLOC(sizeof(char) * 20);
        sprintf(string_as_double, "%f", AS_NUMBER(value));
        return string_as_double;
    }
#else
    switch (value_node.type) {
        case VAL_NIL: return "nil"; break;
        case VAL_NUMBER: {
            char * string_as_double = NATIVE_LOX_MALLOC(sizeof(char) * 20);
            sprintf(string_as_double, "%f", value_node.value_as.immediate);
            return string_as_double;
        }
        case VAL_BOOL: return value_node.value_as.boolean ? "true" : "false"; break;
        case VAL_OBJ:
            switch (value_node.value_as.object->type) {
                case OBJ_STRING: return AS_STRING_CHARS_OBJECT(value_node);
            }
    };
#endif
    runtime_panic("Cannot parse to chars");
    return NULL;
}

char * lox_object_type_to_string(object_type_t object_type) {
    switch (object_type) {
        case OBJ_STRUCT_DEFINITION: return "StructDefinition";
        case OBJ_NATIVE_FUNCTION: return "NativeFunction";
        case OBJ_STRUCT_INSTANCE: return "StructInstance";
        case OBJ_FUNCTION: return "Function";
        case OBJ_PACKAGE: return "Package";
        case OBJ_STRING: return "String";
        case OBJ_ARRAY: return "Array";
    }
}

lox_value_type get_lox_type(lox_value_t lox_value) {
    if(IS_BOOL(lox_value)){
        return VAL_BOOL;
    } else if(IS_NIL(lox_value)){
        return VAL_NIL;
    } else if(IS_NUMBER(lox_value)){
        return VAL_NUMBER;
    } else if(IS_OBJECT(lox_value)) {
        return VAL_OBJ;
    } else {
        return VAL_NIL;
    }
}

static uint64_t lox_object_to_native_type(struct object *);

uint64_t lox_to_native_type(lox_value_t lox_value) {
    if(IS_BOOL(lox_value)){
        return (uint64_t) AS_BOOL(lox_value);
    } else if (IS_NIL(lox_value)) {
        return (uint64_t) NULL;
    } else if (IS_OBJECT(lox_value)) {
        return lox_object_to_native_type(AS_OBJECT(lox_value));
    } else if(IS_NUMBER(lox_value)) {
        return (uint64_t) AS_NUMBER(lox_value);
    } else {
        runtime_panic("");
    }
}

static uint64_t lox_object_to_native_type(struct object * object) {
    switch (object->type) {
        case OBJ_STRING: return (uint64_t) ((struct string_object *) object)->chars;
        case OBJ_ARRAY: return (uint64_t) ((struct array_object *) object)->values.values;
        case OBJ_FUNCTION:
        case OBJ_NATIVE_FUNCTION:
        case OBJ_STRUCT_INSTANCE:
        case OBJ_STRUCT_DEFINITION:
        case OBJ_PACKAGE: {
            return (uint64_t) object;
        }
    }
}
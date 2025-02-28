#include "lox_ir_type.h"

struct lox_ir_type * create_lox_ir_type(lox_ir_type_t type, struct lox_allocator * allocator) {
    struct lox_ir_type * lox_ir_type = LOX_MALLOC(allocator, sizeof(struct lox_ir_type));
    memset(lox_ir_type, 0, sizeof(struct lox_ir_type));
    lox_ir_type->type = type;
    return lox_ir_type;
}

struct lox_ir_type * create_initialize_struct_lox_ir_type(
        struct struct_definition_object * definition,
        struct lox_allocator * allocator
) {
    struct struct_instance_lox_ir_type * struct_instance = LOX_MALLOC(allocator, sizeof(struct struct_instance_lox_ir_type));
    struct_instance->definition = definition;
    init_u64_hash_table(&struct_instance->type_by_field_name, allocator);

    struct lox_ir_type * type = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_STRUCT_INSTANCE, allocator);
    type->value.struct_instance = struct_instance;

    return type;
}

struct lox_ir_type * clone_lox_ir_type(struct lox_ir_type * clone_src, struct lox_allocator * allocator) {
    struct lox_ir_type * clone_dst = LOX_MALLOC(allocator, sizeof(struct lox_ir_type));
    clone_dst->type = clone_src->type;

    struct array_lox_ir_type * array_clone_src = clone_src->value.array;
    if (array_clone_src != NULL) {
        clone_dst->value.array = LOX_MALLOC(allocator, sizeof(struct array_lox_ir_type));
        clone_dst->value.array->type = clone_lox_ir_type(array_clone_src->type, allocator);
    }

    struct struct_instance_lox_ir_type * struct_clone_src = clone_src->value.struct_instance;
    if (struct_clone_src != NULL) {
        struct u64_hash_table * struct_fields_clone_dst = clone_u64_hash_table(&struct_clone_src->type_by_field_name, allocator);
        struct_clone_src->type_by_field_name.capacity = struct_fields_clone_dst->capacity;
        struct_clone_src->type_by_field_name.entries = struct_fields_clone_dst->entries;
        struct_clone_src->type_by_field_name.size = struct_fields_clone_dst->size;
        struct_clone_src->type_by_field_name.allocator = allocator;

        struct_clone_src->definition = struct_clone_src->definition;
    }

    clone_dst->value.struct_instance = struct_clone_src;
    clone_dst->value.array = array_clone_src;

    return clone_dst;
}

struct lox_ir_type * create_array_lox_ir_type(
        struct lox_ir_type * array_type,
        struct lox_allocator * allocator
) {
    struct array_lox_ir_type * array_lox_ir_type = LOX_MALLOC(allocator, sizeof(struct array_lox_ir_type));
    array_lox_ir_type->type = array_type;

    struct lox_ir_type * type = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ARRAY, allocator);
    type->value.array = array_lox_ir_type;

    return type;
}

lox_ir_type_t profiled_type_to_lox_ir_type(profile_data_type_t profiled_type) {
    switch (profiled_type) {
        case PROFILE_DATA_TYPE_I64: return LOX_IR_TYPE_LOX_I64;
        case PROFILE_DATA_TYPE_F64: return LOX_IR_TYPE_F64;
        case PROFILE_DATA_TYPE_BOOLEAN: return LOX_IR_TYPE_LOX_BOOLEAN;
        case PROFILE_DATA_TYPE_NIL: return LOX_IR_TYPE_LOX_NIL;
        case PROFILE_DATA_TYPE_ARRAY: return LOX_IR_TYPE_LOX_ARRAY;
        case PROFILE_DATA_TYPE_STRUCT_INSTANCE: return LOX_IR_TYPE_LOX_STRUCT_INSTANCE;
        case PROFILE_DATA_TYPE_ANY: return LOX_IR_TYPE_LOX_ANY;
        case PROFILE_DATA_TYPE_STRING: return LOX_IR_TYPE_LOX_STRING;
        default: return 0;
    }
}

lox_ir_type_t native_type_to_lox_ir_type(lox_ir_type_t native_type) {
    switch (native_type) {
        case LOX_IR_TYPE_NATIVE_I64: return LOX_IR_TYPE_LOX_I64;
        case LOX_IR_TYPE_NATIVE_STRING: return LOX_IR_TYPE_LOX_STRING;
        case LOX_IR_TYPE_NATIVE_BOOLEAN: return LOX_IR_TYPE_LOX_BOOLEAN;
        case LOX_IR_TYPE_NATIVE_NIL: return LOX_IR_TYPE_LOX_NIL;
        case LOX_IR_TYPE_NATIVE_ARRAY: return LOX_IR_TYPE_LOX_ARRAY;
        case LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE: return LOX_IR_TYPE_LOX_STRUCT_INSTANCE;
        default: return native_type;
    }
}

lox_ir_type_t lox_type_to_native_lox_ir_type(lox_ir_type_t lox_type) {
    switch (lox_type) {
        case LOX_IR_TYPE_LOX_I64: return LOX_IR_TYPE_NATIVE_I64;
        case LOX_IR_TYPE_LOX_STRING: return LOX_IR_TYPE_NATIVE_STRING;
        case LOX_IR_TYPE_LOX_BOOLEAN: return LOX_IR_TYPE_NATIVE_BOOLEAN;
        case LOX_IR_TYPE_LOX_NIL: return LOX_IR_TYPE_NATIVE_NIL;
        case LOX_IR_TYPE_LOX_ARRAY: return LOX_IR_TYPE_NATIVE_ARRAY;
        case LOX_IR_TYPE_LOX_STRUCT_INSTANCE: return LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE;
        default: return lox_type;
    }
}

bool is_eq_lox_ir_type(struct lox_ir_type * a, struct lox_ir_type * b) {
    if(a->type != b->type){
        return false;
    }

    switch (a->type) {
        case LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE:
        case LOX_IR_TYPE_LOX_STRUCT_INSTANCE: {
            if(a->value.struct_instance->definition != b->value.struct_instance->definition){
                return false;
            }

            struct struct_definition_object * definition = a->value.struct_instance->definition;

            for(int i = 0; definition != NULL && i < definition->n_fields; i++){
                char * field_name = definition->field_names[i]->chars;

                struct lox_ir_type * a_field = get_u64_hash_table(&a->value.struct_instance->type_by_field_name, (uint64_t) field_name);
                struct lox_ir_type * b_field = get_u64_hash_table(&b->value.struct_instance->type_by_field_name, (uint64_t) field_name);

                if(a_field != NULL && b_field != NULL && !is_eq_lox_ir_type(a_field, b_field)){
                    return false;
                }
                if((a_field != NULL && b_field == NULL) || (a_field == NULL && b_field != NULL)){
                    return false;
                }
            }

            return true;
        }
        case LOX_IR_TYPE_NATIVE_ARRAY:
        case LOX_IR_TYPE_LOX_ARRAY: {
            return is_eq_lox_ir_type(a->value.array->type, b->value.array->type);
        }
        default: {
            return true;
        }
    }
}

struct lox_ir_type * get_struct_field_lox_ir_type(
        struct lox_ir_type * struct_type,
        char * field_name,
        struct lox_allocator * allocator
) {
    struct lox_ir_type * field_type = get_u64_hash_table(
            &struct_type->value.struct_instance->type_by_field_name, (uint64_t) field_name
    );
    if(field_type == NULL){
        struct lox_ir_type * any_type = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ANY, allocator);
        put_u64_hash_table(&struct_type->value.struct_instance->type_by_field_name, (uint64_t) field_name, any_type);
        return any_type;
    } else {
        return field_type;
    }
}

char * to_string_lox_ir_type(lox_ir_type_t type) {
    switch (type) {
        case LOX_IR_TYPE_F64:
            return "F64";
        case LOX_IR_TYPE_UNKNOWN:
            return "Uknown";
        case LOX_IR_TYPE_NATIVE_I64:
            return "Native I64";
        case LOX_IR_TYPE_NATIVE_STRING:
            return "Native String";
        case LOX_IR_TYPE_NATIVE_BOOLEAN:
            return "Native Boolean";
        case LOX_IR_TYPE_NATIVE_NIL:
            return "Native Nil";
        case LOX_IR_TYPE_NATIVE_ARRAY:
            return "Native Array";
        case LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE:
            return "Native StructInstance";
        case LOX_IR_TYPE_LOX_ANY:
            return "Lox Any";
        case LOX_IR_TYPE_LOX_I64:
            return "Lox I64";
        case LOX_IR_TYPE_LOX_STRING:
            return "Lox String";
        case LOX_IR_TYPE_LOX_BOOLEAN:
            return "Lox Boolean";
        case LOX_IR_TYPE_LOX_NIL:
            return "Lox Nil";
        case LOX_IR_TYPE_LOX_ARRAY:
            return "Lox Array";
        case LOX_IR_TYPE_LOX_STRUCT_INSTANCE:
            return "Lox StructInstance";
    }
}

struct lox_ir_type * native_to_lox_lox_ir_type(struct lox_ir_type * native_type, struct lox_allocator * allocator) {
    if(is_lox_lox_ir_type(native_type->type)){
        return native_type;
    }

    switch (native_type->type) {
        case LOX_IR_TYPE_NATIVE_I64: return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_I64, allocator);
        case LOX_IR_TYPE_NATIVE_STRING: return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_STRING, allocator);
        case LOX_IR_TYPE_NATIVE_BOOLEAN: return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_BOOLEAN, allocator);
        case LOX_IR_TYPE_NATIVE_NIL: return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_NIL, allocator);
        case LOX_IR_TYPE_NATIVE_ARRAY: {
            struct lox_ir_type * array = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_ARRAY, allocator);
            array->value.array = native_type->value.array;
            return array;
        }
        case LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE: {
            struct lox_ir_type * struct_instance = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_LOX_STRUCT_INSTANCE, allocator);
            struct_instance->value.struct_instance = native_type->value.struct_instance;
            return struct_instance;
        }
    }
    //TODO Runtime panic
}

struct lox_ir_type * lox_to_native_lox_ir_type(struct lox_ir_type * lox_type, struct lox_allocator * allocator) {
    if (is_native_lox_ir_type(lox_type->type)) {
        return lox_type;
    }

    switch (lox_type->type) {
        case LOX_IR_TYPE_LOX_I64: return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_NATIVE_I64, allocator);
        case LOX_IR_TYPE_LOX_STRING: return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_NATIVE_STRING, allocator);
        case LOX_IR_TYPE_LOX_BOOLEAN: return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_NATIVE_BOOLEAN, allocator);
        case LOX_IR_TYPE_LOX_NIL: return CREATE_LOX_IR_TYPE(LOX_IR_TYPE_NATIVE_NIL, allocator);
        case LOX_IR_TYPE_LOX_ARRAY: {
            struct lox_ir_type * array = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_NATIVE_ARRAY, allocator);
            array->value.array = lox_type->value.array;
            return array;
        }
        case LOX_IR_TYPE_LOX_STRUCT_INSTANCE: {
            struct lox_ir_type * struct_instance = CREATE_LOX_IR_TYPE(LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE, allocator);
            struct_instance->value.struct_instance = lox_type->value.struct_instance;
            return struct_instance;
        }
    }
    //TODO Runtime panic
}

bool is_lox_lox_ir_type(lox_ir_type_t type) {
    return type >= LOX_IR_TYPE_LOX_ANY;
}

bool is_native_lox_ir_type(lox_ir_type_t type) {
    return type >= LOX_IR_TYPE_NATIVE_I64 && type <= LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE;
}

lox_ir_type_t binary_to_lox_ir_type(bytecode_t operator, lox_ir_type_t left, lox_ir_type_t right) {
    switch (operator) {
        case OP_GREATER:
        case OP_EQUAL:
        case OP_LESS: {
            return LOX_IR_TYPE_LOX_BOOLEAN;
        }
        case OP_BINARY_OP_AND:
        case OP_BINARY_OP_OR:
        case OP_LEFT_SHIFT:
        case OP_RIGHT_SHIFT:
        case OP_MODULO: {
            return LOX_IR_TYPE_LOX_I64;
        }
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_ADD: {
            if(left == LOX_IR_TYPE_UNKNOWN || right == LOX_IR_TYPE_UNKNOWN){
                return LOX_IR_TYPE_UNKNOWN;
            }
            bool some_string = left == LOX_IR_TYPE_LOX_STRING || right == LOX_IR_TYPE_LOX_STRING;
            bool some_f64 = left == LOX_IR_TYPE_F64 || right == LOX_IR_TYPE_F64;
            bool some_any = left == LOX_IR_TYPE_LOX_ANY || right == LOX_IR_TYPE_LOX_ANY;
            bool some_lox = is_lox_lox_ir_type(left) || is_lox_lox_ir_type(right);

            if (some_string) {
                return LOX_IR_TYPE_LOX_STRING;
            } else if (some_any) {
                return LOX_IR_TYPE_LOX_ANY;
            } else if(some_f64) {
                return LOX_IR_TYPE_F64;
            } else {
                return LOX_IR_TYPE_LOX_I64;
            }
        }
    }
}

bool is_same_format_lox_ir_type(lox_ir_type_t left, lox_ir_type_t right) {
    if(left == LOX_IR_TYPE_F64 || right == LOX_IR_TYPE_F64){
        return true;
    }

    return (is_lox_lox_ir_type(left) && is_lox_lox_ir_type(right)) ||
           (is_native_lox_ir_type(left) && is_native_lox_ir_type(right));
}

bool is_format_equivalent_lox_ir_type(lox_ir_type_t left, lox_ir_type_t right) {
    if (left == right) {
        return true;
    }

    switch (left) {
        case LOX_IR_TYPE_F64: return right == LOX_IR_TYPE_F64;
        case LOX_IR_TYPE_NATIVE_I64: return right == LOX_IR_TYPE_LOX_I64;
        case LOX_IR_TYPE_UNKNOWN: return true;
        case LOX_IR_TYPE_LOX_ANY: return true;
        case LOX_IR_TYPE_NATIVE_STRING: return right == LOX_IR_TYPE_LOX_STRING;
        case LOX_IR_TYPE_NATIVE_BOOLEAN: return right == LOX_IR_TYPE_LOX_BOOLEAN;
        case LOX_IR_TYPE_NATIVE_NIL: return right == LOX_IR_TYPE_LOX_NIL;
        case LOX_IR_TYPE_NATIVE_ARRAY: return right == LOX_IR_TYPE_LOX_ARRAY;
        case LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE: return right == LOX_IR_TYPE_LOX_STRUCT_INSTANCE;
        case LOX_IR_TYPE_LOX_I64: return right == LOX_IR_TYPE_NATIVE_I64;
        case LOX_IR_TYPE_LOX_STRING: return right == LOX_IR_TYPE_NATIVE_STRING;
        case LOX_IR_TYPE_LOX_BOOLEAN: return right == LOX_IR_TYPE_NATIVE_BOOLEAN;
        case LOX_IR_TYPE_LOX_NIL: return right == LOX_IR_TYPE_NATIVE_NIL;
        case LOX_IR_TYPE_LOX_ARRAY: return right == LOX_IR_TYPE_NATIVE_ARRAY;
        case LOX_IR_TYPE_LOX_STRUCT_INSTANCE: return right == LOX_IR_TYPE_NATIVE_STRUCT_INSTANCE;
    }
}

bool is_same_number_binay_format_lox_ir_type(lox_ir_type_t left, lox_ir_type_t right) {
    struct u8_set values;
    add_u8_set(&values, right + 1);
    add_u8_set(&values, left + 1);

    bool some_other_type_found = false;

    for (int i = 0 ; i < (LOX_IR_TYPE_LOX_LAST_TYPE + 1); i++) {
        if (i != LOX_IR_TYPE_LOX_ANY
            && i != LOX_IR_TYPE_LOX_I64
            && i != LOX_IR_TYPE_F64
        ) {
            some_other_type_found = true;
            break;
        }
    }

    return !some_other_type_found;
}
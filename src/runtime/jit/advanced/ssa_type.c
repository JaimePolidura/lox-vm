#include "ssa_type.h"

struct ssa_type * create_ssa_type(ssa_type_t type, struct lox_allocator * allocator) {
    struct ssa_type * ssa_type = LOX_MALLOC(allocator, sizeof(struct ssa_type));
    memset(ssa_type, 0, sizeof(struct ssa_type));
    ssa_type->type = type;
    return ssa_type;
}

struct ssa_type * create_initialize_struct_ssa_type(
        struct struct_definition_object * definition,
        struct lox_allocator * allocator
) {
    struct struct_instance_ssa_type * struct_instance = LOX_MALLOC(allocator, sizeof(struct struct_instance_ssa_type));
    struct_instance->definition = definition;
    init_u64_hash_table(&struct_instance->type_by_field_name, allocator);

    struct ssa_type * ssa_type = CREATE_SSA_TYPE(SSA_TYPE_LOX_STRUCT_INSTANCE, allocator);
    ssa_type->value.struct_instance = struct_instance;

    return ssa_type;
}

struct ssa_type * create_array_ssa_type(
        struct ssa_type * array_type,
        struct lox_allocator * allocator
) {
    struct array_ssa_type * array_ssa_type = LOX_MALLOC(allocator, sizeof(struct array_ssa_type));
    array_ssa_type->type = array_type;

    struct ssa_type * ssa_type = CREATE_SSA_TYPE(SSA_TYPE_LOX_ARRAY, allocator);
    ssa_type->value.array = array_ssa_type;

    return ssa_type;
}

ssa_type_t profiled_type_to_ssa_type(profile_data_type_t profiled_type) {
    switch (profiled_type) {
        case PROFILE_DATA_TYPE_I64: return SSA_TYPE_LOX_I64;
        case PROFILE_DATA_TYPE_F64: return SSA_TYPE_F64;
        case PROFILE_DATA_TYPE_BOOLEAN: return SSA_TYPE_LOX_BOOLEAN;
        case PROFILE_DATA_TYPE_NIL: return SSA_TYPE_LOX_NIL;
        case PROFILE_DATA_TYPE_ARRAY: return SSA_TYPE_LOX_ARRAY;
        case PROFILE_DATA_TYPE_STRUCT_INSTANCE: return SSA_TYPE_LOX_STRUCT_INSTANCE;
        case PROFILE_DATA_TYPE_ANY: return SSA_TYPE_LOX_ANY;
        case PROFILE_DATA_TYPE_STRING: return SSA_TYPE_LOX_STRING;
        default: return 0;
    }
}

ssa_type_t native_type_to_lox_ssa_type(ssa_type_t native_type) {
    switch (native_type) {
        case SSA_TYPE_NATIVE_I64: return SSA_TYPE_LOX_I64;
        case SSA_TYPE_NATIVE_STRING: return SSA_TYPE_LOX_STRING;
        case SSA_TYPE_NATIVE_BOOLEAN: return SSA_TYPE_LOX_BOOLEAN;
        case SSA_TYPE_NATIVE_NIL: return SSA_TYPE_LOX_NIL;
        case SSA_TYPE_NATIVE_ARRAY: return SSA_TYPE_LOX_ARRAY;
        case SSA_TYPE_NATIVE_STRUCT_INSTANCE: return SSA_TYPE_LOX_STRUCT_INSTANCE;
        default: return native_type;
    }
}

ssa_type_t lox_type_to_native_ssa_type(ssa_type_t lox_type) {
    switch (lox_type) {
        case SSA_TYPE_LOX_I64: return SSA_TYPE_NATIVE_I64;
        case SSA_TYPE_LOX_STRING: return SSA_TYPE_NATIVE_STRING;
        case SSA_TYPE_LOX_BOOLEAN: return SSA_TYPE_NATIVE_BOOLEAN;
        case SSA_TYPE_LOX_NIL: return SSA_TYPE_NATIVE_NIL;
        case SSA_TYPE_LOX_ARRAY: return SSA_TYPE_NATIVE_ARRAY;
        case SSA_TYPE_LOX_STRUCT_INSTANCE: return SSA_TYPE_NATIVE_STRUCT_INSTANCE;
        default: return lox_type;
    }
}

bool is_eq_ssa_type(struct ssa_type * a, struct ssa_type * b) {
    if(a->type != b->type){
        return false;
    }

    switch (a->type) {
        case SSA_TYPE_NATIVE_STRUCT_INSTANCE:
        case SSA_TYPE_LOX_STRUCT_INSTANCE: {
            if(a->value.struct_instance->definition != b->value.struct_instance->definition){
                return false;
            }

            struct struct_definition_object * definition = a->value.struct_instance->definition;

            for(int i = 0; definition != NULL && i < definition->n_fields; i++){
                char * field_name = definition->field_names[i]->chars;

                struct ssa_type * a_field = get_u64_hash_table(&a->value.struct_instance->type_by_field_name, (uint64_t) field_name);
                struct ssa_type * b_field = get_u64_hash_table(&b->value.struct_instance->type_by_field_name, (uint64_t) field_name);

                if(a_field != NULL && b_field != NULL && !is_eq_ssa_type(a_field, b_field)){
                    return false;
                }
                if((a_field != NULL && b_field == NULL) || (a_field == NULL && b_field != NULL)){
                    return false;
                }
            }

            return true;
        }
        case SSA_TYPE_NATIVE_ARRAY:
        case SSA_TYPE_LOX_ARRAY: {
            return is_eq_ssa_type(a->value.array->type, b->value.array->type);
        }
        default: {
            return true;
        }
    }
}

struct ssa_type * get_struct_field_ssa_type(
        struct ssa_type * struct_type,
        char * field_name,
        struct lox_allocator * allocator
) {
    struct ssa_type * field_type = get_u64_hash_table(
            &struct_type->value.struct_instance->type_by_field_name, (uint64_t) field_name
    );
    if(field_type == NULL){
        struct ssa_type * any_type = CREATE_SSA_TYPE(SSA_TYPE_LOX_ANY, allocator);
        put_u64_hash_table(&struct_type->value.struct_instance->type_by_field_name, (uint64_t) field_name, any_type);
        return any_type;
    } else {
        return field_type;
    }
}

char * to_string_ssa_type(ssa_type_t type) {
    switch (type) {
        case SSA_TYPE_F64:
            return "F64";
        case SSA_TYPE_UNKNOWN:
            return "Uknown";
        case SSA_TYPE_NATIVE_I64:
            return "Native I64";
        case SSA_TYPE_NATIVE_STRING:
            return "Native String";
        case SSA_TYPE_NATIVE_BOOLEAN:
            return "Native Boolean";
        case SSA_TYPE_NATIVE_NIL:
            return "Native Nil";
        case SSA_TYPE_NATIVE_ARRAY:
            return "Native Array";
        case SSA_TYPE_NATIVE_STRUCT_INSTANCE:
            return "Native StructInstance";
        case SSA_TYPE_LOX_ANY:
            return "Lox Any";
        case SSA_TYPE_LOX_I64:
            return "Lox I64";
        case SSA_TYPE_LOX_STRING:
            return "Lox String";
        case SSA_TYPE_LOX_BOOLEAN:
            return "Lox Boolean";
        case SSA_TYPE_LOX_NIL:
            return "Lox Nil";
        case SSA_TYPE_LOX_ARRAY:
            return "Lox Array";
        case SSA_TYPE_LOX_STRUCT_INSTANCE:
            return "Lox StructInstance";
    }
}

struct ssa_type * native_to_lox_ssa_type(struct ssa_type * native_type, struct lox_allocator * allocator) {
    if(is_lox_ssa_type(native_type->type)){
        return native_type;
    }

    switch (native_type->type) {
        case SSA_TYPE_NATIVE_I64: return CREATE_SSA_TYPE(SSA_TYPE_LOX_I64, allocator);
        case SSA_TYPE_NATIVE_STRING: return CREATE_SSA_TYPE(SSA_TYPE_LOX_STRING, allocator);
        case SSA_TYPE_NATIVE_BOOLEAN: return CREATE_SSA_TYPE(SSA_TYPE_LOX_BOOLEAN, allocator);
        case SSA_TYPE_NATIVE_NIL: return CREATE_SSA_TYPE(SSA_TYPE_LOX_NIL, allocator);
        case SSA_TYPE_NATIVE_ARRAY: {
            struct ssa_type * array = CREATE_SSA_TYPE(SSA_TYPE_LOX_ARRAY, allocator);
            array->value.array = native_type->value.array;
            return array;
        }
        case SSA_TYPE_NATIVE_STRUCT_INSTANCE: {
            struct ssa_type * struct_instance = CREATE_SSA_TYPE(SSA_TYPE_LOX_STRUCT_INSTANCE, allocator);
            struct_instance->value.struct_instance = native_type->value.struct_instance;
            return struct_instance;
        }
    }
    //TODO Runtime panic
}

struct ssa_type * lox_to_native_ssa_type(struct ssa_type * lox_type, struct lox_allocator * allocator) {
    if (is_native_ssa_type(lox_type->type)) {
        return lox_type;
    }

    switch (lox_type->type) {
        case SSA_TYPE_LOX_I64: return CREATE_SSA_TYPE(SSA_TYPE_NATIVE_I64, allocator);
        case SSA_TYPE_LOX_STRING: return CREATE_SSA_TYPE(SSA_TYPE_NATIVE_STRING, allocator);
        case SSA_TYPE_LOX_BOOLEAN: return CREATE_SSA_TYPE(SSA_TYPE_NATIVE_BOOLEAN, allocator);
        case SSA_TYPE_LOX_NIL: return CREATE_SSA_TYPE(SSA_TYPE_NATIVE_NIL, allocator);
        case SSA_TYPE_LOX_ARRAY: {
            struct ssa_type * array = CREATE_SSA_TYPE(SSA_TYPE_NATIVE_ARRAY, allocator);
            array->value.array = lox_type->value.array;
            return array;
        }
        case SSA_TYPE_LOX_STRUCT_INSTANCE: {
            struct ssa_type * struct_instance = CREATE_SSA_TYPE(SSA_TYPE_NATIVE_STRUCT_INSTANCE, allocator);
            struct_instance->value.struct_instance = lox_type->value.struct_instance;
            return struct_instance;
        }
    }
    //TODO Runtime panic
}

bool is_lox_ssa_type(ssa_type_t type) {
    return type >= SSA_TYPE_LOX_ANY;
}


bool is_native_ssa_type(ssa_type_t type) {
    return type >= SSA_TYPE_NATIVE_I64 && type <= SSA_TYPE_NATIVE_STRUCT_INSTANCE;
}

ssa_type_t binary_to_ssa_type(bytecode_t operator, ssa_type_t left, ssa_type_t right, bool return_lox_as_default) {
    switch (operator) {
        case OP_GREATER:
        case OP_EQUAL:
        case OP_LESS: {
            return return_lox_as_default ? SSA_TYPE_LOX_BOOLEAN : SSA_TYPE_NATIVE_BOOLEAN;
        }
        case OP_BINARY_OP_AND:
        case OP_BINARY_OP_OR:
        case OP_LEFT_SHIFT:
        case OP_RIGHT_SHIFT:
        case OP_MODULO: {
            return return_lox_as_default ? SSA_TYPE_LOX_I64 : SSA_TYPE_NATIVE_I64;
        }
        case OP_ADD: {
            if(left == SSA_TYPE_UNKNOWN || right == SSA_TYPE_UNKNOWN){
                return SSA_TYPE_UNKNOWN;
            }
            if (left == SSA_TYPE_LOX_ANY || right == SSA_TYPE_LOX_ANY) {
                return SSA_TYPE_LOX_ANY;
            }

            bool some_string = left == SSA_TYPE_LOX_STRING || right == SSA_TYPE_LOX_STRING ||
                    left == SSA_TYPE_NATIVE_STRING || right == SSA_TYPE_NATIVE_STRING;
            bool some_f64 = left == SSA_TYPE_F64 || right == SSA_TYPE_F64;

            if(some_string){
                return return_lox_as_default ? SSA_TYPE_LOX_STRING : SSA_TYPE_NATIVE_STRING;
            } else if(some_f64){
                return SSA_TYPE_F64;
            } else {
                return return_lox_as_default ? SSA_TYPE_LOX_I64 : SSA_TYPE_NATIVE_I64;
            }
        }
        case OP_SUB:
        case OP_MUL: {
            if(left == SSA_TYPE_UNKNOWN || right == SSA_TYPE_UNKNOWN){
                return SSA_TYPE_UNKNOWN;
            }

            if(left == SSA_TYPE_F64 || right == SSA_TYPE_F64) {
                return SSA_TYPE_F64;
            } else {
                return return_lox_as_default ? SSA_TYPE_LOX_I64 : SSA_TYPE_NATIVE_I64;
            }
        }
        case OP_DIV: {
            return SSA_TYPE_F64;
        }
    }
}
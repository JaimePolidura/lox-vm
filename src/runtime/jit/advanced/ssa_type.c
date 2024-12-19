#include "ssa_type.h"

struct ssa_type * create_ssa_type(ssa_type_t type, struct lox_allocator * allocator) {
    struct ssa_type * ssa_type = LOX_MALLOC(allocator, sizeof(struct ssa_type));
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

    struct ssa_type * ssa_type = CREATE_SSA_TYPE(SSA_TYPE_NATIVE_STRUCT_INSTANCE, allocator);
    ssa_type->value.struct_instance = struct_instance;

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
        default: return native_type; //TODO Panick
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
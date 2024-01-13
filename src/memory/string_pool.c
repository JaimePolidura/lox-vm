#include "string_pool.h"

struct string_pool global_string_pool;

struct string_pool_add_result add_to_global_string_pool(char * string_ptr, int length) {
    uint32_t string_hash = hash_string(string_ptr, length);
    struct string_object * pooled_string = get_key_by_hash(&global_string_pool.strings, string_hash);
    bool string_already_in_pool = pooled_string != NULL;

    if(!string_already_in_pool){
        struct string_object * string = ALLOCATE_OBJ(struct string_object, OBJ_STRING);
        string->hash = string_hash;
        string->chars = string_ptr;
        string->length = length;

        put_hash_table(&global_string_pool.strings, string, NIL_VALUE());

        return (struct string_pool_add_result){
                .string_object = string,
                .created_new = true
        };
    } else {
        return (struct string_pool_add_result){
                .string_object = pooled_string,
                .created_new = false
        };
    }
}

void init_global_string_pool() {
    init_hash_table(&global_string_pool.strings);
}

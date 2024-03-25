#include "string_pool.h"

struct string_pool global_string_pool;

struct string_pool_add_result add_substring_to_global_string_pool(struct substring substring) {
    return add_to_global_string_pool(start_substring(substring), length_substring(substring));
}

struct string_pool_add_result add_to_global_string_pool(char * string_ptr, int length) {
    lock_mutex(&global_string_pool.lock);

    uint32_t string_hash = hash_string(string_ptr, length);
    struct string_object * pooled_string = get_key_by_hash(&global_string_pool.strings, string_hash);
    bool string_already_in_pool = pooled_string != NULL;

    if(!string_already_in_pool) {
        struct string_object * string = copy_chars_to_string_object(string_ptr, length);
        string->hash = string_hash;

        put_hash_table(&global_string_pool.strings, string, NIL_VALUE);

        unlock_mutex(&global_string_pool.lock);

        return (struct string_pool_add_result){
                .string_object = string,
                .created_new = true
        };
    } else {
        unlock_mutex(&global_string_pool.lock);

        return (struct string_pool_add_result){
                .string_object = pooled_string,
                .created_new = false
        };
    }
}

void remove_entry_string_pool(struct string_object * key) {
    lock_mutex(&global_string_pool.lock);

    remove_hash_table(&global_string_pool.strings, key);

    unlock_mutex(&global_string_pool.lock);
}

void init_global_string_pool() {
    init_hash_table(&global_string_pool.strings);
    init_mutex(&global_string_pool.lock);
}

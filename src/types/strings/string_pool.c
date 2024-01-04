#include "string_pool.h"

static struct string_pool_add_result add_string_pool_private_with_options(struct string_pool * pool, char * string_ptr, int length, bool create_new_copy);

struct string_pool_add_result add_copy_of_string_pool(struct string_pool * pool, char * string_ptr, int length) {
    return add_string_pool_private_with_options(pool, string_ptr, length, true);
}

struct string_pool_add_result add_string_pool(struct string_pool * pool, char * string_ptr, int length) {
    return add_string_pool_private_with_options(pool, string_ptr, length, false);
}

static struct string_pool_add_result add_string_pool_private_with_options(struct string_pool * pool, char * string_ptr, int length, bool create_new_copy) {
    uint32_t string_hash = hash_string(string_ptr, length);
    struct string_object * pooled_string = get_key_by_hash(&pool->strings, string_hash);
    bool string_already_in_pool = pooled_string != NULL;

    if(!string_already_in_pool){
        if(create_new_copy){
            char * new_string_ptr = malloc(sizeof(char) * length + 1);
            memcpy(new_string_ptr, string_ptr, length);
            new_string_ptr[length] = '\0';
            string_ptr = new_string_ptr;
        }

        struct string_object * string = malloc(sizeof(struct string_object));
        string->object.type = OBJ_STRING;
        string->hash = string_hash;
        string->chars = string_ptr;
        string->length = length;

        put_hash_table(&pool->strings, string, FROM_NIL());

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

void init_string_pool(struct string_pool * pool) {
    init_hash_table(&pool->strings);
}

void add_all_pool(struct string_pool * to, struct string_pool * from) {
    add_all_hash_table(&to->strings, &from->strings);
}
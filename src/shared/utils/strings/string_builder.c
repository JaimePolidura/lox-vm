#include "string_builder.h"

static int calculate_final_string_size(struct string_builder *string_builder);

void init_string_builder(struct string_builder * string_builder, struct lox_allocator * allocator) {
    string_builder->allocator = allocator;
    string_builder->first = NULL;
    string_builder->last = NULL;
}

void free_string_builder(struct string_builder * string_builder) {
    struct string_builder_node * current_node = string_builder->first;
    while(current_node->next != NULL){
        struct string_builder_node * node_to_free = current_node;
        current_node = current_node->next;
        LOX_FREE(string_builder->allocator, node_to_free);
    }
    LOX_FREE(string_builder->allocator, current_node);
}

void append_string_builder(struct string_builder * string_builder, char * string) {
    append_with_length_string_builder(string_builder, string, strlen(string));
}

void append_with_length_string_builder(struct string_builder * string_builder, char * string, int string_length) {
    struct string_builder_node * new_node = LOX_MALLOC(string_builder->allocator, sizeof(struct string_builder_node));
    new_node->length = string_length;
    new_node->chars = string;
    new_node->next = NULL;

    if(string_builder->first == NULL){
        string_builder->first = new_node;
    }
    if(string_builder->last != NULL){
        struct string_builder_node * prev_node = string_builder->last;
        prev_node->next = new_node;
    }
    string_builder->last = new_node;
}

void remove_last_string_builder(struct string_builder * string_builder) {
    //Emtpy string builder
    if (string_builder->first == NULL && string_builder->last == NULL) {
        return;
    }
    //Only 1 element
    if (string_builder->first == string_builder->last) {
        LOX_FREE(string_builder->allocator, string_builder->first);
        string_builder->first = NULL;
        string_builder->last = NULL;
        return;
    }

    //Get prev to last control_node_to_lower
    struct string_builder_node * prev_to_last = string_builder->first;
    while(prev_to_last != NULL && prev_to_last->next != NULL && prev_to_last->next != string_builder->last){
        prev_to_last = prev_to_last->next;
    }
    //Remove last control_node_to_lower
    struct string_builder_node * last_node = string_builder->last;
    LOX_FREE(string_builder->allocator, last_node);
    //Set last control_node_to_lower to prev last control_node_to_lower
    string_builder->last = prev_to_last;
}

char * to_string_string_builder(struct string_builder * string_builder, struct lox_allocator * allocator) {
    int final_string_size = calculate_final_string_size(string_builder);
    char * final_string = LOX_MALLOC(allocator, sizeof(char) * final_string_size);

    char * current_final_string_ptr = final_string;
    struct string_builder_node * current_node = string_builder->first;
    while(current_node != NULL){
        memcpy(current_final_string_ptr, current_node->chars, current_node->length);
        current_final_string_ptr += current_node->length;
        current_node = current_node->next;
    }
    *current_final_string_ptr = '\0';

    return final_string;
}

static int calculate_final_string_size(struct string_builder * string_builder) {
    int current_size = 0;
    struct string_builder_node * current_node = string_builder->first;
    while(current_node != NULL) {
        current_size += current_node->length;
        current_node = current_node->next;
    }

    //+1 To incorporate Null ascii terminator char: \0
    return current_size + 1;
}
#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared.h"

struct string_builder_node {
    char * chars;
    int length;
    struct string_builder_node * next;
};

struct string_builder {
    struct string_builder_node * first;
    struct string_builder_node * last;
    struct lox_allocator * allocator;
};

void init_string_builder(struct string_builder *, struct lox_allocator *);
void free_string_builder(struct string_builder *);

void append_string_builder(struct string_builder *, char *);
void append_with_length_string_builder(struct string_builder *, char *, int);
void remove_last_string_builder(struct string_builder *);

char * to_string_string_builder(struct string_builder *, struct lox_allocator *);

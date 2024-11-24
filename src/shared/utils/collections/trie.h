#pragma once

#include "shared/utils/memory/lox_allocator.h"
#include "shared/utils/utils.h"
#include "shared.h"

#define TRIE_CHARS ('_' - 'A' + 1)

#define TO_TRIE_INDEX_KEY(key) (TO_UPPER_CASE(key) - 65)

#define NON_TRIE_VALUE ((void *) 0xFFFFFFFFFFFFFFFF)

typedef bool (*consumer_t)(void *, void *);

// All keys are set to lowercase. The keys also include _
struct trie_node {
    struct trie_node * nodes[TRIE_CHARS]; //Including the underscore

    char * key;
    int key_length;
    int depth;
    void * data;
};

struct trie_list {
    struct lox_allocator * allocator;
    struct trie_node * head;
    int size;
};

struct trie_list * alloc_trie_list(struct lox_allocator *);
void init_trie_list(struct trie_list * trie_list, struct lox_allocator *);
void free_trie_list(struct trie_list * trie);

void for_each_node(struct trie_list * trie, void * extra, consumer_t consumer_callback);
void * find_trie(struct trie_list * trie, char * key, int key_length);
bool put_trie(struct trie_list * trie, char * new_key, int new_key_length, void * new_data);
bool contains_trie(struct trie_list * trie, char * key, int key_length);

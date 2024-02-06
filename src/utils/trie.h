#pragma once

#include "shared.h"

#define TRIE_CHARS ('_' - 'A' + 1)

#define TO_UPPER_CASE(character) ( ~((character >= 'a' & character <= 'z') << 5) & character )

#define TO_TRIE_INDEX_KEY(key) (TO_UPPER_CASE(key) - 65)

// All keys are set to lowercase. The keys also include _
struct trie_node {
    struct trie_node * nodes[TRIE_CHARS]; //Including the underscore

    char * key;
    int key_length;
    int depth;
    void * data;
};

struct trie_list {
    struct trie_node * head;
};

struct trie_list * alloc_trie_list();
void init_trie_list(struct trie_list * trie_list);
void free_trie_list(struct trie_list * trie);

void * find_trie(struct trie_list * trie, char * key, int key_length);
bool put_trie(struct trie_list * trie, char * new_key, int new_key_length, void * new_data);
bool contains_trie(struct trie_list * trie, char * key, int key_length);
void clear_trie(struct trie_list * trie);
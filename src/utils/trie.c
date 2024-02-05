#include "trie.h"

static struct trie_node * alloc_trie_node();
static bool compare_key_strings(char * a, char * b, int length_a);
static char * to_lower_case(char * src, int length_src);

static struct trie_node * find_node(struct trie_list * trie, char * key, int key_length);

void * find_trie(struct trie_list * trie, char * key, int key_length) {
    struct trie_node * node = find_node(trie, key, key_length);

    if(node->key_length == key_length &&
        compare_key_strings(node->key, key, key_length)) {

        return node->data;
    } else {
        return NULL;
    }
}

bool contains_trie(struct trie_list * trie, char * key, int key_length) {
    return find_trie(trie, key, key_length) != NULL;
}

bool put_trie(struct trie_list * trie, char * new_key, int new_key_length, void * new_data) {
    struct trie_node * prev_node = trie->head;
    bool some_new_node_created = false;

    for(int i = 0; i < new_key_length; i++) {
        char actual_key = TO_TRIE_INDEX_KEY(new_key[i]);
        struct trie_node * current_node = prev_node->nodes[actual_key];

        if(current_node == NULL){
            current_node = alloc_trie_node();
            current_node->depth = i;
            prev_node->nodes[actual_key] = current_node;
            some_new_node_created = true;
        }

        prev_node = current_node;
    }

    if(!some_new_node_created && prev_node->key != NULL) {
        return false; // Node already exists
    }

    prev_node->key = to_lower_case(new_key, new_key_length);
    prev_node->key_length = new_key_length;
    prev_node->data = new_data;

    return true;
}

struct trie_list * alloc_trie_list() {
    struct trie_list * trie_list = malloc(sizeof(struct trie_list));
    init_trie_list(trie_list);
    return trie_list;
}

void init_trie_list(struct trie_list * trie_list) {
    trie_list->head = alloc_trie_node();
}

void free_trie_list(struct trie_list * trie) {
    //TODO
}

static struct trie_node * alloc_trie_node() {
    struct trie_node * trie_node = malloc(sizeof(struct trie_node));
    trie_node->key = NULL;
    trie_node->data = NULL;
    trie_node->key_length = 0;
    trie_node->depth = 0;
    for(int i = 0; i < TRIE_CHARS; i++){
        trie_node->nodes[i] = NULL;
    }

    return trie_node;
}

static struct trie_node * find_node(struct trie_list * trie, char * key, int key_length) {
    struct trie_node * current_node = trie->head;

    for(int i = 0; i < key_length; i++){
        struct trie_node * next_node = current_node->nodes[TO_TRIE_INDEX_KEY(key[i])];

        if(next_node == NULL) {
            return current_node;
        } else {
            current_node = next_node;
        }
    }

    return current_node;
}

// TODO Optimize
static bool compare_key_strings(char * a, char * b, int length_a) {
    for(int i = 0; i < length_a; i++){
        if(TO_UPPER_CASE(a[i]) != TO_UPPER_CASE(b[i])){
            return false;
        }
    }

    return true;
}

// TODO Optimize
static char * to_lower_case(char * src, int length_src) {
    char * new = malloc(sizeof(char) * length_src);
    for(int i = 0; i < length_src; i++){
        new[i] = TO_UPPER_CASE(src[i]);
    }

    return new;
}
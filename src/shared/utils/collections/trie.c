#include "trie.h"

static struct trie_node * alloc_trie_node();

static struct trie_node * find_node(struct trie_list * trie, char * key, int key_length);

void * find_trie(struct trie_list * trie, char * key, int key_length) {
    struct trie_node * node = find_node(trie, key, key_length);

    if(node->key_length == key_length &&
        string_equals_ignore_case(node->key, key, key_length)) {

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

    prev_node->key = to_upper_case(new_key, new_key_length);
    prev_node->key_length = new_key_length;
    prev_node->data = new_data;
    trie->size++;

    return true;
}

void clear_trie_recursive(struct trie_node * node) {
    for (int i = 0; i < TRIE_CHARS; ++i) {
        if(node->nodes[i] != NULL){
            clear_trie_recursive(node->nodes[i]);
            free(node->nodes[i]);
            node->nodes[i] = NULL;
        }
    }
 }

static void for_each_node_recursive(struct trie_node * node, consumer_t consumer_callback) {
    for(int i = 0; i < TRIE_CHARS; i++){
        if(node->nodes[i] != NULL){
            for_each_node_recursive(node->nodes[i], consumer_callback);
        }
    }

    if(node->data != NULL){
        consumer_callback(node);
    }
}

void for_each_node(struct trie_list * trie, consumer_t consumer_callback) {
    for(int i = 0; i < TRIE_CHARS; i++){
        if(trie->head->nodes[i] != NULL){
            for_each_node_recursive(trie->head->nodes[i], consumer_callback);
        }
    }
}

void free_trie_list(struct trie_list * trie) {
    clear_trie_recursive(trie->head);
    trie->size = 0;
}

struct trie_list * alloc_trie_list() {
    struct trie_list * trie_list = malloc(sizeof(struct trie_list));
    init_trie_list(trie_list);
    return trie_list;
}

void init_trie_list(struct trie_list * trie_list) {
    trie_list->head = alloc_trie_node();
    trie_list->size = 0;
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

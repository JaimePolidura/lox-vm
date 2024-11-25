#include "card_table.h"

static struct mark_bitmap * find_card(struct card_table *, uint64_t * address);
static struct mark_bitmap * add_card(struct card_table *, int index);

struct card_table * alloc_card_table(struct config config, uint64_t * memory_space_start_address, uint64_t * memory_space_end_address) {
    struct card_table * card_table = NATIVE_LOX_MALLOC(sizeof(struct card_table));
    init_card_table(card_table, config, memory_space_start_address, memory_space_end_address);
    return card_table;
}

void init_card_table(struct card_table * card_table, struct config config, uint64_t * memory_space_start_address, uint64_t * memory_space_end_address) {
    int n_cards = ceil((memory_space_end_address - memory_space_start_address) / (double) config.generational_gc_config.n_addresses_per_card_table);
    card_table->cards = NATIVE_LOX_MALLOC(sizeof(struct mark_bitmap *) * n_cards);
    memset(card_table->cards, 0, sizeof(struct mark_bitmap *) * n_cards);

    card_table->n_addresses_per_card_table = config.generational_gc_config.n_addresses_per_card_table;
    card_table->start_address_memory_space = memory_space_start_address;
    card_table->n_cards = n_cards;
}

void mark_dirty_card_table(struct card_table * card_table, uint64_t * address) {
    struct mark_bitmap * card = find_card(card_table, address);
    set_marked_bitmap(card, address);
}

bool is_dirty_card_table(struct card_table * card_table, uint64_t * address) {
    struct mark_bitmap * card = find_card(card_table, address);
    return is_marked_bitmap(card, address);
}

void clear_card_table(struct card_table * table) {
    for (int i = 0; i < table->n_cards; ++i) {
        if (table->cards[i] != NULL) {
            free_mark_bitmap(table->cards[i]);
            NATIVE_LOX_FREE(table->cards[i]);
            table->cards[i] = NULL;
        }
    }
}

static struct mark_bitmap * find_card(struct card_table * card_table, uint64_t * address) {
    int index = (card_table->start_address_memory_space - address) / card_table->n_addresses_per_card_table;
    struct mark_bitmap * card = card_table->cards[index];

    if (card == NULL) {
        card = add_card(card_table, index);
    }

    return card;
}

static struct mark_bitmap * add_card(struct card_table * card_table, int index) {
    int n_addresses = card_table->n_addresses_per_card_table;
    uint64_t * card_start_address = card_table->start_address_memory_space + (index * n_addresses * sizeof(void *));
    struct mark_bitmap * card = alloc_mark_bitmap(n_addresses, card_start_address);
    card_table->cards[index] = card;
    return card;
}

void for_each_card_table(struct card_table * table, void * extra, mark_bitmap_consumer_t consumer) {
    bool continue_iterating = true;

    for (int i = 0; i < table->n_cards && continue_iterating; i++) {
        struct mark_bitmap * card = table->cards[i];
        if (card != NULL) {
            continue_iterating = for_each_marked_bitmap(card, extra, consumer);
        }
    }
}
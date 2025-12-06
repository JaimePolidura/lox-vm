#include "config.h"

struct config config = (struct config) {
    .compile_inline_methods = true,

    .generational_gc_config = {
            .eden_size_mb = 1,
            .eden_block_size_kb = 1,
            .survivor_size_mb = 2,
            .old_size_mb = 4,
            .n_generations_to_old = 2,
            .n_addresses_per_card_table = 64,
    }
};
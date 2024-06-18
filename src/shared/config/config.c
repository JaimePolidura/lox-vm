#include "config.h"

struct config config = (struct config) {
    .compile_inline_methods = true,

    .generational_gc_config = {
            .eden_size_mb = 5
    }
};
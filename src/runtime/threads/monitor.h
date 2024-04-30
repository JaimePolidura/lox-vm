#pragma once

#include "shared.h"
#include "shared/utils/concurrency/mutex.h"

typedef uint8_t monitor_number_t;

struct monitor {
    pthread_cond_t condition;
    struct mutex mutex;

    volatile bool free;
};

void enter_monitor(struct monitor * monitor);

void exit_monitor(struct monitor * monitor);

void init_monitor(struct monitor * monitor);
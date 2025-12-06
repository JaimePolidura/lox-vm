#pragma once

#include "shared.h"

// A wrapper on pthread or any other implementation
// Mutex is reentrant
struct mutex {
    pthread_mutex_t native_mutex;
};

void init_mutex(struct mutex * lock);
void free_mutex(struct mutex * lock);

void lock_mutex(struct mutex * lock);
void unlock_mutex(struct mutex * lock);
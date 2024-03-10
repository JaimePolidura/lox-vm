#pragma once

#include "shared.h"
#include "mutex.h"

struct rw_mutex {
    pthread_rwlock_t native_rw_lock;

    struct mutex mutex;
    struct mutex readers_count_lock;
    int readers_count;
};

void init_rw_mutex(struct rw_mutex * mutex);
void free_rw_mutex(struct rw_mutex * mutex);

void lock_reader_rw_mutex(struct rw_mutex * rw_mutex);
void unlock_reader_rw_mutex(struct rw_mutex * rw_mutex);
void lock_writer_rw_mutex(struct rw_mutex * rw_mutex);
void unlock_writer_rw_mutex(struct rw_mutex * rw_mutex);

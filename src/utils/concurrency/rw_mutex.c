#include "rw_mutex.h"

void init_rw_mutex(struct rw_mutex * mutex) {
    init_mutex(&mutex->mutex);
    init_mutex(&mutex->readers_count_lock);
    mutex->readers_count = 0;
}

void free_rw_mutex(struct rw_mutex * mutex) {
    mutex->readers_count = 0;
    free_mutex(&mutex->mutex);
}

void lock_reader_rw_mutex(struct rw_mutex * rw_mutex) {
    lock_mutex(&rw_mutex->readers_count_lock);

    if(++rw_mutex->readers_count == 1) {
        lock_mutex(&rw_mutex->mutex);
    }

    unlock_mutex(&rw_mutex->readers_count_lock);
}

void unlock_reader_rw_mutex(struct rw_mutex * rw_mutex) {
    lock_mutex(&rw_mutex->readers_count_lock);

    if(--rw_mutex->readers_count == 0) {
        unlock_mutex(&rw_mutex->mutex);
    }

    unlock_mutex(&rw_mutex->readers_count_lock);
}

void lock_writer_rw_mutex(struct rw_mutex * rw_mutex) {
    lock_mutex(&rw_mutex->mutex);
}

void unlock_writer_rw_mutex(struct rw_mutex * rw_mutex) {
    unlock_mutex(&rw_mutex->mutex);
}
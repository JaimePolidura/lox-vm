#include "rw_mutex.h"

void init_rw_mutex(struct rw_mutex * mutex) {
    init_mutex(&mutex->mutex);
    mutex->readers_count = 0;
}

void free_rw_mutex(struct rw_mutex * mutex) {
    mutex->readers_count = 0;
    free_mutex(&mutex->mutex);
}

void lock_reader_rw_mutex(struct rw_mutex * rw_mutex) {
    if(atomic_fetch_add(&rw_mutex->readers_count, 1) == 0) {
        lock_mutex(&rw_mutex->mutex);
    }
}

void unlock_reader_rw_mutex(struct rw_mutex * rw_mutex) {
    if(atomic_fetch_sub(&rw_mutex->readers_count, 1) == 1) {
        rw_mutex->readers_count = 0;
        unlock_mutex(&rw_mutex->mutex);
    }
}

void lock_writer_rw_mutex(struct rw_mutex * rw_mutex) {
    lock_mutex(&rw_mutex->mutex);
}

void unlock_writer_rw_mutex(struct rw_mutex * rw_mutex) {
    unlock_mutex(&rw_mutex->mutex);
}
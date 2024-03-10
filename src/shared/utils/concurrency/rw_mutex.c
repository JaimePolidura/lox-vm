#include "rw_mutex.h"

void init_rw_mutex(struct rw_mutex * mutex) {
    pthread_rwlock_init(&mutex->native_rw_lock, NULL);
}

void free_rw_mutex(struct rw_mutex * mutex) {
    pthread_rwlock_destroy(&mutex->native_rw_lock);
}

void lock_reader_rw_mutex(struct rw_mutex * rw_mutex) {
    pthread_rwlock_rdlock(&rw_mutex->native_rw_lock);
}

void unlock_reader_rw_mutex(struct rw_mutex * rw_mutex) {
    pthread_rwlock_unlock(&rw_mutex->native_rw_lock);
}

void lock_writer_rw_mutex(struct rw_mutex * rw_mutex) {
    pthread_rwlock_wrlock(&rw_mutex->native_rw_lock);
}

void unlock_writer_rw_mutex(struct rw_mutex * rw_mutex) {
    pthread_rwlock_unlock(&rw_mutex->native_rw_lock);
}
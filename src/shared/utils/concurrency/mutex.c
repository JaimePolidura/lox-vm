#include "mutex.h"

void init_mutex(struct mutex * lock) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&lock->native_mutex, &attr);
}

void free_mutex(struct mutex * lock) {
    pthread_mutex_destroy(&lock->native_mutex);
}

void lock_mutex(struct mutex * lock) {
    pthread_mutex_lock(&lock->native_mutex);
}

void unlock_mutex(struct mutex * lock) {
    pthread_mutex_unlock(&lock->native_mutex);
}
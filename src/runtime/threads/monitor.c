#include "monitor.h"

void init_monitor(struct monitor * monitor) {
    init_mutex(&monitor->mutex);
    pthread_cond_init(&monitor->condition, NULL);
    monitor->free = true;
}

void enter_monitor(struct monitor * monitor) {
    lock_mutex(&monitor->mutex);

    while(!monitor->free){
        pthread_cond_wait(&monitor->condition, &monitor->mutex.native_mutex);
    }

    monitor->free = false;

    unlock_mutex(&monitor->mutex);
}

void exit_monitor(struct monitor * monitor) {
    lock_mutex(&monitor->mutex);

    monitor->free = true;
    pthread_cond_signal(&monitor->condition);

    unlock_mutex(&monitor->mutex);
}
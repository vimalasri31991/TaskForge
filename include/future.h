#ifndef FUTURE_H
#define FUTURE_H

#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;

    int completed;
    int result;
} Future;

int future_init(Future *future);

void future_set(Future *future, int result);

int future_get(Future *future);

void future_destroy(Future *future);

#endif

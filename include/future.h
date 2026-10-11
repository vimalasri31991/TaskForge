
#ifndef FUTURE_H
#define FUTURE_H

#include <pthread.h>

typedef enum {
    FUTURE_PENDING,
    FUTURE_RUNNING,
    FUTURE_COMPLETED,
    FUTURE_CANCELLED
} FutureStatus;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    FutureStatus status;
    int result;
} Future;

int future_init(Future *future);
void future_set_running(Future *future);
void future_set(Future *future, int result);
int future_cancel(Future *future);
int future_get(Future *future);
FutureStatus future_get_status(Future *future);
void future_destroy(Future *future);

#endif

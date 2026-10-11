
#ifndef TASKFORGE_H
#define TASKFORGE_H

#include <pthread.h>
#include <stddef.h>
#include <stdatomic.h>

#include "queue.h"
#include "future.h"

typedef int (*TaskFunction)(void *);

typedef struct
{
    TaskFunction function;
    void *argument;
    Future *future;
    TaskPriority priority;
} Task;

typedef struct
{
    pthread_t *workers;
    size_t worker_count;
    TaskQueue queue;

    atomic_int shutting_down;

    pthread_mutex_t lifecycle_mutex;
    pthread_cond_t lifecycle_condition;
    size_t active_submitters;
} TaskForge;

int taskforge_init(
    TaskForge *pool,
    size_t worker_count,
    size_t queue_capacity
);

Future *taskforge_submit(
    TaskForge *pool,
    TaskFunction function,
    void *argument,
    TaskPriority priority
);

/* Returns NULL immediately if the queue is full or shutting down. */
Future *taskforge_try_submit(
    TaskForge *pool,
    TaskFunction function,
    void *argument,
    TaskPriority priority
);

int taskforge_cancel(TaskForge *pool, Future *future);
void taskforge_shutdown(TaskForge *pool);

#endif

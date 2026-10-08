#ifndef TASKFORGE_H
#define TASKFORGE_H

#include <pthread.h>
#include <stddef.h>
#include <stdatomic.h>

#include "queue.h"
#include "future.h"


typedef int (*TaskFunction)(void *);


/*
 * Task now contains a priority.
 */
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

} TaskForge;


/*
 * Initialize TaskForge.
 */
int taskforge_init(
    TaskForge *pool,
    size_t worker_count,
    size_t queue_capacity
);


/*
 * Module 2:
 * Submit task with priority.
 */
Future *taskforge_submit(
    TaskForge *pool,
    TaskFunction function,
    void *argument,
    TaskPriority priority
);


/*
 * Graceful shutdown.
 */
void taskforge_shutdown(
    TaskForge *pool
);


#endif

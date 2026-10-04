#include "taskforge.h"

#include <stdlib.h>
#include <stdio.h>

static void *worker_function(void *argument)
{
    TaskForge *pool = (TaskForge *)argument;

    while (1)
    {
        Task *task = (Task *)queue_pop(&pool->queue);

        if (task == NULL)
        {
            break;
        }

        int result = task->function(task->argument);

        future_set(task->future, result);

        free(task->argument);
        free(task);
    }

    return NULL;
}

int taskforge_init(
    TaskForge *pool,
    size_t worker_count,
    size_t queue_capacity)
{
    if (pool == NULL ||
        worker_count == 0 ||
        queue_capacity == 0)
    {
        return -1;
    }

    pool->worker_count = worker_count;

    /*
     * Initialize shutdown state atomically.
     * 0 = running
     * 1 = shutting down
     */
    atomic_init(&pool->shutting_down, 0);

    if (queue_init(&pool->queue, queue_capacity) != 0)
    {
        return -1;
    }

    pool->workers =
        malloc(sizeof(pthread_t) * worker_count);

    if (pool->workers == NULL)
    {
        queue_destroy(&pool->queue);
        return -1;
    }

    for (size_t i = 0; i < worker_count; i++)
    {
        if (pthread_create(
                &pool->workers[i],
                NULL,
                worker_function,
                pool) != 0)
        {
            /*
             * If worker creation fails,
             * shut down the queue and
             * join already-created workers.
             */
            queue_shutdown(&pool->queue);

            for (size_t j = 0; j < i; j++)
            {
                pthread_join(
                    pool->workers[j],
                    NULL
                );
            }

            free(pool->workers);
            queue_destroy(&pool->queue);

            return -1;
        }
    }

    return 0;
}

Future *taskforge_submit(
    TaskForge *pool,
    TaskFunction function,
    void *argument)
{
    if (pool == NULL || function == NULL)
    {
        return NULL;
    }

    /*
     * Atomically check whether shutdown
     * has already started.
     */
    if (atomic_load(&pool->shutting_down))
    {
        return NULL;
    }

    Task *task = malloc(sizeof(Task));

    if (task == NULL)
    {
        return NULL;
    }

    Future *future = malloc(sizeof(Future));

    if (future == NULL)
    {
        free(task);
        return NULL;
    }

    if (future_init(future) != 0)
    {
        free(task);
        free(future);
        return NULL;
    }

    task->function = function;
    task->argument = argument;
    task->future = future;

    if (queue_push(&pool->queue, task) != 0)
    {
        future_destroy(future);

        free(future);
        free(task);

        return NULL;
    }

    return future;
}

void taskforge_shutdown(TaskForge *pool)
{
    if (pool == NULL)
    {
        return;
    }

    /*
     * Atomically change the state from
     * running to shutting down.
     */
    int already_shutting_down =
        atomic_exchange(
            &pool->shutting_down,
            1
        );

    /*
     * If another thread already initiated
     * shutdown, do not perform it again.
     */
    if (already_shutting_down)
    {
        return;
    }

    /*
     * Stop accepting new queue operations
     * and wake waiting producers/workers.
     */
    queue_shutdown(&pool->queue);

    /*
     * Wait for every worker to finish.
     */
    for (size_t i = 0;
         i < pool->worker_count;
         i++)
    {
        pthread_join(
            pool->workers[i],
            NULL
        );
    }

    /*
     * Release worker-thread storage.
     */
    free(pool->workers);

    /*
     * Destroy queue resources.
     */
    queue_destroy(&pool->queue);
}

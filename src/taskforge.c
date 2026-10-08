#include "taskforge.h"

#include <stdlib.h>


static void *worker_function(void *argument)
{
    TaskForge *pool =
        (TaskForge *)argument;


    while (1)
    {
        Task *task =
            (Task *)queue_pop(
                &pool->queue
            );


        /*
         * NULL means queue is empty
         * and shutdown has started.
         */
        if (task == NULL)
        {
            break;
        }


        /*
         * Execute the task.
         */
        int result =
            task->function(
                task->argument
            );


        /*
         * Store result in Future.
         */
        future_set(
            task->future,
            result
        );


        /*
         * Worker owns task argument
         * after successful submission.
         */
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
    if (
        pool == NULL ||
        worker_count == 0 ||
        queue_capacity == 0
    )
    {
        return -1;
    }


    pool->worker_count =
        worker_count;


    /*
     * 0 = running
     * 1 = shutting down
     */
    atomic_init(
        &pool->shutting_down,
        0
    );


    if (
        queue_init(
            &pool->queue,
            queue_capacity
        ) != 0
    )
    {
        return -1;
    }


    pool->workers =
        malloc(
            sizeof(pthread_t) *
            worker_count
        );


    if (pool->workers == NULL)
    {
        queue_destroy(
            &pool->queue
        );

        return -1;
    }


    for (
        size_t i = 0;
        i < worker_count;
        i++
    )
    {
        if (
            pthread_create(
                &pool->workers[i],
                NULL,
                worker_function,
                pool
            ) != 0
        )
        {
            queue_shutdown(
                &pool->queue
            );


            for (
                size_t j = 0;
                j < i;
                j++
            )
            {
                pthread_join(
                    pool->workers[j],
                    NULL
                );
            }


            free(pool->workers);

            queue_destroy(
                &pool->queue
            );

            return -1;
        }
    }


    return 0;
}


Future *taskforge_submit(
    TaskForge *pool,
    TaskFunction function,
    void *argument,
    TaskPriority priority)
{
    if (
        pool == NULL ||
        function == NULL
    )
    {
        return NULL;
    }


    /*
     * Reject submissions after shutdown.
     */
    if (
        atomic_load(
            &pool->shutting_down
        )
    )
    {
        return NULL;
    }


    /*
     * Validate priority.
     */
    if (
        priority != PRIORITY_HIGH &&
        priority != PRIORITY_MEDIUM &&
        priority != PRIORITY_LOW
    )
    {
        return NULL;
    }


    Task *task =
        malloc(sizeof(Task));


    if (task == NULL)
    {
        return NULL;
    }


    Future *future =
        malloc(sizeof(Future));


    if (future == NULL)
    {
        free(task);
        return NULL;
    }


    if (
        future_init(future) != 0
    )
    {
        free(task);
        free(future);

        return NULL;
    }


    task->function = function;

    task->argument = argument;

    task->future = future;

    task->priority = priority;


    /*
     * Push task into the appropriate
     * priority queue.
     */
    if (
        queue_push(
            &pool->queue,
            task,
            priority
        ) != 0
    )
    {
        future_destroy(future);

        free(future);

        free(task);

        return NULL;
    }


    return future;
}


void taskforge_shutdown(
    TaskForge *pool)
{
    if (pool == NULL)
    {
        return;
    }


    /*
     * Atomically start shutdown.
     */
    int already_shutting_down =
        atomic_exchange(
            &pool->shutting_down,
            1
        );


    if (already_shutting_down)
    {
        return;
    }


    /*
     * Stop accepting new work and wake
     * blocked producers/workers.
     */
    queue_shutdown(
        &pool->queue
    );


    /*
     * Wait for every worker.
     */
    for (
        size_t i = 0;
        i < pool->worker_count;
        i++
    )
    {
        pthread_join(
            pool->workers[i],
            NULL
        );
    }


    free(pool->workers);


    queue_destroy(
        &pool->queue
    );
}


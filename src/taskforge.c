
#include "taskforge.h"

#include <stdlib.h>

static void submitter_finished(TaskForge *pool)
{
    pthread_mutex_lock(&pool->lifecycle_mutex);

    pool->active_submitters--;

    if (pool->active_submitters == 0)
        pthread_cond_broadcast(&pool->lifecycle_condition);

    pthread_mutex_unlock(&pool->lifecycle_mutex);
}

static int submitter_started(TaskForge *pool)
{
    pthread_mutex_lock(&pool->lifecycle_mutex);

    if (atomic_load(&pool->shutting_down))
    {
        pthread_mutex_unlock(&pool->lifecycle_mutex);
        return -1;
    }

    pool->active_submitters++;

    pthread_mutex_unlock(&pool->lifecycle_mutex);
    return 0;
}

static void *worker_function(void *argument)
{
    TaskForge *pool = argument;

    while (1)
    {
        Task *task = queue_pop(&pool->queue);

        if (task == NULL)
            break;

        future_set_running(task->future);

        if (future_get_status(task->future) == FUTURE_RUNNING)
        {
            int result = task->function(task->argument);
            future_set(task->future, result);
        }

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
    if (pool == NULL || worker_count == 0 || queue_capacity == 0)
        return -1;

    pool->workers = NULL;
    pool->worker_count = worker_count;
    pool->active_submitters = 0;
    atomic_init(&pool->shutting_down, 0);

    if (pthread_mutex_init(&pool->lifecycle_mutex, NULL) != 0)
        return -1;

    if (pthread_cond_init(&pool->lifecycle_condition, NULL) != 0)
    {
        pthread_mutex_destroy(&pool->lifecycle_mutex);
        return -1;
    }

    if (queue_init(&pool->queue, queue_capacity) != 0)
    {
        pthread_cond_destroy(&pool->lifecycle_condition);
        pthread_mutex_destroy(&pool->lifecycle_mutex);
        return -1;
    }

    pool->workers = malloc(sizeof(pthread_t) * worker_count);

    if (pool->workers == NULL)
    {
        queue_destroy(&pool->queue);
        pthread_cond_destroy(&pool->lifecycle_condition);
        pthread_mutex_destroy(&pool->lifecycle_mutex);
        return -1;
    }

    size_t created = 0;

    for (; created < worker_count; created++)
    {
        if (pthread_create(
                &pool->workers[created],
                NULL,
                worker_function,
                pool) != 0)
        {
            queue_shutdown(&pool->queue);

            for (size_t j = 0; j < created; j++)
                pthread_join(pool->workers[j], NULL);

            free(pool->workers);
            pool->workers = NULL;

            queue_destroy(&pool->queue);
            pthread_cond_destroy(&pool->lifecycle_condition);
            pthread_mutex_destroy(&pool->lifecycle_mutex);

            return -1;
        }
    }

    return 0;
}

static Future *submit_internal(
    TaskForge *pool,
    TaskFunction function,
    void *argument,
    TaskPriority priority,
    int nonblocking)
{
    if (pool == NULL || function == NULL)
        return NULL;

    if (priority != PRIORITY_HIGH &&
        priority != PRIORITY_MEDIUM &&
        priority != PRIORITY_LOW)
        return NULL;

    if (submitter_started(pool) != 0)
        return NULL;

    Task *task = malloc(sizeof(Task));

    if (task == NULL)
    {
        submitter_finished(pool);
        return NULL;
    }

    Future *future = malloc(sizeof(Future));

    if (future == NULL)
    {
        free(task);
        submitter_finished(pool);
        return NULL;
    }

    if (future_init(future) != 0)
    {
        free(task);
        free(future);
        submitter_finished(pool);
        return NULL;
    }

    task->function = function;
    task->argument = argument;
    task->future = future;
    task->priority = priority;

    int push_result;

    if (nonblocking)
        push_result = queue_try_push(&pool->queue, task, priority);
    else
        push_result = queue_push(&pool->queue, task, priority);

    if (push_result != 0)
    {
        future_destroy(future);
        free(future);
        free(task);
        submitter_finished(pool);
        return NULL;
    }

    submitter_finished(pool);
    return future;
}

Future *taskforge_submit(
    TaskForge *pool,
    TaskFunction function,
    void *argument,
    TaskPriority priority)
{
    return submit_internal(pool, function, argument, priority, 0);
}

Future *taskforge_try_submit(
    TaskForge *pool,
    TaskFunction function,
    void *argument,
    TaskPriority priority)
{
    return submit_internal(pool, function, argument, priority, 1);
}

int taskforge_cancel(TaskForge *pool, Future *future)
{
    if (pool == NULL || future == NULL)
        return -1;

    /*
     * Keep the same lock order as the existing cancellation design:
     * queue mutex first, then Future mutex.
     */
    pthread_mutex_lock(&pool->queue.mutex);
    pthread_mutex_lock(&future->mutex);

    if (future->status != FUTURE_PENDING)
    {
        pthread_mutex_unlock(&future->mutex);
        pthread_mutex_unlock(&pool->queue.mutex);
        return -1;
    }

    Task *found = NULL;

    void **buffers[3] = {
        pool->queue.buffer_high,
        pool->queue.buffer_medium,
        pool->queue.buffer_low
    };

    size_t fronts[3] = {
        pool->queue.front_high,
        pool->queue.front_medium,
        pool->queue.front_low
    };

    size_t counts[3] = {
        pool->queue.count_high,
        pool->queue.count_medium,
        pool->queue.count_low
    };

    for (int p = 0; p < 3 && found == NULL; p++)
    {
        for (size_t i = 0; i < counts[p]; i++)
        {
            size_t index = (fronts[p] + i) % pool->queue.capacity;
            Task *candidate = buffers[p][index];

            if (candidate != NULL && candidate->future == future)
            {
                found = candidate;
                break;
            }
        }
    }

    if (found == NULL)
    {
        pthread_mutex_unlock(&future->mutex);
        pthread_mutex_unlock(&pool->queue.mutex);
        return -1;
    }

    future->status = FUTURE_CANCELLED;
    pthread_cond_broadcast(&future->condition);

    pthread_mutex_unlock(&future->mutex);
    pthread_mutex_unlock(&pool->queue.mutex);

    /*
     * queue_remove locks the queue itself, so call it after releasing
     * the queue mutex. The pending Future was marked cancelled above.
     */
    if (queue_remove(&pool->queue, found) != 0)
        return -1;

    free(found->argument);
    free(found);

    return 0;
}

void taskforge_shutdown(TaskForge *pool)
{
    if (pool == NULL)
        return;

    pthread_mutex_lock(&pool->lifecycle_mutex);

    if (atomic_load(&pool->shutting_down))
    {
        pthread_mutex_unlock(&pool->lifecycle_mutex);
        return;
    }

    atomic_store(&pool->shutting_down, 1);

    pthread_mutex_unlock(&pool->lifecycle_mutex);

    queue_shutdown(&pool->queue);

    pthread_mutex_lock(&pool->lifecycle_mutex);

    while (pool->active_submitters != 0)
        pthread_cond_wait(
            &pool->lifecycle_condition,
            &pool->lifecycle_mutex);

    pthread_mutex_unlock(&pool->lifecycle_mutex);

    for (size_t i = 0; i < pool->worker_count; i++)
        pthread_join(pool->workers[i], NULL);

    free(pool->workers);
    pool->workers = NULL;

    queue_destroy(&pool->queue);
    pthread_cond_destroy(&pool->lifecycle_condition);
    pthread_mutex_destroy(&pool->lifecycle_mutex);
}

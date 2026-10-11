
#include "queue.h"

#include <stdlib.h>

#define HIGH_LIMIT 3
#define MEDIUM_LIMIT 3

static int valid_priority(TaskPriority priority)
{
    return priority == PRIORITY_HIGH ||
           priority == PRIORITY_MEDIUM ||
           priority == PRIORITY_LOW;
}

int queue_init(TaskQueue *queue, size_t capacity)
{
    if (queue == NULL || capacity == 0)
    {
        return -1;
    }

    queue->buffer_high = NULL;
    queue->buffer_medium = NULL;
    queue->buffer_low = NULL;

    queue->buffer_high = calloc(capacity, sizeof(void *));
    queue->buffer_medium = calloc(capacity, sizeof(void *));
    queue->buffer_low = calloc(capacity, sizeof(void *));

    if (queue->buffer_high == NULL ||
        queue->buffer_medium == NULL ||
        queue->buffer_low == NULL)
    {
        free(queue->buffer_high);
        free(queue->buffer_medium);
        free(queue->buffer_low);

        queue->buffer_high = NULL;
        queue->buffer_medium = NULL;
        queue->buffer_low = NULL;

        return -1;
    }

    queue->capacity = capacity;

    queue->front_high = 0;
    queue->rear_high = 0;
    queue->count_high = 0;

    queue->front_medium = 0;
    queue->rear_medium = 0;
    queue->count_medium = 0;

    queue->front_low = 0;
    queue->rear_low = 0;
    queue->count_low = 0;

    queue->count = 0;

    /*
     * Queue is initially active.
     */
    queue->shutdown = 0;
    queue->consecutive_high = 0;
    queue->consecutive_medium = 0;

    if (pthread_mutex_init(&queue->mutex, NULL) != 0)
        goto init_error;

    if (pthread_cond_init(&queue->not_empty, NULL) != 0)
    {
        pthread_mutex_destroy(&queue->mutex);
        goto init_error;
    }

    if (pthread_cond_init(&queue->not_full, NULL) != 0)
    {
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        goto init_error;
    }

    return 0;

init_error:
    free(queue->buffer_high);
    free(queue->buffer_medium);
    free(queue->buffer_low);

    queue->buffer_high = NULL;
    queue->buffer_medium = NULL;
    queue->buffer_low = NULL;

    return -1;
}


void queue_destroy(
    TaskQueue *queue)
{
    if (queue == NULL)
        return;

    free(queue->buffer_high);
    free(queue->buffer_medium);
    free(queue->buffer_low);

    queue->buffer_high = NULL;
    queue->buffer_medium = NULL;
    queue->buffer_low = NULL;

    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    pthread_mutex_destroy(&queue->mutex);
}

/*
 * Insert an item while the queue mutex is already locked.
 * The caller must verify that the queue is neither full nor shut down.
 */
static void enqueue_unlocked(
    TaskQueue *queue,
    void *item,
    TaskPriority priority)
{
    if (priority == PRIORITY_HIGH)
    {
        queue->buffer_high[queue->rear_high] = item;
        queue->rear_high =
            (queue->rear_high + 1) % queue->capacity;
        queue->count_high++;
    }
    else if (priority == PRIORITY_MEDIUM)
    {
        queue->buffer_medium[queue->rear_medium] = item;
        queue->rear_medium =
            (queue->rear_medium + 1) % queue->capacity;
        queue->count_medium++;
    }
    else
    {
        queue->buffer_low[queue->rear_low] = item;
        queue->rear_low =
            (queue->rear_low + 1) % queue->capacity;
        queue->count_low++;
    }

    queue->count++;

    pthread_cond_signal(&queue->not_empty);
}

/*
 * Blocking insertion.
 * Waits for space while the queue is full.
 */
int queue_push(
    TaskQueue *queue,
    void *item,
    TaskPriority priority)
{
    if (queue == NULL || item == NULL ||
        !valid_priority(priority))
    {
        return -1;
    }

    pthread_mutex_lock(&queue->mutex);

    while (queue->count >= queue->capacity &&
           !queue->shutdown)
    {
        return;
    }

    free(queue->buffer_high);
    free(queue->buffer_medium);
    free(queue->buffer_low);

    pthread_mutex_destroy(
        &queue->mutex
    );

    pthread_cond_destroy(
        &queue->not_empty
    );

    pthread_cond_destroy(
        &queue->not_full
    );
}


int queue_push(
    TaskQueue *queue,
    void *item,
    TaskPriority priority)
{
    if (queue == NULL || item == NULL)
    {
        return -1;
    }

    enqueue_unlocked(queue, item, priority);

    pthread_mutex_unlock(
        &queue->mutex
    );

    return 0;
}

/*
 * Non-blocking insertion.
 * Returns -1 immediately if the queue is full or shut down.
 */
int queue_try_push(
    TaskQueue *queue,
    void *item,
    TaskPriority priority)
{
    if (queue == NULL || item == NULL ||
        !valid_priority(priority))
    {
        return -1;
    }

    pthread_mutex_lock(&queue->mutex);

    if (queue->shutdown ||
        queue->count >= queue->capacity)
    {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    enqueue_unlocked(queue, item, priority);

    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

/*
 * Remove a particular item without freeing it.
 * The caller is responsible for handling its memory.
 */
int queue_remove(TaskQueue *queue, void *item)
{
    if (queue == NULL || item == NULL)
        return -1;

    pthread_mutex_lock(&queue->mutex);

    void **buffers[3] = {
        queue->buffer_high,
        queue->buffer_medium,
        queue->buffer_low
    };

    size_t *fronts[3] = {
        &queue->front_high,
        &queue->front_medium,
        &queue->front_low
    };

    size_t *rears[3] = {
        &queue->rear_high,
        &queue->rear_medium,
        &queue->rear_low
    };

    size_t *counts[3] = {
        &queue->count_high,
        &queue->count_medium,
        &queue->count_low
    };

    for (int p = 0; p < 3; p++)
    {
        size_t front = *fronts[p];
        size_t count = *counts[p];

        for (size_t i = 0; i < count; i++)
        {
            size_t index =
                (front + i) % queue->capacity;

            if (buffers[p][index] != item)
                continue;

            /* Shift later entries to preserve FIFO order. */
            for (size_t j = i; j + 1 < count; j++)
            {
                size_t current =
                    (front + j) % queue->capacity;

                size_t next =
                    (front + j + 1) % queue->capacity;

                buffers[p][current] = buffers[p][next];
            }

            size_t rear =
                (front + count - 1) % queue->capacity;

            buffers[p][rear] = NULL;
            *rears[p] = rear;

            (*counts[p])--;
            queue->count--;

            pthread_cond_signal(&queue->not_full);

            pthread_mutex_unlock(&queue->mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&queue->mutex);

    return -1;
}

/*
 * Remove the next task according to the priority fairness policy.
 */
void *queue_pop(TaskQueue *queue)
{
    if (queue == NULL)
        return NULL;

    pthread_mutex_lock(&queue->mutex);

void *queue_pop(
    TaskQueue *queue)
{
    pthread_mutex_lock(
        &queue->mutex
    );

    /*
     * Worker waits while the queue is empty.
     */
    while (
        queue->count == 0 &&
        !queue->shutdown
    )
    {
        pthread_cond_wait(
            &queue->not_empty,
            &queue->mutex
        );
    }

    /*
     * Shutdown is complete and no tasks remain.
     */
    if (
        queue->count == 0 &&
        queue->shutdown
    )
    {
        pthread_mutex_unlock(
            &queue->mutex
        );

        return NULL;
    }

    void *item = NULL;

    /* Prefer HIGH until the high-priority fairness limit is reached. */
    if (queue->count_high > 0 &&
        queue->consecutive_high < HIGH_LIMIT)
    {
        item = queue->buffer_high[queue->front_high];
        queue->buffer_high[queue->front_high] = NULL;

        queue->front_high =
            (queue->front_high + 1) % queue->capacity;

        queue->count_high--;

        queue->consecutive_high++;
        queue->consecutive_medium = 0;
    }
    /* Give MEDIUM a turn when available. */
    else if (queue->count_medium > 0)
    {
        item = queue->buffer_medium[queue->front_medium];
        queue->buffer_medium[queue->front_medium] = NULL;

        queue->front_medium =
            (queue->front_medium + 1) % queue->capacity;

        queue->count_medium--;

        queue->consecutive_medium++;
        queue->consecutive_high = 0;
    }
    /* Give LOW a turn after enough MEDIUM selections. */
    else if (queue->count_low > 0 &&
             queue->consecutive_medium >= MEDIUM_LIMIT)
    {
        item = queue->buffer_low[queue->front_low];
        queue->buffer_low[queue->front_low] = NULL;

        queue->front_low =
            (queue->front_low + 1) % queue->capacity;

        queue->count_low--;

        queue->consecutive_high = 0;
        queue->consecutive_medium = 0;
    }
    /* If MEDIUM is empty, allow LOW to proceed. */
    else if (queue->count_low > 0 &&
             queue->count_medium == 0)
    {
        item = queue->buffer_low[queue->front_low];
        queue->buffer_low[queue->front_low] = NULL;

        queue->front_low =
            (queue->front_low + 1) % queue->capacity;

        queue->count_low--;

        queue->consecutive_high = 0;
        queue->consecutive_medium = 0;
    }
    /* Fall back to HIGH if it still has queued tasks. */
    else if (queue->count_high > 0)
    {
        item = queue->buffer_high[queue->front_high];
        queue->buffer_high[queue->front_high] = NULL;

        queue->front_high =
            (queue->front_high + 1) % queue->capacity;

        queue->count_high--;

        queue->consecutive_high++;
    }
    /* Final fallback to MEDIUM. */
    else if (queue->count_medium > 0)
    {
        item = queue->buffer_medium[queue->front_medium];
        queue->buffer_medium[queue->front_medium] = NULL;

        queue->front_medium =
            (queue->front_medium + 1) % queue->capacity;

        queue->count_medium--;

        queue->consecutive_medium++;
        queue->consecutive_high = 0;
    }

    if (item != NULL)
        queue->count--;

    /*
     * Wake a producer waiting because the
     * bounded queue was full.
     */
    pthread_cond_signal(
        &queue->not_full
    );

    pthread_mutex_unlock(
        &queue->mutex
    );

    return item;
}

void queue_shutdown(TaskQueue *queue)
{
    if (queue == NULL)
        return;

    pthread_mutex_lock(&queue->mutex);

void queue_shutdown(
    TaskQueue *queue)
{
    if (queue == NULL)
    {
        return;
    }

    pthread_mutex_lock(
        &queue->mutex
    );

    /*
     * Mark queue as shutting down.
     */
    queue->shutdown = 1;

    /*
     * Wake all workers.
     */
    pthread_cond_broadcast(
        &queue->not_empty
    );

    /*
     * Wake all producers.
     */
    pthread_cond_broadcast(
        &queue->not_full
    );

    pthread_mutex_unlock(
        &queue->mutex
    );
}

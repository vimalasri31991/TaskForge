#include "queue.h"

#include <stdlib.h>

/*
 * Fairness limits
 *
 * A maximum of 3 consecutive HIGH priority
 * tasks can be selected before MEDIUM/LOW
 * gets an opportunity.
 *
 * A maximum of 3 consecutive MEDIUM priority
 * tasks can be selected before LOW gets an
 * opportunity.
 */
#define HIGH_LIMIT 3
#define MEDIUM_LIMIT 3


int queue_init(
    TaskQueue *queue,
    size_t capacity)
{
    if (queue == NULL || capacity == 0)
    {
        return -1;
    }

    /*
     * Separate bounded FIFO buffers are maintained
     * for HIGH, MEDIUM and LOW priority tasks.
     */
    queue->buffer_high =
        malloc(sizeof(void *) * capacity);

    queue->buffer_medium =
        malloc(sizeof(void *) * capacity);

    queue->buffer_low =
        malloc(sizeof(void *) * capacity);

    if (queue->buffer_high == NULL ||
        queue->buffer_medium == NULL ||
        queue->buffer_low == NULL)
    {
        free(queue->buffer_high);
        free(queue->buffer_medium);
        free(queue->buffer_low);

        return -1;
    }

    queue->capacity = capacity;

    /*
     * HIGH queue.
     */
    queue->front_high = 0;
    queue->rear_high = 0;
    queue->count_high = 0;

    /*
     * MEDIUM queue.
     */
    queue->front_medium = 0;
    queue->rear_medium = 0;
    queue->count_medium = 0;

    /*
     * LOW queue.
     */
    queue->front_low = 0;
    queue->rear_low = 0;
    queue->count_low = 0;

    /*
     * Total number of tasks.
     */
    queue->count = 0;

    /*
     * Queue is initially active.
     */
    queue->shutdown = 0;

    /*
     * Fairness counters.
     */
    queue->consecutive_high = 0;
    queue->consecutive_medium = 0;

    /*
     * Synchronization primitives.
     */
    pthread_mutex_init(
        &queue->mutex,
        NULL
    );

    pthread_cond_init(
        &queue->not_empty,
        NULL
    );

    pthread_cond_init(
        &queue->not_full,
        NULL
    );

    return 0;
}


void queue_destroy(
    TaskQueue *queue)
{
    if (queue == NULL)
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

    pthread_mutex_lock(
        &queue->mutex
    );

    /*
     * Producer waits when the entire bounded
     * queue is full.
     */
    while (
        queue->count == queue->capacity &&
        !queue->shutdown
    )
    {
        pthread_cond_wait(
            &queue->not_full,
            &queue->mutex
        );
    }

    /*
     * Shutdown rejects new tasks.
     */
    if (queue->shutdown)
    {
        pthread_mutex_unlock(
            &queue->mutex
        );

        return -1;
    }

    /*
     * Insert task into the appropriate
     * priority FIFO queue.
     */
    if (priority == PRIORITY_HIGH)
    {
        queue->buffer_high[
            queue->rear_high
        ] = item;

        queue->rear_high =
            (queue->rear_high + 1)
            % queue->capacity;

        queue->count_high++;
    }
    else if (priority == PRIORITY_MEDIUM)
    {
        queue->buffer_medium[
            queue->rear_medium
        ] = item;

        queue->rear_medium =
            (queue->rear_medium + 1)
            % queue->capacity;

        queue->count_medium++;
    }
    else
    {
        queue->buffer_low[
            queue->rear_low
        ] = item;

        queue->rear_low =
            (queue->rear_low + 1)
            % queue->capacity;

        queue->count_low++;
    }

    /*
     * Update total queue size.
     */
    queue->count++;

    /*
     * Wake one worker waiting for a task.
     */
    pthread_cond_signal(
        &queue->not_empty
    );

    pthread_mutex_unlock(
        &queue->mutex
    );

    return 0;
}


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

    /*
     * ------------------------------------------------
     * PRIORITY + FAIRNESS SCHEDULER
     * ------------------------------------------------
     *
     * Normally HIGH has the highest priority.
     *
     * However, after HIGH_LIMIT consecutive HIGH
     * tasks, MEDIUM or LOW gets a chance.
     */

    if (
        queue->count_high > 0 &&
        queue->consecutive_high < HIGH_LIMIT
    )
    {
        /*
         * Select HIGH.
         */
        item =
            queue->buffer_high[
                queue->front_high
            ];

        queue->front_high =
            (queue->front_high + 1)
            % queue->capacity;

        queue->count_high--;

        queue->consecutive_high++;

        /*
         * HIGH execution resets the MEDIUM
         * consecutive counter.
         */
        queue->consecutive_medium = 0;
    }

    /*
     * HIGH fairness limit reached.
     *
     * Prefer MEDIUM if available.
     */
    else if (
        queue->count_medium > 0
    )
    {
        item =
            queue->buffer_medium[
                queue->front_medium
            ];

        queue->front_medium =
            (queue->front_medium + 1)
            % queue->capacity;

        queue->count_medium--;

        queue->consecutive_medium++;

        /*
         * MEDIUM execution resets HIGH
         * consecutive counter.
         */
        queue->consecutive_high = 0;
    }

    /*
     * MEDIUM fairness limit reached.
     *
     * Give LOW a chance.
     */
    else if (
        queue->count_low > 0 &&
        queue->consecutive_medium >= MEDIUM_LIMIT
    )
    {
        item =
            queue->buffer_low[
                queue->front_low
            ];

        queue->front_low =
            (queue->front_low + 1)
            % queue->capacity;

        queue->count_low--;

        queue->consecutive_high = 0;
        queue->consecutive_medium = 0;
    }

    /*
     * No MEDIUM task exists.
     *
     * LOW can execute even if HIGH exists,
     * preventing LOW starvation.
     */
    else if (
        queue->count_low > 0 &&
        queue->count_medium == 0
    )
    {
        item =
            queue->buffer_low[
                queue->front_low
            ];

        queue->front_low =
            (queue->front_low + 1)
            % queue->capacity;

        queue->count_low--;

        queue->consecutive_high = 0;
        queue->consecutive_medium = 0;
    }

    /*
     * Fallback to HIGH.
     */
    else if (
        queue->count_high > 0
    )
    {
        item =
            queue->buffer_high[
                queue->front_high
            ];

        queue->front_high =
            (queue->front_high + 1)
            % queue->capacity;

        queue->count_high--;

        queue->consecutive_high++;
    }

    /*
     * Final fallback to MEDIUM.
     */
    else
    {
        item =
            queue->buffer_medium[
                queue->front_medium
            ];

        queue->front_medium =
            (queue->front_medium + 1)
            % queue->capacity;

        queue->count_medium--;

        queue->consecutive_medium++;
        queue->consecutive_high = 0;
    }

    /*
     * One slot has now become available.
     */
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

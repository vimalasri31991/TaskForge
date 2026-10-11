#include "future.h"


int future_init(Future *future)
{
    if (future == NULL)
    {
        return -1;
    }

    if (pthread_mutex_init(&future->mutex, NULL) != 0)
    {
        return -1;
    }

    if (pthread_cond_init(&future->condition, NULL) != 0)
    {
        pthread_mutex_destroy(&future->mutex);
        return -1;
    }

    future->status = FUTURE_PENDING;
    future->result = 0;

    return 0;
}


/*
 * A worker claims a pending task by changing
 * its status to RUNNING.
 */
void future_set_running(Future *future)
{
    if (future == NULL)
    {
        return;
    }

    pthread_mutex_lock(&future->mutex);

    if (future->status == FUTURE_PENDING)
    {
        future->status = FUTURE_RUNNING;
    }

    pthread_mutex_unlock(&future->mutex);
}


/*
 * Publish the task result.
 */
void future_set(Future *future, int result)
{
    if (future == NULL)
    {
        return;
    }

    pthread_mutex_lock(&future->mutex);

    if (future->status == FUTURE_RUNNING)
    {
        future->result = result;
        future->status = FUTURE_COMPLETED;

        pthread_cond_broadcast(&future->condition);
    }

    pthread_mutex_unlock(&future->mutex);
}


/*
 * Cancel only a task that is still pending.
 *
 * Returns:
 *  0  = cancellation successful
 * -1  = task is already running/completed/cancelled
 */
int future_cancel(Future *future)
{
    if (future == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&future->mutex);

    if (future->status != FUTURE_PENDING)
    {
        pthread_mutex_unlock(&future->mutex);
        return -1;
    }

    future->status = FUTURE_CANCELLED;

    pthread_cond_broadcast(&future->condition);

    pthread_mutex_unlock(&future->mutex);

    return 0;
}


/*
 * Wait for completion or cancellation.
 *
 * Returns the task result on completion.
 * Returns -1 if the task was cancelled.
 */
int future_get(Future *future)
{
    if (future == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&future->mutex);

    while (
        future->status == FUTURE_PENDING ||
        future->status == FUTURE_RUNNING
    )
    {
        pthread_cond_wait(
            &future->condition,
            &future->mutex
        );
    }

    int result;

    if (future->status == FUTURE_CANCELLED)
    {
        result = -1;
    }
    else
    {
        result = future->result;
    }

    pthread_mutex_unlock(&future->mutex);

    return result;
}


/*
 * Read the current Future status.
 */
FutureStatus future_get_status(Future *future)
{
    if (future == NULL)
    {
        return FUTURE_CANCELLED;
    }

    pthread_mutex_lock(&future->mutex);

    FutureStatus status = future->status;

    pthread_mutex_unlock(&future->mutex);

    return status;
}


void future_destroy(Future *future)
{
    if (future == NULL)
    {
        return;
    }

    pthread_mutex_destroy(&future->mutex);
    pthread_cond_destroy(&future->condition);
}

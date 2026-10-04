#include "future.h"

int future_init(Future *future)
{
    if (future == NULL)
        return -1;

    pthread_mutex_init(&future->mutex, NULL);
    pthread_cond_init(&future->condition, NULL);

    future->completed = 0;
    future->result = 0;

    return 0;
}

void future_set(Future *future, int result)
{
    pthread_mutex_lock(&future->mutex);

    future->result = result;
    future->completed = 1;

    pthread_cond_signal(&future->condition);

    pthread_mutex_unlock(&future->mutex);
}

int future_get(Future *future)
{
    pthread_mutex_lock(&future->mutex);

    while (!future->completed)
    {
        pthread_cond_wait(
            &future->condition,
            &future->mutex
        );
    }

    int result = future->result;

    pthread_mutex_unlock(&future->mutex);

    return result;
}

void future_destroy(Future *future)
{
    if (future == NULL)
        return;

    pthread_mutex_destroy(&future->mutex);
    pthread_cond_destroy(&future->condition);
}

#include "queue.h"
#include <stdlib.h>

int queue_init(TaskQueue *queue, size_t capacity)
{
    if (queue == NULL || capacity == 0)
        return -1;

    queue->buffer = malloc(sizeof(void *) * capacity);

    if (queue->buffer == NULL)
        return -1;

    queue->capacity = capacity;
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    queue->shutdown = 0;

    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);

    return 0;
}

void queue_destroy(TaskQueue *queue)
{
    if (queue == NULL)
        return;

    free(queue->buffer);

    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
}

int queue_push(TaskQueue *queue, void *item)
{
    pthread_mutex_lock(&queue->mutex);

    while (queue->count == queue->capacity && !queue->shutdown)
    {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    if (queue->shutdown)
    {
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    queue->buffer[queue->rear] = item;
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->count++;

    pthread_cond_signal(&queue->not_empty);

    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

void *queue_pop(TaskQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);

    while (queue->count == 0 && !queue->shutdown)
    {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    if (queue->count == 0 && queue->shutdown)
    {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    void *item = queue->buffer[queue->front];

    queue->front = (queue->front + 1) % queue->capacity;
    queue->count--;

    pthread_cond_signal(&queue->not_full);

    pthread_mutex_unlock(&queue->mutex);

    return item;
}

void queue_shutdown(TaskQueue *queue)
{
    pthread_mutex_lock(&queue->mutex);

    queue->shutdown = 1;

    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);

    pthread_mutex_unlock(&queue->mutex);
}

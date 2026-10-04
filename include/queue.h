#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stddef.h>

typedef struct {
    void **buffer;

    size_t capacity;
    size_t front;
    size_t rear;
    size_t count;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    int shutdown;
} TaskQueue;

int queue_init(TaskQueue *queue, size_t capacity);

void queue_destroy(TaskQueue *queue);

int queue_push(TaskQueue *queue, void *item);

void *queue_pop(TaskQueue *queue);

void queue_shutdown(TaskQueue *queue);

#endif

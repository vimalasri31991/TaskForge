#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stddef.h>

typedef enum
{
    PRIORITY_HIGH = 0,
    PRIORITY_MEDIUM = 1,
    PRIORITY_LOW = 2
} TaskPriority;

typedef struct
{
    void **buffer_high;
    void **buffer_medium;
    void **buffer_low;

    size_t capacity;

    size_t front_high;
    size_t rear_high;
    size_t count_high;

    size_t front_medium;
    size_t rear_medium;
    size_t count_medium;

    size_t front_low;
    size_t rear_low;
    size_t count_low;

    size_t count;

    pthread_mutex_t mutex;

    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    int shutdown;

    /*
     * Fairness counters.
     *
     * After a limited number of consecutive
     * high-priority tasks, a lower-priority
     * task gets a chance.
     */
    int consecutive_high;
    int consecutive_medium;

} TaskQueue;


int queue_init(
    TaskQueue *queue,
    size_t capacity
);


void queue_destroy(
    TaskQueue *queue
);


int queue_push(
    TaskQueue *queue,
    void *item,
    TaskPriority priority
);


void *queue_pop(
    TaskQueue *queue
);


void queue_shutdown(
    TaskQueue *queue
);


#endif


#include "taskforge.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_condition = PTHREAD_COND_INITIALIZER;

static int gate_started = 0;
static int release_gate = 0;

static int blocking_task(void *argument)
{
    (void)argument;

    pthread_mutex_lock(&gate_mutex);

    gate_started = 1;
    pthread_cond_broadcast(&gate_condition);

    while (!release_gate)
        pthread_cond_wait(&gate_condition, &gate_mutex);

    pthread_mutex_unlock(&gate_mutex);

    return 100;
}

static int value_task(void *argument)
{
    int result = *(int *)argument;
    return result;
}

static void release_future(Future *future)
{
    if (future != NULL)
    {
        future_destroy(future);
        free(future);
    }
}

int main(void)
{
    TaskForge pool;

    if (taskforge_init(&pool, 1, 2) != 0)
    {
        fprintf(stderr, "TaskForge initialization failed.\n");
        return 1;
    }

    /*
     * Occupy the only worker. The gate task stays running until
     * the main thread releases it.
     */
    Future *gate_future = taskforge_submit(
        &pool, blocking_task, NULL, PRIORITY_HIGH);

    if (gate_future == NULL)
    {
        fprintf(stderr, "Could not submit gate task.\n");
        taskforge_shutdown(&pool);
        return 1;
    }

    pthread_mutex_lock(&gate_mutex);

    while (!gate_started)
        pthread_cond_wait(&gate_condition, &gate_mutex);

    pthread_mutex_unlock(&gate_mutex);

    /*
     * The only worker is blocked, so these two tasks fill the queue.
     */
    int *value1 = malloc(sizeof(int));
    int *value2 = malloc(sizeof(int));
    int *rejected_value = malloc(sizeof(int));

    if (value1 == NULL || value2 == NULL || rejected_value == NULL)
    {
        fprintf(stderr, "Memory allocation failed.\n");

        free(value1);
        free(value2);
        free(rejected_value);

        pthread_mutex_lock(&gate_mutex);
        release_gate = 1;
        pthread_cond_broadcast(&gate_condition);
        pthread_mutex_unlock(&gate_mutex);

        release_future(gate_future);
        taskforge_shutdown(&pool);
        return 1;
    }

    *value1 = 10;
    *value2 = 20;
    *rejected_value = 30;

    Future *future1 = taskforge_submit(
        &pool, value_task, value1, PRIORITY_MEDIUM);

    Future *future2 = taskforge_submit(
        &pool, value_task, value2, PRIORITY_MEDIUM);

    if (future1 == NULL || future2 == NULL)
    {
        fprintf(stderr, "Could not fill the queue as expected.\n");

        /*
         * Ownership transfers only when submission succeeds.
         */
        if (future1 == NULL)
            free(value1);

        if (future2 == NULL)
            free(value2);

        free(rejected_value);

        pthread_mutex_lock(&gate_mutex);
        release_gate = 1;
        pthread_cond_broadcast(&gate_condition);
        pthread_mutex_unlock(&gate_mutex);

        release_future(gate_future);
        release_future(future1);
        release_future(future2);

        taskforge_shutdown(&pool);
        return 1;
    }

    /*
     * The queue is full. try_submit must return NULL without waiting.
     */
    Future *rejected = taskforge_try_submit(
        &pool, value_task, rejected_value, PRIORITY_MEDIUM);

    if (rejected != NULL)
    {
        fprintf(stderr, "FAIL: submission unexpectedly succeeded.\n");
        release_future(rejected);
    }
    else
    {
        printf("PASS: full queue rejected submission immediately.\n");

        /* Failed submission leaves argument ownership with us. */
        free(rejected_value);
    }

    /*
     * Allow the worker to finish the queued tasks.
     */
    pthread_mutex_lock(&gate_mutex);
    release_gate = 1;
    pthread_cond_broadcast(&gate_condition);
    pthread_mutex_unlock(&gate_mutex);

    int gate_result = future_get(gate_future);
    int result1 = future_get(future1);
    int result2 = future_get(future2);

    printf("Gate task result: %d\n", gate_result);
    printf("Queued task 1 result: %d\n", result1);
    printf("Queued task 2 result: %d\n", result2);

    int passed = (rejected == NULL &&
                  gate_result == 100 &&
                  result1 == 10 &&
                  result2 == 20);

    release_future(gate_future);
    release_future(future1);
    release_future(future2);

    taskforge_shutdown(&pool);

    pthread_cond_destroy(&gate_condition);
    pthread_mutex_destroy(&gate_mutex);

    if (passed)
    {
        printf("\nMODULE 4 TEST PASSED\n");
        return 0;
    }

    printf("\nMODULE 4 TEST FAILED\n");
    return 1;
}

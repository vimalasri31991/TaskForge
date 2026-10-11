#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "taskforge.h"


static pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_condition = PTHREAD_COND_INITIALIZER;

static int release_worker = 0;
static int second_task_executed = 0;


int blocking_task(void *argument)
{
    (void)argument;

    pthread_mutex_lock(&gate_mutex);

    while (!release_worker)
    {
        pthread_cond_wait(
            &gate_condition,
            &gate_mutex
        );
    }

    pthread_mutex_unlock(&gate_mutex);

    return 100;
}


int normal_task(void *argument)
{
    int value = *(int *)argument;

    second_task_executed = 1;

    return value * 10;
}


int main(void)
{
    TaskForge pool;

    printf("====================================\n");
    printf("     TASKFORGE MODULE 3 TEST        \n");
    printf("          TASK CANCELLATION         \n");
    printf("====================================\n");


    if (taskforge_init(&pool, 1, 4) != 0)
    {
        printf("Failed to initialize TaskForge\n");
        return 1;
    }


    /*
     * Submit a task that blocks the only worker.
     */
    Future *running_future =
        taskforge_submit(
            &pool,
            blocking_task,
            NULL,
            PRIORITY_HIGH
        );

    if (running_future == NULL)
    {
        printf("Failed to submit blocking task\n");
        taskforge_shutdown(&pool);
        return 1;
    }


    /*
     * Give the worker time to pick up the first task.
     */
    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = 100000000L;

    nanosleep(&delay, NULL);


    /*
     * Submit a second task. The only worker is blocked,
     * so this task should remain queued.
     */
    int *value = malloc(sizeof(int));

    if (value == NULL)
    {
        pthread_mutex_lock(&gate_mutex);
        release_worker = 1;
        pthread_cond_broadcast(&gate_condition);
        pthread_mutex_unlock(&gate_mutex);

        future_get(running_future);
        future_destroy(running_future);
        free(running_future);

        taskforge_shutdown(&pool);

        return 1;
    }

    *value = 7;

    Future *cancel_future =
        taskforge_submit(
            &pool,
            normal_task,
            value,
            PRIORITY_LOW
        );

    if (cancel_future == NULL)
    {
        free(value);

        pthread_mutex_lock(&gate_mutex);
        release_worker = 1;
        pthread_cond_broadcast(&gate_condition);
        pthread_mutex_unlock(&gate_mutex);

        future_get(running_future);
        future_destroy(running_future);
        free(running_future);

        taskforge_shutdown(&pool);

        return 1;
    }


    /*
     * Cancel the second task before the worker can run it.
     */
    int cancel_result =
        taskforge_cancel(
            &pool,
            cancel_future
        );

    if (cancel_result == 0)
    {
        printf("Queued task cancelled successfully.\n");
    }
    else
    {
        printf("Cancellation failed unexpectedly.\n");
    }


    /*
     * Release the running task.
     */
    pthread_mutex_lock(&gate_mutex);

    release_worker = 1;

    pthread_cond_broadcast(&gate_condition);

    pthread_mutex_unlock(&gate_mutex);


    int running_result =
        future_get(running_future);

    printf(
        "Running task result: %d\n",
        running_result
    );


    int cancelled_result =
        future_get(cancel_future);

    printf(
        "Cancelled task Future result: %d\n",
        cancelled_result
    );


    FutureStatus cancelled_status =
        future_get_status(cancel_future);

    printf(
        "Cancelled task status: %s\n",
        cancelled_status == FUTURE_CANCELLED
            ? "CANCELLED"
            : "UNEXPECTED"
    );


    printf(
        "Cancelled task executed: %s\n",
        second_task_executed ? "YES" : "NO"
    );


    int passed =
        cancel_result == 0 &&
        running_result == 100 &&
        cancelled_result == -1 &&
        cancelled_status == FUTURE_CANCELLED &&
        second_task_executed == 0;


    future_destroy(running_future);
    free(running_future);

    future_destroy(cancel_future);
    free(cancel_future);


    taskforge_shutdown(&pool);


    pthread_mutex_destroy(&gate_mutex);
    pthread_cond_destroy(&gate_condition);


    if (passed)
    {
        printf("\nMODULE 3 TEST PASSED\n");
    }
    else
    {
        printf("\nMODULE 3 TEST FAILED\n");
    }


    return passed ? 0 : 1;
}

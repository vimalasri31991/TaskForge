#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "taskforge.h"


/*
 * Task function:
 * Receives an integer and returns its square.
 */
int square_task(void *argument)
{
    int value = *(int *)argument;

    printf(
        "Worker processing %d\n",
        value
    );

    /*
     * Sleep for 100 milliseconds.
     * This is only for demonstration so
     * concurrent worker execution is visible.
     */
    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = 100000000L;

    nanosleep(&delay, NULL);

    return value * value;
}


int main(void)
{
    TaskForge pool;

    printf("====================================\n");
    printf("       TASKFORGE DEMONSTRATION      \n");
    printf("====================================\n");


    /*
     * Initialize TaskForge.
     *
     * 4 worker threads
     * Queue capacity = 4
     */
    printf("Creating thread pool...\n");

    if (taskforge_init(&pool, 4, 4) != 0)
    {
        printf("Failed to initialize TaskForge\n");
        return 1;
    }

    printf("Workers : 4\n");
    printf("Queue   : 4\n");

    printf("\nSubmitting tasks...\n");


    /*
     * Store futures for all submitted tasks.
     */
    Future *futures[10];

    for (int i = 0; i < 10; i++)
    {
        /*
         * Allocate memory for the task argument.
         */
        int *value = malloc(sizeof(int));

        if (value == NULL)
        {
            printf(
                "Memory allocation failed for task %d\n",
                i + 1
            );

            /*
             * Shutdown before exiting.
             */
            taskforge_shutdown(&pool);

            return 1;
        }

        *value = i + 1;


        /*
         * Submit task to TaskForge.
         */
        futures[i] =
            taskforge_submit(
                &pool,
                square_task,
                value
            );


        /*
         * Check whether submission succeeded.
         */
        if (futures[i] == NULL)
        {
            printf(
                "Task %d submission failed\n",
                i + 1
            );

            /*
             * The task was not accepted,
             * so its argument must be freed here.
             */
            free(value);
        }
        else
        {
            printf(
                "Submitted task %d\n",
                i + 1
            );
        }
    }


    /*
     * Wait for all task results.
     */
    printf("\nWaiting for results...\n");

    for (int i = 0; i < 10; i++)
    {
        if (futures[i] != NULL)
        {
            /*
             * future_get() blocks until the
             * worker completes the task.
             */
            int result =
                future_get(futures[i]);

            printf(
                "Task %d result = %d\n",
                i + 1,
                result
            );


            /*
             * Future is no longer needed.
             */
            future_destroy(futures[i]);

            free(futures[i]);

            futures[i] = NULL;
        }
    }


    /*
     * Graceful shutdown.
     */
    printf("\nInitiating graceful shutdown...\n");

    taskforge_shutdown(&pool);

    printf("All workers joined.\n");
    printf("TaskForge shutdown complete.\n");

    printf("\n====================================\n");
    printf("        TEST COMPLETED SUCCESSFULLY \n");
    printf("====================================\n");

    return 0;
}

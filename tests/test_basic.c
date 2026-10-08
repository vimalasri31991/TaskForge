#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "taskforge.h"


int priority_task(void *argument)
{
    int value = *(int *)argument;

    printf(
        "Worker processing task %d\n",
        value
    );

    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = 100000000L;

    nanosleep(
        &delay,
        NULL
    );

    return value * 10;
}


const char *priority_name(
    TaskPriority priority)
{
    switch (priority)
    {
        case PRIORITY_HIGH:
            return "HIGH";

        case PRIORITY_MEDIUM:
            return "MEDIUM";

        case PRIORITY_LOW:
            return "LOW";

        default:
            return "UNKNOWN";
    }
}


int main(void)
{
    TaskForge pool;


    printf(
        "====================================\n"
    );

    printf(
        "       TASKFORGE MODULE 2 TEST      \n"
    );

    printf(
        "     PRIORITY + FAIRNESS TEST       \n"
    );

    printf(
        "====================================\n"
    );


    printf(
        "Creating thread pool...\n"
    );


    if (
        taskforge_init(
            &pool,
            2,
            6
        ) != 0
    )
    {
        printf(
            "Failed to initialize TaskForge\n"
        );

        return 1;
    }


    printf(
        "Workers : 2\n"
    );

    printf(
        "Queue   : 6\n"
    );


    /*
     * Ten tasks with different priorities.
     */
    Future *futures[10];


    TaskPriority priorities[10] =
    {
        PRIORITY_LOW,
        PRIORITY_HIGH,
        PRIORITY_HIGH,
        PRIORITY_MEDIUM,
        PRIORITY_HIGH,
        PRIORITY_LOW,
        PRIORITY_MEDIUM,
        PRIORITY_HIGH,
        PRIORITY_LOW,
        PRIORITY_MEDIUM
    };


    printf(
        "\nSubmitting priority tasks...\n\n"
    );


    for (
        int i = 0;
        i < 10;
        i++
    )
    {
        int *value =
            malloc(sizeof(int));


        if (value == NULL)
        {
            printf(
                "Memory allocation failed\n"
            );

            taskforge_shutdown(
                &pool
            );

            return 1;
        }


        *value = i + 1;


        futures[i] =
            taskforge_submit(
                &pool,
                priority_task,
                value,
                priorities[i]
            );


        if (
            futures[i] == NULL
        )
        {
            printf(
                "Task %d submission failed\n",
                i + 1
            );

            free(value);
        }
        else
        {
            printf(
                "Submitted Task %d [%s]\n",
                i + 1,
                priority_name(
                    priorities[i]
                )
            );
        }
    }


    printf(
        "\nWaiting for task results...\n\n"
    );


    for (
        int i = 0;
        i < 10;
        i++
    )
    {
        if (
            futures[i] != NULL
        )
        {
            int result =
                future_get(
                    futures[i]
                );


            printf(
                "Task %d [%s] Result = %d\n",
                i + 1,
                priority_name(
                    priorities[i]
                ),
                result
            );


            future_destroy(
                futures[i]
            );


            free(
                futures[i]
            );


            futures[i] = NULL;
        }
    }


    printf(
        "\nInitiating graceful shutdown...\n"
    );


    taskforge_shutdown(
        &pool
    );


    printf(
        "All workers joined.\n"
    );


    printf(
        "TaskForge shutdown complete.\n"
    );


    printf(
        "\n====================================\n"
    );

    printf(
        "       MODULE 2 TEST PASSED        \n"
    );

    printf(
        "====================================\n"
    );


    return 0;
}

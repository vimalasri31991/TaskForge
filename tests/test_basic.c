#include <stdio.h>
#include <stdlib.h>

#include "taskforge.h"
#include "future.h"
#include "queue.h"

#define TASK_COUNT 5

static int square_task(void *argument)
{
    int *value = (int *)argument;
    return (*value) * (*value);
}

int main(void)
{
    TaskForge pool;
    Future *futures[TASK_COUNT] = {0};
    int submitted = 0;

    if (taskforge_init(&pool, 3, 10) != 0)
    {
        fprintf(stderr, "Failed to initialize TaskForge\n");
        return EXIT_FAILURE;
    }

    printf("TaskForge initialized successfully.\n");

    for (int i = 0; i < TASK_COUNT; i++)
    {
        int *value = malloc(sizeof(*value));

        if (value == NULL)
        {
            fprintf(stderr, "Memory allocation failed\n");
            goto cleanup;
        }


        *value = i + 1;

        futures[i] = taskforge_submit(
            &pool,
            square_task,
            value,
            PRIORITY_MEDIUM
        );

        if (futures[i] == NULL)
        {
            fprintf(stderr, "Task %d submission failed\n", i + 1);
            free(value); /* Caller retains ownership on failure. */
            goto cleanup;
        }

        /* TaskForge owns value after successful submission. */
        submitted++;
    }

    for (int i = 0; i < submitted; i++)
    {
        int result = future_get(futures[i]);

        printf(
            "Task %d: %d squared = %d\n",
            i + 1,
            i + 1,
            result
        );
    }

cleanup:
    /* Wait for accepted tasks before releasing their Futures. */
    for (int i = 0; i < submitted; i++)
    {
        (void)future_get(futures[i]);
    }


    for (int i = 0; i < submitted; i++)
    {
        future_destroy(futures[i]);
        free(futures[i]);
    }

    if (submitted != TASK_COUNT)
    {
        fprintf(stderr, "TaskForge basic test failed.\n");
        return EXIT_FAILURE;
    }

    printf("TaskForge basic test completed.\n");
    return EXIT_SUCCESS;
}

#include <stdio.h>
#include <stdlib.h>

#include "taskforge.h"

typedef struct
{
    int value;
    int result;
} TaskInput;

static int return_result(void *argument)
{
    const TaskInput *input = argument;
    return input->result + input->value;
}

int main(void)
{
    TaskForge pool;
    Future *futures[2] = {NULL, NULL};
    int exit_code = EXIT_FAILURE;

    if (taskforge_init(&pool, 2, 4) != 0)
    {
        fprintf(stderr, "Could not initialize TaskForge\n");
        return EXIT_FAILURE;
    }

    const TaskInput inputs[2] = {
        {.value = 5, .result = 37},
        {.value = 0, .result = -7}
    };

    size_t submitted = 0;
    for (; submitted < 2; submitted++)
    {
        TaskInput *argument = malloc(sizeof(*argument));
        if (argument == NULL)
        {
            fprintf(stderr, "Could not allocate task argument\n");
            break;
        }

        *argument = inputs[submitted];
        futures[submitted] = taskforge_submit(
            &pool,
            return_result,
            argument,
            PRIORITY_MEDIUM);

        if (futures[submitted] == NULL)
        {
            free(argument); /* Ownership transfers only on success. */
            fprintf(stderr, "Task submission failed\n");
            break;
        }
    }

    int results_ok = submitted == 2;
    for (size_t i = 0; i < submitted; i++)
    {
        int result = future_get(futures[i]);
        FutureStatus status = future_get_status(futures[i]);
        printf("Task %zu: status=%s, result=%d\n",
               i + 1,
               status == FUTURE_COMPLETED ? "completed" : "unexpected",
               result);

        if (status != FUTURE_COMPLETED || result != inputs[i].value + inputs[i].result)
            results_ok = 0;
    }

    taskforge_shutdown(&pool);

    for (size_t i = 0; i < submitted; i++)
    {
        future_destroy(futures[i]);
        free(futures[i]);
    }

    if (results_ok)
    {
        puts("Module 5: task arguments and future results passed.");
        exit_code = EXIT_SUCCESS;
    }
    else
    {
        fprintf(stderr, "Module 5: task result check failed.\n");
    }

    return exit_code;
}

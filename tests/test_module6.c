#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "taskforge.h"

#define TASK_COUNT 8

typedef struct
{
    TaskForge *pool;
    sigset_t wait_set;
    atomic_int interrupted;
} SignalControl;

static int delayed_result(void *argument)
{
    int *value = argument;
    struct timespec delay = {.tv_sec = 1, .tv_nsec = 0};

    while (nanosleep(&delay, &delay) == -1 && errno == EINTR)
    {
        /* Retry with the remaining time. */
    }

    return *value * 10;
}

static void *signal_waiter(void *argument)
{
    SignalControl *control = argument;
    int signal_number;
    int error = sigwait(&control->wait_set, &signal_number);

    if (error != 0)
    {
        fprintf(stderr, "sigwait failed: %d\n", error);
        return NULL;
    }

    if (signal_number == SIGINT)
    {
        puts("SIGINT received; draining queued tasks and stopping workers.");
        taskforge_shutdown(control->pool);
        atomic_store(&control->interrupted, 1);
    }

    return NULL;
}

int main(void)
{
    TaskForge pool;
    Future *futures[TASK_COUNT] = {0};
    SignalControl control;
    sigset_t original_mask;
    pthread_t signal_thread;
    size_t submitted = 0;
    int signal_thread_started = 0;
    int result = EXIT_FAILURE;

    sigemptyset(&control.wait_set);
    sigaddset(&control.wait_set, SIGINT);
    sigaddset(&control.wait_set, SIGUSR1);
    atomic_init(&control.interrupted, 0);

    /* Block signals before creating workers so every thread inherits the mask. */
    int error = pthread_sigmask(SIG_BLOCK, &control.wait_set, &original_mask);
    if (error != 0)
    {
        fprintf(stderr, "Could not block signals: %d\n", error);
        return EXIT_FAILURE;
    }

    if (taskforge_init(&pool, 3, 4) != 0)
    {
        fprintf(stderr, "Could not initialize TaskForge\n");
        (void)pthread_sigmask(SIG_SETMASK, &original_mask, NULL);
        return EXIT_FAILURE;
    }

    control.pool = &pool;
    error = pthread_create(&signal_thread, NULL, signal_waiter, &control);
    if (error != 0)
    {
        fprintf(stderr, "Could not start signal waiter: %d\n", error);
        taskforge_shutdown(&pool);
        (void)pthread_sigmask(SIG_SETMASK, &original_mask, NULL);
        return EXIT_FAILURE;
    }
    signal_thread_started = 1;

    puts("Tasks are being submitted; wait for the submission message, then press Ctrl-C.");

    for (; submitted < TASK_COUNT; submitted++)
    {
        int *argument = malloc(sizeof(*argument));
        if (argument == NULL)
        {
            fprintf(stderr, "Could not allocate task argument\n");
            break;
        }

        *argument = (int)submitted + 1;
        futures[submitted] = taskforge_submit(
            &pool,
            delayed_result,
            argument,
            PRIORITY_MEDIUM);

        if (futures[submitted] == NULL)
        {
            free(argument); /* Ownership transfers only on success. */
            break;
        }
    }

    printf("Submitted %zu task(s); press Ctrl-C now to test graceful draining.\n",
           submitted);

    for (size_t i = 0; i < submitted; i++)
    {
        int value = future_get(futures[i]);
        printf("Future %zu completed with result %d\n", i + 1, value);
    }

    if (signal_thread_started)
    {
        /* Wake sigwait for normal exit; SIGINT instead initiates shutdown. */
        (void)pthread_kill(signal_thread, SIGUSR1);
        (void)pthread_join(signal_thread, NULL);
        signal_thread_started = 0;
    }

    if (!atomic_load(&control.interrupted))
        taskforge_shutdown(&pool);

    for (size_t i = 0; i < submitted; i++)
    {
        future_destroy(futures[i]);
        free(futures[i]);
    }

    if (submitted == TASK_COUNT)
    {
        puts("Module 6: signal-aware draining completed.");
        result = EXIT_SUCCESS;
    }

    /* The demo is exiting; no thread remains to consume these blocked signals. */
    (void)pthread_sigmask(SIG_SETMASK, &original_mask, NULL);
    return result;
}

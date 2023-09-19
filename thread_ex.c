#include "csapp.h"

void *thread_function(void *arg)
{
    int *value = (int *)arg;
    printf("Thread received: %d\n", *value);
    int *result = malloc(sizeof(int));
    *result = *value * 2;
    Pthread_exit(result);
}

int main() {
    pthread_t thread;
    int value = 42;

    Pthread_create(&thread, NULL, thread_function, (void*)&value);

    void *thread_result;
    Pthread_join(thread, &thread_result);

    int *result = (int *)thread_result;
    printf("Thread result: %d\n", *result);

    free(result);
    return 0;
}
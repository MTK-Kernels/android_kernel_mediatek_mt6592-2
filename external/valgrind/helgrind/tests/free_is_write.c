
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

static char* s_mem;

static void* thread_func(void* arg)
{
    sleep(1);
    free(s_mem);
    return NULL;
}

int main(int argc, char** argv)
{
    pthread_t tid;
    int quiet;

    fprintf(stderr, "Start.\n");

    quiet = argc > 1;

    s_mem = malloc(10);
    if (0 && !quiet)
        fprintf(stderr, "Pointer to allocated memory: %p\n", s_mem);
    assert(s_mem);
    pthread_create(&tid, NULL, thread_func, NULL);

    char c = s_mem[5];
    __asm__ __volatile__("" : : "r"((long)c) );

    pthread_join(tid, NULL);
    fprintf(stderr, "Done.\n");
    return 0;
}

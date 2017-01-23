#include <pthread.h>
#include <alloca.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h> 

void *
nop(void *nil)
{
    return NULL;
}

void
__yell(void)
{
    char buf[256];
    memset(buf, 0, sizeof(buf));
}

int main(int argc, char **argv, char** envp)
{
    pthread_t thr[50];
    int i, err;

    for (i = 0; i < sizeof(thr) / sizeof(*thr); i++) {
        err = pthread_create(&thr[i], NULL, nop, NULL);
        assert(!err);
    }

    alloca(4096);
    __yell();

    for (i = 0; i < sizeof(thr) / sizeof(*thr); i++)
        pthread_join(thr[i], NULL);

    if ( argc == 2 && atoi(argv[1]) > 0) {
       
       char** new_env;
       char more_env[100];
       char n[10];
       int j;

       sprintf(more_env, "N%d=ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ",  atoi(argv[1]));
       for (j = 0; envp[j]; j++)
          ;
       new_env = malloc((j+2) * sizeof(char*));
       assert (new_env != NULL);
       for (i = 0; i < j; i++)
          new_env[i] = envp[i];
       new_env[i++] = more_env;
       new_env[i++] = NULL;
       assert(i == j+2);
       sprintf (n, "%d",  atoi(argv[1]) - 1);
       
       execle(argv[0], argv[0], n, NULL, new_env);
       assert(0);
    } else
       return 0;
}

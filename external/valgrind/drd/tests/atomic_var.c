
#define _GNU_SOURCE

#include <pthread.h>
#include <stdio.h>   
#include <stdlib.h>  
#include "../../config.h"

#ifndef HAVE_BUILTIN_ATOMIC
#error Sorry, but this test program can only be compiled by a compiler that\
has built-in functions for atomic memory access.
#endif

static __inline__
int sync_add_and_fetch(int* p, int i)
{
  return __sync_add_and_fetch(p, i);
}

static int s_x = 0;
char g_dummy[512];
static int s_y = 0;

static void* thread_func_1(void* arg)
{
  s_y = 1;
  (void) sync_add_and_fetch(&s_x, 1);
  return 0;
}

static void* thread_func_2(void* arg)
{
  while (sync_add_and_fetch(&s_x, 0) == 0)
    ;
  fprintf(stderr, "y = %d\n", s_y);
  return 0;
}

int main(int argc, char** argv)
{
  int i;
  const int n_threads = 2;
  pthread_t tid[n_threads];

  fprintf(stderr, "Start of test.\n");
  pthread_create(&tid[0], 0, thread_func_1, 0);
  pthread_create(&tid[1], 0, thread_func_2, 0);
  for (i = 0; i < n_threads; i++)
    pthread_join(tid[i], 0);
  fprintf(stderr, "Test finished.\n");

  return 0;
}

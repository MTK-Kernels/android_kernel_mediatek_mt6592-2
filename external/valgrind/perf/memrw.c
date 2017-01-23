#define _GNU_SOURCE
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>


static int sz_b; 
static int nr_b; 
static int nr_b_ws; 
static int nr_loops; 
static int nr_thr; 
static int nr_repeat; 


static int verbose = 0;
static unsigned char **t_b; 

static void *memrw_fn(void *v)
{
   int loops, m, b;
   int dowrite;
   int differs = 0;
   unsigned char prev = 0;

   for (loops = 0; loops < nr_loops; loops++) {
      
      
      
      
      for (m = 0; m < nr_b_ws; m++) {
         for (b = 0; b < sz_b; b++) {
            dowrite = b % 5 == 0;
            
            if (dowrite) {
               if (t_b[m][b] < 255)
                  t_b[m][b] += differs;
               else
                  t_b[m][b] = 0;
            } else {
               differs = t_b[m][b] != prev;
               prev = t_b[m][b];
            }
         }
      }
   }
   return NULL;
}

int main (int argc, char *argv[])
{
   int a;
   int ret;
   int i;
   int r;
   pthread_t thr;

   
   
   
   
   
   
   sz_b = 1024 * 1024;
   nr_b = 10;
   nr_b_ws = 10;
   nr_loops = 3;
   nr_repeat = 1;
   verbose = 0;
   for (a = 1; a < argc; a+=2) {
      if        (strcmp(argv[a], "-b") == 0) {
         sz_b = atoi(argv[a+1]);
      } else if (strcmp(argv[a], "-t") == 0) {
         nr_b = atoi(argv[a+1]);
      } else if (strcmp(argv[a], "-w") == 0) {
         nr_b_ws = atoi(argv[a+1]);
      } else if (strcmp(argv[a], "-l") == 0) {
         nr_loops = atoi(argv[a+1]);
      } else if (strcmp(argv[a], "-r") == 0) {
         nr_repeat = atoi(argv[a+1]);
      } else if (strcmp(argv[a], "-v") == 0) {
         verbose = atoi(argv[a+1]);
      } else {
         printf("unknown arg %s\n", argv[a]);
      }
   }
   if (nr_b_ws > nr_b)
      nr_b_ws = nr_b; 

   nr_thr = 1;

   printf ("total program memory -t %llu MB"
           " working set -w %llu MB\n",
           ((unsigned long long)nr_b * sz_b) 
             / (unsigned long long) (1024*1024),
           ((unsigned long long)nr_b_ws * sz_b) 
             / (unsigned long long)(1024*1024));
   printf (" working set R or W -l %d times"
           " repeat the whole stuff -r %d times\n",
           nr_loops,
           nr_repeat);

   for (r = 0; r < nr_repeat; r++) {
      printf ("creating and initialising the total program memory\n");
      t_b = malloc(nr_b * sizeof(char*));
      if (t_b == NULL)
         perror("malloc t_b");
      for (i = 0; i < nr_b; i++) {
         t_b[i] = calloc(sz_b, 1);
         if (t_b[i] == NULL)
            perror("malloc t_b[i]");
      }
      
      printf("starting thread that will read or write the working set\n");
      ret = pthread_create(&thr, NULL, memrw_fn, &nr_thr);
      if (ret != 0)
         perror("pthread_create");
      printf("waiting for thread termination\n");
      
      ret = pthread_join(thr, NULL);
      if (ret != 0)
         perror("pthread_join");
      printf("thread terminated\n");

      
      for (i = 0; i < nr_b; i++)
         free (t_b[i]);
      free (t_b);
      printf("memory freed\n");
   }

   return 0;
}

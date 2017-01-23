#include <config.h>
#include <pthread.h>
#include <stdio.h>

#ifdef HAVE_TLS

static int only_touch_stackvar;

static __thread int local;
__thread int global;

static __thread int badly_shared_local;

pthread_mutex_t protect_ptr_to_badly_shared_local = PTHREAD_MUTEX_INITIALIZER;
int *ptr_to_badly_shared_local;

static void local_false_positive(void)
{
   local = local + 1; 
}

static void global_false_positive(void)
{
   global = global + 1; 
}

static void badly_shared_local_error_expected(void)
{
   *ptr_to_badly_shared_local = *ptr_to_badly_shared_local + 1; 
   
}

static void *level2(void *p)
{
   int stackvar = 0;

   stackvar = stackvar + only_touch_stackvar;
   
   local_false_positive();
   global_false_positive();
   if (only_touch_stackvar != 0) {
      badly_shared_local_error_expected();
   }
   return 0;
}

#define NLEVEL2 10
static void *level1(void *p)
{
   pthread_t threads[NLEVEL2];
   int curthread = 0;
   int i;

   pthread_mutex_lock(&protect_ptr_to_badly_shared_local);
   if (ptr_to_badly_shared_local == NULL)
      ptr_to_badly_shared_local = &badly_shared_local;
   pthread_mutex_unlock(&protect_ptr_to_badly_shared_local);

   for(i = 0; i < NLEVEL2; i++) {
      pthread_create(&threads[curthread++], NULL, level2, NULL);
   }

   for(i = 0; i < curthread; i++)
      pthread_join(threads[i], NULL);

   return 0;
}

#define NLEVEL1 3
int main(int argc, char*argv[])
{
   pthread_t threads[NLEVEL1];
   int curthread = 0;
   int i;

   only_touch_stackvar = argc > 1;

   for(i = 0; i < NLEVEL1; i++) {
      pthread_create(&threads[curthread++], NULL, level1, NULL);
   }

   fprintf(stderr, "starting join in main\n");
   fflush(stderr);
   for(i = 0; i < curthread; i++)
      pthread_join(threads[i], NULL);
   fprintf(stderr, "finished join in main\n");
   fflush(stderr);
   return 0;
}
#else
int main()
{
   fprintf(stderr, "starting join in main\n");
   fflush(stderr);
   
   fprintf(stderr, "finished join in main\n");
   fflush(stderr);
   return 0;
}
#endif

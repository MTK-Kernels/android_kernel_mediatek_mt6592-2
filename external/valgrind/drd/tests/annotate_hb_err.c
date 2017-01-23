

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "unified_annotations.h"


int main(int argc, char** argv)
{
  pthread_mutex_t m;
  pthread_cond_t  cv;
  int i[64];

  pthread_mutex_init(&m, NULL);
  pthread_cond_init(&cv, NULL);

  
  U_ANNOTATE_HAPPENS_AFTER(&i);

  
  U_ANNOTATE_HAPPENS_BEFORE(&m);

  
  U_ANNOTATE_HAPPENS_BEFORE(&cv);

  
  U_ANNOTATE_HAPPENS_BEFORE(&i);
  pthread_cond_init((pthread_cond_t*)&i, NULL);

  
  U_ANNOTATE_NEW_MEMORY(&i, sizeof(i));
  U_ANNOTATE_HAPPENS_BEFORE(&i);
  U_ANNOTATE_HAPPENS_AFTER(&i);
  U_ANNOTATE_NEW_MEMORY(&i, sizeof(i));
  U_ANNOTATE_HAPPENS_BEFORE(&i);
  U_ANNOTATE_NEW_MEMORY(&i, sizeof(i));

  
  U_ANNOTATE_HAPPENS_BEFORE(&i);
  U_ANNOTATE_HAPPENS_AFTER(&i);
  U_ANNOTATE_HAPPENS_BEFORE(&i);

  fprintf(stderr, "Done.\n");
  return 0;
}

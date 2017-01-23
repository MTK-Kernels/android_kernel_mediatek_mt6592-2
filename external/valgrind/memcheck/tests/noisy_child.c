
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

void do_child_badness ( char* p )
{
   
   free(p);
}

void do_parent_badness ( char* p )
{
   
   p[10] = 42;
}


int main ( void )
{
  pid_t child;
  char* p = malloc(10); assert(p);
  free(p);

  
  p[5] = 22;

  child = fork();
  assert(child != -1); 

  if (child == 0) {
     
     do_child_badness(p);
  } else {
     
     do_parent_badness(p);
  }

  return 0;

}

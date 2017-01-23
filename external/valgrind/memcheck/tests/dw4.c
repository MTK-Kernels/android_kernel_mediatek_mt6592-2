


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tests/sys_mman.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "memcheck/memcheck.h"

void croak ( void* aV )
{
  if(VALGRIND_CHECK_MEM_IS_ADDRESSABLE(aV,1) != 0)
     return;
  char* a = (char*)aV;
  char* undefp = malloc(1);
  char saved = *a;
  assert(undefp);
  *a = *undefp;
  (void) VALGRIND_CHECK_MEM_IS_DEFINED(a, 1);
  *a = saved;
  free(undefp);
}

struct s1
{
  char c;
  short s;
  int i;
  long l;
  float f;
  double d;
};

struct s1 S2[30];

int main ( void )
{
  struct s1 local;
  struct s1* onheap = malloc(sizeof (struct s1));
  void *p, *q;
  int fd;
  int n;
  char filename[256];

  assert(onheap);
  croak(&onheap->i);

  croak( &S2[0].i );
  croak( &local.i );

  
  p = mmap( 0, 16 * 1024, PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
  assert(p != MAP_FAILED);
  croak( p);

  
  snprintf(filename, sizeof(filename), "./valgrind-dw4-test.%d",
           getpid());

  unlink(filename);

  fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  assert (fd > 0);
  n = write(fd, filename, strlen(filename));
  assert (n > 8);
  q = mmap(NULL, 100, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert (q != MAP_FAILED);
  croak( q);
  unlink(filename);

  
  void *addr = sbrk(0);
  croak(addr); 
  sbrk(4 * 1024); 
  croak(addr); 
  addr = (void *) ((char*)addr + 2 * 1024);
  croak(addr); 
  sbrk(-3*1024);
  croak(addr); 
  
  return 0;
}

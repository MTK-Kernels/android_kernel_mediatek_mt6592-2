


#include <stdio.h>

__attribute__((noinline)) void foo ( long* sa, int n )
{
  int i;
  for (i = 0; i < n; i++)
    sa[i] = 0;
}

__attribute__((noinline)) void bar ( long* sa, int n )
{
   foo(sa, n);
}

int main ( void )
{
  int i;
  long beforea[3];
  long a[7];
  long aftera[3];
  bar(a, 7+1);     
  bar(a, 7+0);     
  for (i = 0; i < 7+1; i++) {
     a[i] = 0;
  }
 {char beforebuf[8];
  char buf[8];
  char afterbuf[8];
  sprintf(buf, "%d", 123456789);
  return 1 & ((a[4] + beforea[1] + aftera[1] + beforebuf[1] 
                    + buf[2] + afterbuf[3]) / 100000) ;
 }
}

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "valgrind.h"

typedef 
   struct _Lard {
      struct _Lard* next; 
      char stuff[999]; 
   }
   Lard;
Lard* lard = NULL;
static int ctr = 0;

void addMoreLard ( void )
{
   Lard* p;
   ctr++;
   if ((ctr % 3) == 1) {
      p = malloc(sizeof(Lard));
      p->next = lard;
      lard = p;
   }
}
static void post ( char* s, int n, int r );
static void pre ( char* s, int n );
static int fact1 ( int n );
static int fact2 ( int n );

__attribute__((noinline))
int mul ( int x, int y ) { return x * y; }

int fact1 ( int n )
{
   addMoreLard();
   if (n == 0) return 1; else return mul(n, fact2(n-1));
}
int fact2 ( int n )
{
   addMoreLard();
   if (n == 0) return 1; else return mul(n, fact1(n-1));
}


int I_WRAP_SONAME_FNNAME_ZU(NONE,fact1) ( int n )
{
   int    r;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   pre("wrapper1", n);
   addMoreLard();
   CALL_FN_W_W(r, fn, n);
   addMoreLard();
   post("wrapper1", n, r);
   if (n >= 3) r += fact2(2);
   return r;
}

int I_WRAP_SONAME_FNNAME_ZU(NONE,fact2) ( int n )
{
   int    r;
   OrigFn fn;
   VALGRIND_GET_ORIG_FN(fn);
   pre("wrapper2", n);
   addMoreLard();
   CALL_FN_W_W(r, fn, n);
   addMoreLard();
   post("wrapper2", n, r);
   return r;
}


int main ( void )
{
   int r, n = 15; 
   Lard *p, *p_next;
   printf("computing fact1(%d)\n", n); fflush(stdout);
   r = fact1(n);
   printf("fact1(%d) = %d\n", n, r); fflush(stdout);

   printf("allocated %d Lards\n", ctr); fflush(stdout);
   for (p = lard; p; p = p_next) {
      p_next = p->next;
      free(p);
   }

   return 0;
}

static void send ( char* s )
{
  while (*s) {
    write(1, s, 1);
    s++;
  }
}

static void pre ( char* s, int n )
{
  char buf[50];
  fflush(stdout);
  sprintf(buf,"%d", n);
  send("in ");
  send(s);
  send("-pre:  fact(");
  send(buf);
  send(")\n");
  fflush(stdout);
}

static void post ( char* s, int n, int r )
{
  char buf[50];
  fflush(stdout);
  sprintf(buf,"%d", n);
  send("in ");
  send(s);
  send("-post: fact(");
  send(buf);
  send(") = ");
  sprintf(buf,"%d", r);
  send(buf);
  send("\n");
  fflush(stdout);
}

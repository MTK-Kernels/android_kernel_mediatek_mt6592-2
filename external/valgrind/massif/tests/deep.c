
#include <stdlib.h>

void a12(int n) { malloc(n); }
void a11(int n) { a12(n); }
void a10(int n) { a11(n); }
void a9 (int n) { a10(n); }
void a8 (int n) { a9 (n); }
void a7 (int n) { a8 (n); }
void a6 (int n) { a7 (n); }
void a5 (int n) { a6 (n); }
void a4 (int n) { a5 (n); }
void a3 (int n) { a4 (n); }
void a2 (int n) { a3 (n); }
void a1 (int n) { a2 (n); }

int main(void)
{
   int i;

   
   for (i = 0; i < 10; i++)
      a1(400);    

   return 0;
}

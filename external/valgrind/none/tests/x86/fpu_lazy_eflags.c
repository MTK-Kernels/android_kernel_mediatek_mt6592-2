
#include <stdio.h>

int main(int argc, char **argv)
{
   union {
      float a[2];
      int b[2];
   } u;

   u.a[0] = 0.0 / 0.0;
   u.a[1] = ((*u.b & 0x7FC00000) != 0x7FC00000);
   printf("%f\n", u.a[1]);

   return 0;
}


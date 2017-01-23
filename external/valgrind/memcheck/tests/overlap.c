#include <string.h>
#include <stdio.h>

char b[50];

void reset_b(void)
{
   int i;

   for (i = 0; i < 50; i++)
      b[i] = '_';
   b[49] = '\0';
}

void reset_b2(void)
{
   reset_b();
   strcpy(b, "ABCDEFG");
}

int main(void)
{
   char x[100];
   char a[] = "abcdefghijklmnopqrstuvwxyz";
   int  i;

   

   for (i = 0; i < 50; i++) {
      x[i] = i+1;    
   }
   for (i = 50; i < 100; i++) {
      
      
      x[i] = 0;  
               
   }

   memcpy(x+20, x, 20);    
   memcpy(x+20, x, 21);    
   memcpy(x, x+20, 20);    
   memcpy(x, x+20, 21);    

   strncpy(x+20, x, 20);    
   strncpy(x+20, x, 21);    
   strncpy(x, x+20, 20);    
   strncpy(x, x+20, 21);    
   
   x[39] = '\0';
   strcpy(x, x+20);    

   x[39] = 39;
   x[40] = '\0';
   strcpy(x, x+20);    

   x[19] = '\0';
   strcpy(x+20, x);    


   

   reset_b();
   printf("`%s'\n", b);

   strcpy(b, a);
   printf("`%s'\n", b);
   
   reset_b();
   strncpy(b, a, 25);
   printf("`%s'\n", b);

   reset_b();
   strncpy(b, a, 26);
   printf("`%s'\n", b);

   reset_b();
   strncpy(b, a, 27);
   printf("`%s'\n", b);

   printf("\n");

   

   reset_b2();
   printf("`%s'\n", b);
   
   reset_b2();
   strcat(b, a);
   printf("`%s'\n", b);
   
   reset_b2();
   strncat(b, a, 25);
   printf("`%s'\n", b);
   
   reset_b2();
   strncat(b, a, 26);
   printf("`%s'\n", b);
   
   reset_b2();
   strncat(b, a, 27);
   printf("`%s'\n", b);


   for ( i = 0; i < 2; i++) 
      strncat(a+20, a, 21);    
   strncat(a, a+20, 21);

   {
      char dest[64];
      char src [16];
      strcpy( src, "short" );
      strncpy( dest, src, 20 );
   }

   return 0;
}

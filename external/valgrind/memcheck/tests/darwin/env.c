#include <assert.h>
#include <stdlib.h>
#include <string.h>


int main(void)
{
   char* val1 = "x";
   char* val2 = "xx";
   char* val3 = "xxx";

   setenv("MYVAR", val1, 0); 
   assert( 0 == strcmp(getenv("MYVAR"), val1) );

   setenv("MYVAR", val2, 1); 
   assert( 0 == strcmp(getenv("MYVAR"), val2) );

   setenv("MYVAR", val3, 0); 
   assert( 0 == strcmp(getenv("MYVAR"), val2) );

   putenv("MYVAR=xxxx");                  
   assert( 0 == strcmp(getenv("MYVAR"), "xxxx") );

   unsetenv("MYVAR");
   assert( NULL == getenv("MYVAR") );

   return 0;
}


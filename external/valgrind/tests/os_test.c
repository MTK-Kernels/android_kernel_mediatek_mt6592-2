
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#define False  0
#define True   1
typedef int    Bool;

char* all_OSes[] = {
   "linux",
   "darwin",
   NULL
};

#if defined(VGO_linux)
static Bool matches_version(char *min_version)
{
   int a1, a2, a3, g1, g2, g3;  

   if (min_version == NULL)  return True;  

   
   FILE *fp = fopen("/proc/sys/kernel/osrelease", "r");
   if (fp == NULL || fscanf(fp, "%d.%d.%d", &a1, &a2, &a3) != 3) return False;
   fclose(fp);

   
   if (sscanf(min_version, "%d.%d.%d", &g1, &g2, &g3) != 3) return False;


   if (a1 > g1) return True;
   if (a1 < g1) return False;
   if (a2 > g2) return True;
   if (a2 < g2) return False;
   if (a3 >= g3) return True;

   return False;
}
#endif

static Bool go(char* OS, char *min_version)
{ 
#if defined(VGO_linux)
   if ( 0 == strcmp( OS, "linux" ) && matches_version( min_version )) return True;

#elif defined(VGO_darwin)
   if ( 0 == strcmp( OS, "darwin" ) ) return True;

#else
#  error Unknown OS
#endif   

   return False;
}

int main(int argc, char **argv)
{
   int i;
   if ( argc < 2 ) {
      fprintf( stderr, "usage: os_test <OS-type> [<min-version>]\n" );
      exit(3);             
   }
   if (go( argv[1], argv[2] )) {
      return 0;            
   }
   for (i = 0; NULL != all_OSes[i]; i++) {
      if ( 0 == strcmp( argv[1], all_OSes[i] ) )
         return 1;         
   }
   return 2;               
}


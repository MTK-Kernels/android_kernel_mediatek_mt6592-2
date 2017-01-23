#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define FEATURE_PRESENT       0
#define FEATURE_NOT_PRESENT   1
#define UNRECOGNISED_FEATURE  2
#define USAGE_ERROR           3

#if defined(VGA_mips32) || defined(VGA_mips64)
static int mipsCPUInfo(const char *search_string) {
   const char *file_name = "/proc/cpuinfo";

   char cpuinfo_line[256];

   FILE *f = NULL;
   if ((f = fopen (file_name, "r")) == NULL)
     return 0;

   while (fgets (cpuinfo_line, sizeof (cpuinfo_line), f) != NULL)
   {
     if (strstr (cpuinfo_line, search_string) != NULL)
     {
         fclose (f);
         return 1;
     }
   }

   fclose (f);

   
   return 0;
}

static int go(char *feature)
{
   int cpuinfo;
   if (strcmp(feature, "fpu") == 0) {
#if defined(__mips_hard_float)
      return FEATURE_PRESENT;
#else
      return FEATURE_NOT_PRESENT;
#endif
   }
   else if (strcmp(feature, "mips32-dsp") == 0) {
      const char *dsp = "dsp";
      cpuinfo = mipsCPUInfo(dsp);
      if (cpuinfo == 1) {
         return FEATURE_PRESENT;
      } else{
         return FEATURE_NOT_PRESENT;
      }
   } else if (strcmp(feature, "mips32-dspr2") == 0) {
      const char *dsp2 = "dsp2";
      cpuinfo = mipsCPUInfo(dsp2);
      if (cpuinfo == 1) {
         return FEATURE_PRESENT;
      } else{
         return FEATURE_NOT_PRESENT;
      }
   } else if (strcmp(feature, "cavium-octeon") == 0) {
      const char *cavium = "Cavium Octeon";
      cpuinfo = mipsCPUInfo(cavium);
      if (cpuinfo == 1) {
         return FEATURE_PRESENT;
      } else{
         return FEATURE_NOT_PRESENT;
      }
   } else if (strcmp(feature, "cavium-octeon2") == 0) {
      const char *cavium2 = "Cavium Octeon II";
      cpuinfo = mipsCPUInfo(cavium2);
      if (cpuinfo == 1) {
         return FEATURE_PRESENT;
      } else{
         return FEATURE_NOT_PRESENT;
      }
   } else if (strcmp(feature, "mips-be") == 0) {
#if defined (_MIPSEB)
     return FEATURE_PRESENT;
#else
     return FEATURE_NOT_PRESENT;
#endif
   } else {
      return UNRECOGNISED_FEATURE;
   }

}

#else

static int go(char *feature)
{
   
   return UNRECOGNISED_FEATURE;
}

#endif


int main(int argc, char **argv)
{
   if (argc != 2) {
      fprintf( stderr, "usage: mips_features <feature>\n" );
      exit(USAGE_ERROR);
   }
   return go(argv[1]);
}

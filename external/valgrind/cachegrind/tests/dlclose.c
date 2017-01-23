
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

int main(int argc, char **argv) {
   void *handle;
   void (*myprint)(void);
   char *error;

   handle = dlopen ("./myprint.so", RTLD_LAZY);
   if (!handle) {
       fputs (dlerror(), stderr);
       exit(1);
   }

   myprint = dlsym(handle, "myprint");
   if ((error = dlerror()) != NULL)  {
       fprintf (stderr, "%s\n", error);
       exit(1);
   }

   (*myprint)();

   
   dlclose(handle);

   return 0;
}


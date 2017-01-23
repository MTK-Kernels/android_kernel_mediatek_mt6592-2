
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>



int main ( void )
{
  int i, r, sum = 0;
  char* im_a_global_array;
  void* hdl = dlopen("./preen_invars_so.so", RTLD_NOW);
  assert(hdl);
  im_a_global_array = dlsym(hdl, "im_a_global_array");
  assert(im_a_global_array);
  

  for (i = 10; i >= 0; i--) {
     sum += im_a_global_array[i];
  }


  if (sum & 1) printf("%s bar %d\n", "foo", sum & 1); else
               printf("foo %s %d\n", "bar", 1 - (sum & 1));

  r = dlclose(hdl);
  assert(r == 0);

  return 0;
}

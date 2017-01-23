
#include <assert.h>
#include <stdlib.h>

int main()
{
  int i;
  void* p;

  for (i = 0; i < 100000; i++)
    free(malloc(40960));

  for (i = 0; i < 100000; i++)
  {
    p = realloc(NULL, 40960);
    p = realloc(p, 50000);
    p = realloc(p, 40000);
    p = realloc(p, 0);
#if defined(VGO_darwin)
    if (p)
      free(p);
#else
    assert(! p);
#endif
  }

  return 0;
}

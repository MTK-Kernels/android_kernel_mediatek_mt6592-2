// Uses various wchar_t * functions that have hand written SSE assembly

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

int main(int argc, char **argv)
{
  wchar_t a[] = L"The spazzy orange tiger jumped over the tawny jaguar.";
  wchar_t *b, *c;
  wchar_t *d, *e;

  size_t l = wcslen (a);
  fprintf (stderr, "wcslen: %zd\n", l); 

  b = (wchar_t *) malloc((l + 1) * sizeof (wchar_t));
  c = wcscpy (b, a);

  fprintf (stderr, "wcscmp equal: %d\n", wcscmp (a, b)); 

  d = wcsrchr (a, L'd');
  e = wcschr (a, L'd');

  fprintf (stderr, "wcsrchr == wcschr: %d\n", d == e); 

  free (c); 
  return 0;
}

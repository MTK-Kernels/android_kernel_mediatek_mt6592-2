#include <stdio.h>
#include <stdlib.h>

#define _GNU_SOURCE
#include <string.h>

int main(int argc, char **argv)
{
  char a[] = "The spazzy orange tiger jumped over the tawny jaguar.";
  char *b, *c;
  char *d, *e;

  size_t l = strlen (a);
  fprintf (stderr, "strlen: %zd\n", l); 

  b = (char *) malloc((l + 3)); 
  b[l] = 'X';
  b[l + 1] = 'X';
  b[l + 2] = 'X';
  c = stpncpy (b, a, l + 3);

  fprintf (stderr, "equal: %d\n", strcmp (a, b)); 
  fprintf (stderr, "retlen: %zd\n", c - b); 
  fprintf (stderr, "last: '%c'\n", *(c - 1)); 
  fprintf (stderr, "zero0: %d\n", *c); 
  fprintf (stderr, "zero1: %d\n", *(c + 1)); 
  fprintf (stderr, "zero2: %d\n", *(c + 2)); 

  d = (char *) malloc (l - 1); 
  e = stpncpy (d, b, l - 1);

  fprintf (stderr, "equal: %d\n", strncmp (b, d, l - 1)); 
  fprintf (stderr, "retlen: %zd\n", e - d); 
  fprintf (stderr, "last: '%c'\n", *(e - 1)); 

  free (b);
  free (d);
  return 0;
}

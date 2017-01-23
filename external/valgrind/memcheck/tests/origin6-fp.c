

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../memcheck.h"


double** alloc_square_array ( int nArr )
{
  int i;
  double** vec;
  assert(nArr >= 1);
  vec = malloc(nArr * sizeof(double*));
  assert(vec);
  for (i = 0; i < nArr; i++) {
    vec[i] = malloc(nArr * sizeof(double));
    assert(vec);
  }
  return vec;
}

double** do3x3smooth ( double** arr, int nArr )
{
  int i, j;
  double** out;
  assert(nArr >= 3);
  out = alloc_square_array(nArr - 2);
  assert(out);
  for (i = 1; i < nArr-1; i++) {
    for (j = 1; j < nArr-1; j++) {
      double s =   arr[i-1][j-1] + arr[i-1][j  ] + arr[i-1][j+1]
                 + arr[i  ][j-1] + arr[i  ][j  ] + arr[i  ][j+1]
                 + arr[i+1][j-1] + arr[i+1][j  ] + arr[i+1][j+1];
      out[i-1][j-1] = s / 9.0;
    }
  }
  return out;
}

double sum ( double** arr, int nArr )
{
  int i, j;
  double s = 0.0;
  assert(nArr >= 1);
  for (i = 0; i < nArr; i++) {
    for (j = 0; j < nArr; j++) {
      s += arr[i][j];
    }
  }
  return s;
}

void setup_arr ( double** arr, int nArr )
{
  int i, j;
  assert(nArr >= 1);
  for (i = 0; i < nArr; i++) {
    for (j = 0; j < nArr; j++) {
      arr[i][j] = (double)(i * j);
      if (i == nArr/2 && j == nArr/2) {
         unsigned char* p = (unsigned char*)&arr[i][j];
         (void) VALGRIND_MAKE_MEM_UNDEFINED(p, 1);
      }
    }
  }
}

int main ( void )
{
  int nArr = 2300;
  int ri;
  double r, **arr, **arr2, **arr3;
  arr = alloc_square_array(nArr);
  setup_arr( arr, nArr );
  arr2 = do3x3smooth( arr, nArr );
  arr3 = do3x3smooth( arr2, nArr-2 );
  r = sum( arr3, nArr-4 );
  if (0) fprintf(stderr, "r = %g\n", r );
  r /= 10000.0;
  ri = (int)r;
  if (0) fprintf(stderr, "ri = %d\n", ri);
  if (ri == 696565111) {
     fprintf(stderr, "Test succeeded.\n");
  } else {
     fprintf(stderr, "Test FAILED !\n");
     assert(0);
  }
  return 0;
}









#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define NNN 1000


double
my_mean1 (const double data[], size_t stride, const size_t size)
{
  long double mean = 0;
  size_t i;

  for (i = 0; i < size; i++)
    {
      mean += (data[i * stride] - mean) / (i + 1);
    }
  return mean;
}

double
my_mean2 (const double data[], size_t stride, const size_t size)
{
  int i;
  long double sum = 0;
  for (i = 0; i < size; i++)
    sum += data[i * stride];
  return sum / (double)size;
}


int main (void)
{
  int i;
  const size_t nacc2 = NNN+1;
  double numacc2[NNN+1] ;

  numacc2[0] = 1.2 ;
 
  for (i = 1 ; i < NNN; i += 2) 
     numacc2[i] = 1.1 ;

  for (i = 1 ; i < NNN; i += 2) 
      numacc2[i+1] = 1.3 ;

#if 1
  asm __volatile__("fninit");
#endif

  {
    double m1 = my_mean1 (numacc2, 1, nacc2);
    double m2 = my_mean2 (numacc2, 1, nacc2);
    double expected_mean = 1.2;
    printf("m1 = %19.17f,  exp = %19.17f\n", m1, expected_mean);
    printf("m2 = %19.17f,  exp = %19.17f\n", m2, expected_mean);
  }

  return 0;
}



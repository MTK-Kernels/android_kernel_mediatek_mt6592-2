#include <stdio.h>

int main (void) {
#ifdef HAS_ISA_2_07
   __builtin_tabort (0);
   __builtin_tabortdc (0,0,0);
   __builtin_tabortdci (0,0,0);
   __builtin_tabortwc (0,0,0);
   __builtin_tabortwci (0,0,0);
   __builtin_tbegin (0);
   __builtin_tend (0);
   
   __builtin_trechkpt ();  
   __builtin_treclaim (0); 
   __builtin_tsr (0);
#endif
   return 0;
}

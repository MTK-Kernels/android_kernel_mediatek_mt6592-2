#ifndef _FDLEAK_H_
#define _FDLEAK_H_

#include <stdlib.h>
#include <stdio.h>

#define DO(op) \
   ({ \
      long res = op; \
      if (res < 0) { \
         perror(#op); \
         exit(1); \
      }; \
      res; \
   })

#define CLOSE_INHERITED_FDS { int i; for (i = 3; i < 64; i++) close(i); }

#endif 

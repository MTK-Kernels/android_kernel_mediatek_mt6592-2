#include "../../../include/vki/vki-scnums-x86-linux.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/mman.h>

extern long int syscall (long int __sysno, ...) __THROW;


#define GO(__NR_xxx, s) \
   fprintf(stderr, "-----------------------------------------------------\n"  \
                   "%3d:%20s %s\n"                                            \
                   "-----------------------------------------------------\n", \
                   __NR_xxx, #__NR_xxx, s);

#define SY  res = syscall

#define FAIL  assert(-1 == res);
#define SUCC  assert(-1 != res);
#define SUCC_OR_FAIL    

#define FAILx(E) \
   do { \
      int myerrno = errno; \
      if (-1 == res) { \
         if (E == myerrno) { \
             \
         } else { \
         fprintf(stderr, "Expected error %s (%d), got %d\n", #E, E, myerrno); \
         exit(1); \
         } \
      } else { \
         fprintf(stderr, "Expected error %s (%d), got success\n", #E, E); \
         exit(1); \
      } \
   } while (0);

#define SUCC_OR_FAILx(E) \
   do { \
      int myerrno = errno; \
      if (-1 == res) { \
         if (E == myerrno) { \
             \
         } else { \
         fprintf(stderr, "Expected error %s (%d), got %d\n", #E, E, myerrno); \
         exit(1); \
         } \
      } \
   } while (0);

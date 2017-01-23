#define ANDROID_PATCHES

#include <stddef.h>

extern char* __cxa_demangle(const char*, char*, size_t*, int*);

static inline char* cplus_demangle(const char* c, int i) {
  return __cxa_demangle(c, 0, 0, 0);
}

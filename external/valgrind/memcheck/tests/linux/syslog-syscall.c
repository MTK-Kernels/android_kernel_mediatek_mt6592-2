
#include "../../config.h"
#include <stdio.h>
#if defined(HAVE_SYS_KLOG_H)
#include <sys/klog.h>
#endif

int main(int argc, char** argv)
{
  int number_of_unread_characters;
#if defined HAVE_KLOGCTL
  number_of_unread_characters = klogctl(9, 0, 0);
#endif
  fprintf(stderr, "Done.\n");
  return 0 * number_of_unread_characters;
}

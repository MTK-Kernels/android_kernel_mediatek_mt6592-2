
#include <sys/ioctl.h>

int main(int argc, const char *argv[])
{
#ifdef TIOCSBRK
   ioctl(1, TIOCSBRK, 0);
   ioctl(1, TIOCCBRK, 0);
#endif

   return 0;
}

#include <unistd.h>

// would break and the cachegrind.out.<pid> file wouldn't get written.
int main(void)
{
   chdir("..");

   return 0;
}

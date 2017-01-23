#include <unistd.h>
#include "tests/sys_mman.h"
#include <stdio.h>
#include <stdlib.h>


int main()
{
    void* m;
    
    m = mmap(NULL, 100, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

    if (m == (void*)-1) {
       fprintf(stderr, "error mmapping\n");
       exit(1);
    }
    
    munmap(m, 100);

    return 0;
}

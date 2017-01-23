
#include <stdio.h>
#include "memcheck/memcheck.h"
#include "tests/sys_mman.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#if !defined(MAP_NORESERVE)
#  define MAP_NORESERVE 0
#endif

int main()
{
	char **volatile ptrs;
	int i;
	int fd;
	char *map;

        int ptrbits, stepbits, stepsize, nptrs;
	if (sizeof(void*) == 8) {
           
	   ptrbits = 32;   
           stepbits = 14+1;  
	   stepsize = (1 << stepbits);
	   nptrs = 1 << (ptrbits - stepbits);
	} else {
           
	   ptrbits = 32;
           stepbits = 14;
	   stepsize = (1 << stepbits);
	   nptrs = 1 << (ptrbits - stepbits);
	}

	ptrs = malloc(nptrs * sizeof(char *));
	for (i = 0; i < nptrs; i++)
		ptrs[i] = (char *)((long)i << stepbits);

	
        
	map = mmap(0, stepsize * 2, PROT_NONE, MAP_PRIVATE|MAP_NORESERVE|MAP_ANONYMOUS, -1, 0);
	if (map == (char *)-1)
		perror("trap 1 failed");

        
	map = mmap(0, stepsize * 2, PROT_WRITE, MAP_PRIVATE|MAP_NORESERVE|MAP_ANONYMOUS, -1, 0);
	if (map == (char *)-1)
		perror("trap 2 failed");

	
	fd = open("./pointer-trace-test-file", O_RDWR | O_CREAT | O_EXCL, 0600);
	unlink("./pointer-trace-test-file");
	map = mmap(0, stepsize * 2, PROT_WRITE|PROT_READ, MAP_PRIVATE, fd, 0);
	if (map == (char *)-1)
		perror("trap 3 failed");
	

        
	map = mmap(0, 256*1024, PROT_NONE, MAP_PRIVATE|MAP_NORESERVE|MAP_ANONYMOUS, -1, 0);
	if (map == (char *)-1)
		perror("trap 4 failed");
	else {
		munmap(map, 256*1024);
		(void)VALGRIND_MAKE_MEM_DEFINED(map, 256*1024); 
	}

	VALGRIND_DO_LEAK_CHECK;

	free(ptrs);

        
        
        ptrs = malloc(1000);

	return 0;
}

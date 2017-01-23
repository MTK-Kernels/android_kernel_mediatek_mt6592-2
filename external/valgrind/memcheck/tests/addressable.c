#include "../memcheck.h"
#include "tests/sys_mman.h"
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

static int pgsz;

static char *mm(char *addr, int size, int prot)
{
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;
	char *ret;

	if (addr)
		flags |= MAP_FIXED;

	ret = mmap(addr, size, prot, flags, -1, 0);
	if (ret == (char *)-1) {
		perror("mmap failed");
		exit(1);
	}

	return ret;
}

static void test1()
{
	char *m = mm(0, pgsz * 5, PROT_READ);

	(void) VALGRIND_CHECK_MEM_IS_DEFINED(m, pgsz*5); 
}

static void test2()
{
	char *m = mm(0, pgsz * 5, PROT_READ|PROT_WRITE);
	(void) VALGRIND_CHECK_MEM_IS_DEFINED(m, pgsz*5); 

	munmap(&m[pgsz*2], pgsz);

	(void) VALGRIND_CHECK_MEM_IS_DEFINED(&m[pgsz*2], pgsz); 

	
	m[pgsz*2] = 'x';	
}

static void test3()
{
	char *m = mm(0, pgsz * 5, PROT_READ|PROT_WRITE);

	(void) VALGRIND_MAKE_MEM_UNDEFINED(&m[pgsz], pgsz);
	mm(&m[pgsz], pgsz, PROT_READ);
	(void) VALGRIND_CHECK_MEM_IS_DEFINED(&m[pgsz], pgsz); 
}

static void test4()
{
	char *m = mm(0, pgsz * 5, PROT_READ|PROT_WRITE);

	mprotect(m, pgsz, PROT_WRITE);
	(void) VALGRIND_CHECK_MEM_IS_DEFINED(m, pgsz); 
	m[44] = 'y';		

	mprotect(m, pgsz*5, PROT_NONE);
	m[55] = 'x';		
}

static void test5()
{
	char *m = mm(0, pgsz * 5, PROT_READ|PROT_WRITE);
	
	(void) VALGRIND_MAKE_MEM_UNDEFINED(m, pgsz*5);
	memset(m, 'x', 10);
	(void) VALGRIND_CHECK_MEM_IS_DEFINED(m, 10);	
	(void) VALGRIND_CHECK_MEM_IS_DEFINED(m+10, 10); 

	mprotect(m, pgsz*5, PROT_NONE);
	mprotect(m, pgsz*5, PROT_READ);

	(void) VALGRIND_CHECK_MEM_IS_DEFINED(m, 10);	
	(void) VALGRIND_CHECK_MEM_IS_DEFINED(m+20, 10); 
}

static struct test {
	void (*test)(void);
	int faults;
} tests[] = {
	{ test1, 0 },
	{ test2, 1 },
	{ test3, 0 },
	{ test4, 1 },
	{ test5, 0 },
};
static const int n_tests = sizeof(tests)/sizeof(*tests);
	
int main()
{
	static const struct rlimit zero = { 0, 0 };
	int i;

	pgsz = getpagesize();
	setvbuf(stdout, NULL, _IOLBF, 0);

	setrlimit(RLIMIT_CORE, &zero);

	for(i = 0; i < n_tests; i++) {
		int pid;

		pid = fork();
		if (pid == -1) {
			perror("fork");
			exit(1);
		}
		if (pid == 0) {
			(*tests[i].test)();
			exit(0);
		} else {
			int status;
			int ret;
			
			printf("Test %d: ", i+1);
			fflush(stdout);

			while((ret = waitpid(pid, &status, 0)) != pid) {
				if (errno != EINTR) {
					perror("waitpid");
					exit(1);
				}
			}
			if (WIFSIGNALED(status)) {
				assert(WTERMSIG(status) != 0);

				if (1 == tests[i].faults &&
				    (WTERMSIG(status) == SIGSEGV ||
				     WTERMSIG(status) == SIGBUS))
					printf("PASS\n");
				else
					printf("died with unexpected signal %d\n", 
					       WTERMSIG(status));
			} else if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) == 0) {
					if (tests[i].faults == 0)
						printf("PASS\n");
					else
						printf("exited without expected SIGSEGV or SIGBUS signal\n");
				} else
					printf("exited with unexpected status %d\n",
					       WEXITSTATUS(status));
			} else {
				printf("strange status %x?\n", status);
			}
		}
	}
	exit(0);
}

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#include "../memcheck.h"

static void handler(int s)
{
}

int main()
{
	struct timespec req, rem;
	int ret;

	req.tv_sec = 2;
	req.tv_nsec = 0;

	signal(SIGALRM, handler);

	alarm(1);
	ret = nanosleep(&req, &rem);
	
	if (ret != -1 || errno != EINTR) {
		fprintf(stderr, "FAILED: expected nanosleep to be interrupted\n");
	} else {
		(void) VALGRIND_CHECK_VALUE_IS_DEFINED(rem);
		fprintf(stderr, "PASSED\n"); 
	}

	return 0;
}

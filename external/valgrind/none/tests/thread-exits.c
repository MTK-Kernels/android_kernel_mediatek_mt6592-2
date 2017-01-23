#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

static int grower;

static void handler(int sig)
{
}

static void *thr(void *v)
{
	return 0;
}

#define FRAME 4096

static void grow(int depth)
{
	volatile char frame[FRAME];

	memset((char *)frame, 0xff, sizeof(frame));

	if (depth > 1)
		grow(depth-1);
}

static void *maker(void *v)
{
	int i;

	sleep(1);

	
	printf("creating threads...\n");
	for(i = 0; i < 1300; i++) {
		pthread_t t;
		int ret;
		
		if (i % 100 == 0)
			printf("%d...\n", i);

		ret = pthread_create(&t, NULL, thr, NULL);
		if (ret) {
			printf("pthread_create failed: %s\n", strerror(ret));
			exit(1);
		}

		ret = pthread_join(t, NULL);
		if (ret) {
			printf("pthread_join failed: %s\n", strerror(ret));
			exit(1);
		}
	}
	
	kill(grower, SIGUSR1);

	return NULL;
}

int main()
{
	pthread_t pth;
	sigset_t mask;
	int status;
	struct sigaction sa;

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	sa.sa_handler = handler;
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);
	sigaction(SIGUSR1, &sa, NULL);
	
	grower = fork();

	if (grower == -1) {
		perror("fork");
		exit(1);
	}

	if (grower == 0) {
		pause();	
		grow(10);
		printf("stack grew OK\n");
		exit(0);
	}

	pthread_create(&pth, NULL, maker, NULL);

	
	if (waitpid(grower, &status, 0) != grower)
		printf("FAILED\n");
	else if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		printf("PASS: child OK\n");
	else
		printf("FAILED: exit status=%d\n", status);

	pthread_join(pth, NULL);

	return 0;
}

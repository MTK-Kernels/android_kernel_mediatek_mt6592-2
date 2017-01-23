#include <signal.h>
#include <stdio.h>
#include <string.h>
static volatile int got_ill;
static void handle_ill(int sig)
{
        got_ill = 1;
}
int main()
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_ill;
	sigaction(SIGILL, &sa, NULL);

	got_ill = 0;
        asm volatile(".long 0\n");
        if (got_ill)
                printf("0x00000000 does not loop\n");

	got_ill = 0;
        asm volatile(".long 0xffffffff\n.long 0xffff0000\n");
        if (got_ill)
                printf("0xffffffff does not loop\n");
	return 0;
}


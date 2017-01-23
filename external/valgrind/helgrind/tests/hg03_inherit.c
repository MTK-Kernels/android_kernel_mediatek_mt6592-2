


#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

static volatile int shared[2];

static void *t1(void *v)
{
	volatile int *ip = (int *)v;
	if (0) printf("ta W\n");
	*ip += 44;
	*ip *= 2;
	sleep(1);
	return 0;
}

static void *t2(void *v)
{
	volatile int *ip = (int *)v;
	sleep(2);
	if (0) printf("tb W\n");
	*ip += 88;
	*ip *= 3;
	sleep(1);
	return 0;
}

int main(void)
{
	pthread_t a, b;
	volatile int ret = 0;

	sleep(0);

	shared[0] = 22;
	shared[1] = 77;

	pthread_create(&a, NULL, t1, (void *)&shared[0]);
	
	pthread_create(&b, NULL, t2, (void *)&shared[1]);
	

	pthread_join(a, NULL);
	

	
	
	if (0) printf("r R1\n");
	ret += shared[0];	

	
	
	if (0) printf("r R2\n");
	ret += shared[1];	

	pthread_join(b, NULL);


	return ret;
}

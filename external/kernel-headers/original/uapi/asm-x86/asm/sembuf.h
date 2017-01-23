#ifndef _ASM_X86_SEMBUF_H
#define _ASM_X86_SEMBUF_H

struct semid64_ds {
	struct ipc64_perm sem_perm;	
	__kernel_time_t	sem_otime;	
	__kernel_ulong_t __unused1;
	__kernel_time_t	sem_ctime;	
	__kernel_ulong_t __unused2;
	__kernel_ulong_t sem_nsems;	
	__kernel_ulong_t __unused3;
	__kernel_ulong_t __unused4;
};

#endif 

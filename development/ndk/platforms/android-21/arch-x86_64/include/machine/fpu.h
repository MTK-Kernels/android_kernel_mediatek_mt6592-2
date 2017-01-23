
#ifndef	_MACHINE_FPU_H_
#define	_MACHINE_FPU_H_

#include <sys/types.h>


struct fxsave64 {
	u_int16_t  fx_fcw;
	u_int16_t  fx_fsw;
	u_int8_t   fx_ftw;
	u_int8_t   fx_unused1;
	u_int16_t  fx_fop;
	u_int64_t  fx_rip;
	u_int64_t  fx_rdp;
	u_int32_t  fx_mxcsr;
	u_int32_t  fx_mxcsr_mask;
	u_int64_t  fx_st[8][2];   
	u_int64_t  fx_xmm[16][2]; 
	u_int8_t   fx_unused3[96];
} __packed;

struct savefpu {
	struct fxsave64 fp_fxsave;	
	u_int16_t fp_ex_sw;		
	u_int16_t fp_ex_tw;		
};

#define	__INITIAL_NPXCW__	0x037f
#define __INITIAL_MXCSR__ 	0x1f80
#define __INITIAL_MXCSR_MASK__	0xffbf

#ifdef _KERNEL
struct trapframe;
struct cpu_info;

extern uint32_t	fpu_mxcsr_mask;

void fpuinit(struct cpu_info *);
void fpudrop(void);
void fpudiscard(struct proc *);
void fputrap(struct trapframe *);
void fpusave_proc(struct proc *, int);
void fpusave_cpu(struct cpu_info *, int);
void fpu_kernel_enter(void);
void fpu_kernel_exit(void);

#endif

#endif 



/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2010-2013 RT-RK
      mips-valgrind@rt-rk.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef __VKI_MIPS32_LINUX_H
#define __VKI_MIPS32_LINUX_H

#include <config.h>

#if defined (_MIPSEL)
#define VKI_LITTLE_ENDIAN  1
#elif defined (_MIPSEB)
#define VKI_BIG_ENDIAN  1
#endif



typedef __signed__ char __vki_s8;
typedef unsigned char __vki_u8;

typedef __signed__ short __vki_s16;
typedef unsigned short __vki_u16;

typedef __signed__ int __vki_s32;
typedef unsigned int __vki_u32;

typedef __signed char vki_s8;
typedef unsigned char vki_u8;

typedef __signed short vki_s16;
typedef unsigned short vki_u16;

typedef __signed int vki_s32;
typedef unsigned int vki_u32;

typedef __signed__ long long __vki_s64;
typedef unsigned long long __vki_u64;



#define VKI_PAGE_SHIFT          MIPS_PAGE_SHIFT
#define VKI_PAGE_SIZE           (1UL << VKI_PAGE_SHIFT)
#define VKI_PAGE_MASK           (~(VKI_PAGE_SIZE-1))
#define VKI_MAX_PAGE_SHIFT      VKI_PAGE_SHIFT
#define VKI_MAX_PAGE_SIZE       VKI_PAGE_SIZE


#define VKI_SHMLBA  0x40000


#define VKI_MINSIGSTKSZ     2048

#define VKI_SIG_BLOCK       1    
#define VKI_SIG_UNBLOCK     2    
#define VKI_SIG_SETMASK     3    

typedef void __vki_signalfn_t(int);
typedef __vki_signalfn_t __user *__vki_sighandler_t;

typedef void __vki_restorefn_t(void);
typedef __vki_restorefn_t __user *__vki_sigrestore_t;

#define VKI_SIG_DFL	((__vki_sighandler_t)0)	
#define VKI_SIG_IGN	((__vki_sighandler_t)1)	

#define _VKI_NSIG		128
#define _VKI_NSIG_BPW	32
#define _VKI_NSIG_WORDS	(_VKI_NSIG / _VKI_NSIG_BPW)

typedef unsigned long vki_old_sigset_t;		

typedef struct {
        unsigned long sig[_VKI_NSIG_WORDS];
} vki_sigset_t;

#define VKI_SIGHUP		 1	
#define VKI_SIGINT		 2	
#define VKI_SIGQUIT		 3	
#define VKI_SIGILL		 4	
#define VKI_SIGTRAP		 5	
#define VKI_SIGIOT		 6	
#define VKI_SIGABRT		 VKI_SIGIOT	
#define VKI_SIGEMT		 7
#define VKI_SIGFPE		 8	
#define VKI_SIGKILL		 9	
#define VKI_SIGBUS		10	
#define VKI_SIGSEGV		11	
#define VKI_SIGSYS		12
#define VKI_SIGPIPE		13	
#define VKI_SIGALRM		14	
#define VKI_SIGTERM		15	
#define VKI_SIGUSR1		16	
#define VKI_SIGUSR2		17	
#define VKI_SIGCHLD		18	
#define VKI_SIGCLD		VKI_SIGCHLD	
#define VKI_SIGPWR		19	
#define VKI_SIGWINCH	20	
#define VKI_SIGURG		21	
#define VKI_SIGIO		22	
#define VKI_SIGPOLL		VKI_SIGIO	
#define VKI_SIGSTOP		23	
#define VKI_SIGTSTP		24	
#define VKI_SIGCONT		25	
#define VKI_SIGTTIN		26	
#define VKI_SIGTTOU		27	
#define VKI_SIGVTALRM	28	
#define VKI_SIGPROF		29	
#define VKI_SIGXCPU		30	
#define VKI_SIGXFSZ		31	

#define VKI_SIGRTMIN    32
#define VKI_SIGRTMAX    _VKI_NSIG

#define VKI_SA_ONSTACK		0x08000000
#define VKI_SA_RESETHAND	0x80000000
#define VKI_SA_RESTART		0x10000000
#define VKI_SA_SIGINFO		0x00000008
#define VKI_SA_NODEFER		0x40000000
#define VKI_SA_NOCLDWAIT	0x00010000
#define VKI_SA_NOCLDSTOP	0x00000001

#define VKI_SA_NOMASK		VKI_SA_NODEFER
#define VKI_SA_ONESHOT		VKI_SA_RESETHAND

#define VKI_SA_RESTORER		0x04000000

#define VKI_SS_ONSTACK		1
#define VKI_SS_DISABLE		2

struct vki_old_sigaction {
        
        
        
        
    unsigned long sa_flags;
        __vki_sighandler_t ksa_handler;
        vki_old_sigset_t sa_mask;
        __vki_sigrestore_t sa_restorer;
};

struct vki_sigaction {
        unsigned int    sa_flags;
        __vki_sighandler_t  sa_handler;
        vki_sigset_t        sa_mask;
};


struct vki_sigaction_base {
        
        unsigned long sa_flags;
	__vki_sighandler_t ksa_handler;

	vki_sigset_t sa_mask;		
        __vki_sigrestore_t sa_restorer;
};

typedef  struct vki_sigaction_base  vki_sigaction_toK_t;
typedef  struct vki_sigaction_base  vki_sigaction_fromK_t;

typedef struct vki_sigaltstack {
	void __user *ss_sp;
	vki_size_t ss_size;
	int ss_flags;

} vki_stack_t;



struct _vki_fpreg {
	unsigned short significand[4];
	unsigned short exponent;
};

struct _vki_fpxreg {
	unsigned short significand[4];
	unsigned short exponent;
	unsigned short padding[3];
};

struct _vki_xmmreg {
	unsigned long element[4];
};

struct _vki_fpstate {
	
	unsigned long 	cw;
	unsigned long	sw;
	unsigned long	tag;
	unsigned long	ipoff;
	unsigned long	cssel;
	unsigned long	dataoff;
	unsigned long	datasel;
	struct _vki_fpreg	_st[8];
	unsigned short	status;
	unsigned short	magic;		

	
	unsigned long	_fxsr_env[6];	
	unsigned long	mxcsr;
	unsigned long	reserved;
	struct _vki_fpxreg	_fxsr_st[8];	
	struct _vki_xmmreg	_xmm[8];
	unsigned long	padding[56];
};


struct vki_sigcontext {
        unsigned int            sc_regmask;     
        unsigned int            sc_status;      
        unsigned long long      sc_pc;
        unsigned long long      sc_regs[32];
        unsigned long long      sc_fpregs[32];
        unsigned int            sc_acx;         
        unsigned int            sc_fpc_csr;
        unsigned int            sc_fpc_eir;     
        unsigned int            sc_used_math;
        unsigned int            sc_dsp;         
        unsigned long long      sc_mdhi;
        unsigned long long      sc_mdlo;
        unsigned long           sc_hi1;         
        unsigned long           sc_lo1;         
        unsigned long           sc_hi2;         
        unsigned long           sc_lo2;
        unsigned long           sc_hi3;
        unsigned long           sc_lo3;
};


#define VKI_PROT_NONE		0x0      
#define VKI_PROT_READ		0x1      
#define VKI_PROT_WRITE		0x2      /* page can be written */
#define VKI_PROT_EXEC		0x4      
#define VKI_PROT_GROWSDOWN	0x01000000	
#define VKI_PROT_GROWSUP	0x02000000	

#define VKI_MAP_SHARED		0x001     
#define VKI_MAP_PRIVATE		0x002     
#define VKI_MAP_FIXED		0x010     

#define VKI_MAP_NORESERVE	0x0400   

#define VKI_MAP_NORESERVE   0x0400          
#define VKI_MAP_ANONYMOUS   0x0800          
#define VKI_MAP_GROWSDOWN   0x1000          
#define VKI_MAP_DENYWRITE   0x2000          
#define VKI_MAP_EXECUTABLE  0x4000          
#define VKI_MAP_LOCKED      0x8000          
#define VKI_MAP_POPULATE    0x10000         
#define VKI_MAP_NONBLOCK    0x20000         



#define VKI_O_ACCMODE		   03
#define VKI_O_RDONLY		   00
#define VKI_O_WRONLY		   01
#define VKI_O_RDWR		   02

#define VKI_O_CREAT		0x0100		
#define VKI_O_EXCL		0x0400		

#define VKI_O_TRUNC		0x0200	

#define VKI_O_APPEND		0x0008
#define VKI_O_NONBLOCK		0x0080
#define VKI_O_LARGEFILE     	0x2000

#define VKI_AT_FDCWD            -100

#define VKI_F_DUPFD		 0			
#define VKI_F_GETFD		 1			
#define VKI_F_SETFD		 2			
#define VKI_F_GETFL		 3			
#define VKI_F_SETFL		 4			

#define VKI_F_GETLK		 14
#define VKI_F_SETLK		 6
#define VKI_F_SETLKW		 7

#define VKI_F_SETOWN		 24			
#define VKI_F_GETOWN		 23			
#define VKI_F_SETSIG		 10			
#define VKI_F_GETSIG		 11			

#define VKI_F_SETOWN_EX		15
#define VKI_F_GETOWN_EX		16

#define VKI_F_OFD_GETLK		36
#define VKI_F_OFD_SETLK		37
#define VKI_F_OFD_SETLKW	38

#define VKI_F_GETLK64		33			
#define VKI_F_SETLK64		34
#define VKI_F_SETLKW64		35

#define VKI_FD_CLOEXEC	 1		

#define VKI_F_LINUX_SPECIFIC_BASE	1024

struct vki_f_owner_ex {
	int	type;
	__vki_kernel_pid_t	pid;
};


#define VKI_RLIMIT_DATA		2   
#define VKI_RLIMIT_STACK	3   
#define VKI_RLIMIT_CORE		4   
#define VKI_RLIMIT_NOFILE	5   


#define VKI_SOL_SOCKET	0xffff

#define VKI_SO_TYPE	0x1008

#define VKI_SO_ATTACH_FILTER	26


#define VKI_SIOCSPGRP           0x8902
#define VKI_SIOCGPGRP           0x8904
#define VKI_SIOCATMARK          0x8905
#define VKI_SIOCGSTAMP          0x8906      
#define VKI_SIOCGSTAMPNS        0x8907      


struct vki_stat {
	unsigned	st_dev;
	long		st_pad1[3];		
	unsigned long	st_ino;
	unsigned int	st_mode;
	unsigned long	st_nlink;
	unsigned int	st_uid;
	unsigned int	st_gid;
	unsigned 	st_rdev;
	long		st_pad2[2];
	long		st_size;
	long		st_pad3;
	long		st_atime;
	long		st_atime_nsec;
	long		st_mtime;
	long		st_mtime_nsec;
	long		st_ctime;
	long		st_ctime_nsec;
	long		st_blksize;
	long		st_blocks;
	long		st_pad4[14];
};


struct vki_stat64 {
	unsigned long	st_dev;
	unsigned long	st_pad0[3];	

	unsigned long long	st_ino;

	unsigned int	st_mode;
	unsigned long	st_nlink;

	unsigned int	st_uid;
	unsigned int	st_gid;

	unsigned long	st_rdev;
	unsigned long	st_pad1[3];	

	long long	st_size;

	long		st_atime;
	unsigned long	st_atime_nsec;	

	long		st_mtime;
	unsigned long	st_mtime_nsec;	

	long		st_ctime;
	unsigned long	st_ctime_nsec;	

	unsigned long	st_blksize;
	unsigned long	st_pad2;

	long long	st_blocks;
};


struct vki_statfs {
	long		f_type;
	long		f_bsize;
	long		f_frsize;	
	long		f_blocks;
	long		f_bfree;
	long		f_files;
	long		f_ffree;
	long		f_bavail;

	
	__vki_kernel_fsid_t	f_fsid;
	long		f_namelen;
	long		f_spare[6];
};


struct vki_winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

#define NCC	8
#define NCCS	23
struct vki_termio {
	unsigned short c_iflag;		
	unsigned short c_oflag;		
	unsigned short c_cflag;		
	unsigned short c_lflag;		
	char c_line;			
	unsigned char c_cc[NCCS];	
};



typedef unsigned char   vki_cc_t;
typedef unsigned long   vki_speed_t;
typedef unsigned long   vki_tcflag_t;

struct vki_termios {
	vki_tcflag_t c_iflag;		
	vki_tcflag_t c_oflag;		
	vki_tcflag_t c_cflag;		
	vki_tcflag_t c_lflag;		
	vki_cc_t c_line;			
	vki_cc_t c_cc[NCCS];		
};


#define _VKI_IOC_NRBITS		8
#define _VKI_IOC_TYPEBITS	8
#define _VKI_IOC_SIZEBITS	13
#define _VKI_IOC_DIRBITS	3

#define _VKI_IOC_NRMASK		((1 << _VKI_IOC_NRBITS)-1)
#define _VKI_IOC_TYPEMASK	((1 << _VKI_IOC_TYPEBITS)-1)
#define _VKI_IOC_SIZEMASK	((1 << _VKI_IOC_SIZEBITS)-1)
#define _VKI_IOC_DIRMASK	((1 << _VKI_IOC_DIRBITS)-1)

#define _VKI_IOC_NRSHIFT	0
#define _VKI_IOC_TYPESHIFT	(_VKI_IOC_NRSHIFT+_VKI_IOC_NRBITS)
#define _VKI_IOC_SIZESHIFT	(_VKI_IOC_TYPESHIFT+_VKI_IOC_TYPEBITS)
#define _VKI_IOC_DIRSHIFT	(_VKI_IOC_SIZESHIFT+_VKI_IOC_SIZEBITS)

#define _VKI_IOC_NONE	1U
#define _VKI_IOC_READ	2U
#define _VKI_IOC_WRITE	4U

#define _VKI_IOC(dir,type,nr,size) \
	(((dir)  << _VKI_IOC_DIRSHIFT) | \
	 ((type) << _VKI_IOC_TYPESHIFT) | \
	 ((nr)   << _VKI_IOC_NRSHIFT) | \
	 ((size) << _VKI_IOC_SIZESHIFT))

extern unsigned int __VKI_invalid_size_argument_for_IOC;
#define _VKI_IO(type,nr)	_VKI_IOC(_VKI_IOC_NONE,(type),(nr),0)
#define _VKI_IOR(type,nr,size)	_VKI_IOC(_VKI_IOC_READ,(type),(nr),(_VKI_IOC_TYPECHECK(size)))
#define _VKI_IOW(type,nr,size)	_VKI_IOC(_VKI_IOC_WRITE,(type),(nr),(_VKI_IOC_TYPECHECK(size)))
#define _VKI_IOWR(type,nr,size)	_VKI_IOC(_VKI_IOC_READ|_VKI_IOC_WRITE,(type),(nr),(_VKI_IOC_TYPECHECK(size)))

#define _VKI_IOC_DIR(nr)	(((nr) >> _VKI_IOC_DIRSHIFT) & _VKI_IOC_DIRMASK)
#define _VKI_IOC_TYPE(nr)	(((nr) >> _VKI_IOC_TYPESHIFT) & _VKI_IOC_TYPEMASK)
#define _VKI_IOC_NR(nr)		(((nr) >> _VKI_IOC_NRSHIFT) & _VKI_IOC_NRMASK)
#define _VKI_IOC_SIZE(nr)	(((nr) >> _VKI_IOC_SIZESHIFT) & _VKI_IOC_SIZEMASK)


#define VKI_TCGETA		0x5401
#define VKI_TCSETA		0x5402	
#define VKI_TCSETAW		0x5403
#define VKI_TCSETAF		0x5404

#define VKI_TCSBRK		0x5405
#define VKI_TCXONC		0x5406
#define VKI_TCFLSH		0x5407

#define VKI_TCGETS		0x540d
#define VKI_TCSETS		0x540e
#define VKI_TCSETSW		0x540f
#define VKI_TCSETSF		0x5410

#define VKI_TIOCEXCL		0x740d		
#define VKI_TIOCNXCL		0x740e		
#define VKI_TIOCOUTQ		0x7472		
#define VKI_TIOCSTI		0x5472		
#define VKI_TIOCMGET		0x741d		
#define VKI_TIOCMBIS		0x741b		
#define VKI_TIOCMBIC		0x741c		
#define VKI_TIOCMSET		0x741a		
#define VKI_TIOCPKT		0x5470		
#define	 VKI_TIOCPKT_DATA		0x00	
#define	 VKI_TIOCPKT_FLUSHREAD		0x01	
#define	 VKI_TIOCPKT_FLUSHWRITE		0x02	
#define	 VKI_TIOCPKT_STOP		0x04	
#define	 VKI_TIOCPKT_START		0x08	
#define	 VKI_TIOCPKT_NOSTOP		0x10	
#define	 VKI_TIOCPKT_DOSTOP		0x20	
#define VKI_TIOCSWINSZ	_VKI_IOW('t', 103, struct vki_winsize)	
#define VKI_TIOCGWINSZ	_VKI_IOR('t', 104, struct vki_winsize)	
#define VKI_TIOCNOTTY	0x5471		
#define VKI_TIOCSETD	0x7401
#define VKI_TIOCGETD	0x7400

#define VKI_FIOCLEX		0x6601
#define VKI_FIONCLEX		0x6602
#define VKI_FIOASYNC		0x667d
#define VKI_FIONBIO		0x667e
#define VKI_FIOQSIZE		0x667f

#define VKI_TIOCGLTC		0x7474			
#define VKI_TIOCSLTC		0x7475			
#define VKI_TIOCSPGRP		_VKI_IOW('t', 118, int)	
#define VKI_TIOCGPGRP		_VKI_IOR('t', 119, int)	
#define VKI_TIOCCONS		_VKI_IOW('t', 120, int)	

#define VKI_FIONREAD		0x467f
#define VKI_TIOCINQ		FIONREAD

#define VKI_TIOCGETP        	0x7408
#define VKI_TIOCSETP        	0x7409
#define VKI_TIOCSETN        	0x740a			

#define VKI_TIOCSBRK	0x5427  
#define VKI_TIOCCBRK	0x5428  
#define VKI_TIOCGSID	0x7416  
#define VKI_TIOCGPTN	_VKI_IOR('T',0x30, unsigned int) 
#define VKI_TIOCSPTLCK	_VKI_IOW('T',0x31, int)  

#define VKI_TIOCSCTTY		0x5480		
#define VKI_TIOCGSOFTCAR	0x5481
#define VKI_TIOCSSOFTCAR	0x5482
#define VKI_TIOCLINUX		0x5483
#define VKI_TIOCGSERIAL		0x5484
#define VKI_TIOCSSERIAL		0x5485
#define VKI_TCSBRKP		0x5486	
#define VKI_TIOCSERCONFIG	0x5488
#define VKI_TIOCSERGWILD	0x5489
#define VKI_TIOCSERSWILD	0x548a
#define VKI_TIOCGLCKTRMIOS	0x548b
#define VKI_TIOCSLCKTRMIOS	0x548c
#define VKI_TIOCSERGSTRUCT	0x548d 
#define VKI_TIOCSERGETLSR   	0x548e 
#define VKI_TIOCSERGETMULTI 	0x548f 
#define VKI_TIOCSERSETMULTI 	0x5490 
#define VKI_TIOCMIWAIT      	0x5491 
#define VKI_TIOCGICOUNT     	0x5492 
#define VKI_TIOCGHAYESESP	0x5493 
#define VKI_TIOCSHAYESESP	0x5494 


#define VKI_POLLIN		0x0001

struct vki_pollfd {
	int fd;
	short events;
	short revents;
};

struct vki_ucontext {
	unsigned long		uc_flags;
	struct vki_ucontext    *uc_link;
	vki_stack_t		uc_stack;
	struct vki_sigcontext	uc_mcontext;
	vki_sigset_t		uc_sigmask;	
};

typedef void vki_modify_ldt_t;


struct vki_ipc64_perm
{
        __vki_kernel_key_t  key;
        __vki_kernel_uid_t  uid;
        __vki_kernel_gid_t  gid;
        __vki_kernel_uid_t  cuid;
        __vki_kernel_gid_t  cgid;
        __vki_kernel_mode_t mode;
        unsigned short  seq;
        unsigned short  __pad1;
        unsigned long   __unused1;
        unsigned long   __unused2;
};


struct vki_semid64_ds {
        struct vki_ipc64_perm sem_perm;             
        __vki_kernel_time_t sem_otime;              
        __vki_kernel_time_t sem_ctime;              
        unsigned long   sem_nsems;              
        unsigned long   __unused1;
        unsigned long   __unused2;
};



struct vki_msqid64_ds {
	struct vki_ipc64_perm msg_perm;
	__vki_kernel_time_t msg_stime;	
	unsigned long	__unused1;
	__vki_kernel_time_t msg_rtime;	
	unsigned long	__unused2;
	__vki_kernel_time_t msg_ctime;	
	unsigned long	__unused3;
	unsigned long  msg_cbytes;	
	unsigned long  msg_qnum;	
	unsigned long  msg_qbytes;	
	__vki_kernel_pid_t msg_lspid;	
	__vki_kernel_pid_t msg_lrpid;	
	unsigned long  __unused4;
	unsigned long  __unused5;
};


struct vki_ipc_kludge {
        struct vki_msgbuf __user *msgp;
        long msgtyp;
};

#define VKI_SEMOP            1
#define VKI_SEMGET           2
#define VKI_SEMCTL           3
#define VKI_SEMTIMEDOP       4
#define VKI_MSGSND          11
#define VKI_MSGRCV          12
#define VKI_MSGGET          13
#define VKI_MSGCTL          14
#define VKI_SHMAT           21
#define VKI_SHMDT           22
#define VKI_SHMGET          23
#define VKI_SHMCTL          24


struct vki_shmid64_ds {
        struct vki_ipc64_perm       shm_perm;       
        vki_size_t                  shm_segsz;      
        __vki_kernel_time_t         shm_atime;      
        __vki_kernel_time_t         shm_dtime;      
        __vki_kernel_time_t         shm_ctime;      
        __vki_kernel_pid_t          shm_cpid;       
        __vki_kernel_pid_t          shm_lpid;       
        unsigned long           shm_nattch;     
        unsigned long           __unused1;
        unsigned long           __unused2;
};

struct vki_shminfo64 {
        unsigned long   shmmax;
        unsigned long   shmmin;
        unsigned long   shmmni;
        unsigned long   shmseg;
        unsigned long   shmall;
        unsigned long   __unused1;
        unsigned long   __unused2;
        unsigned long   __unused3;
        unsigned long   __unused4;
};

struct vki_pt_regs {
#ifdef CONFIG_32BIT
        
        unsigned long pad0[6];
#endif
        
        unsigned long regs[32];

        
        unsigned long cp0_status;
        unsigned long hi;
        unsigned long lo;
#ifdef CONFIG_CPU_HAS_SMARTMIPS
        unsigned long acx;
#endif
        unsigned long cp0_badvaddr;
        unsigned long cp0_cause;
        unsigned long cp0_epc;
#ifdef CONFIG_MIPS_MT_SMTC
        unsigned long cp0_tcstatus;
#endif 
#ifdef CONFIG_CPU_CAVIUM_OCTEON
        unsigned long long mpl[3];        
        unsigned long long mtp[3];        
#endif
} __attribute__ ((aligned (8)));


#define vki_user_regs_struct vki_pt_regs

#define MIPS_lo  	lo
#define MIPS_hi  	hi
#define MIPS_r31		regs[31]
#define MIPS_r30		regs[30]
#define MIPS_r29		regs[29]
#define MIPS_r28		regs[28]
#define MIPS_r27		regs[27]
#define MIPS_r26		regs[26]
#define MIPS_r25		regs[25]
#define MIPS_r24		regs[24]
#define MIPS_r23		regs[23]
#define MIPS_r22		regs[22]
#define MIPS_r21		regs[21]
#define MIPS_r20		regs[20]
#define MIPS_r19		regs[19]
#define MIPS_r18		regs[18]
#define MIPS_r17		regs[17]
#define MIPS_r16		regs[16]
#define MIPS_r15		regs[15]
#define MIPS_r14		regs[14]
#define MIPS_r13		regs[13]
#define MIPS_r12		regs[12]
#define MIPS_r11		regs[11]
#define MIPS_r10		regs[10]
#define MIPS_r9		regs[9]
#define MIPS_r8		regs[8]
#define MIPS_r7		regs[7]
#define MIPS_r6		regs[6]
#define MIPS_r5		regs[5]
#define MIPS_r4		regs[4]
#define MIPS_r3		regs[3]
#define MIPS_r2		regs[2]
#define MIPS_r1		regs[1]
#define MIPS_r0		regs[0]

#define VKI_PTRACE_GETREGS            12
#define VKI_PTRACE_SETREGS            13
#define VKI_PTRACE_GETFPREGS          14
#define VKI_PTRACE_SETFPREGS          15
typedef unsigned long vki_elf_greg_t;

#define VKI_ELF_NGREG (sizeof (struct vki_user_regs_struct) / sizeof(vki_elf_greg_t))
#define VKI_ELF_NFPREG			33	

typedef vki_elf_greg_t vki_elf_gregset_t[VKI_ELF_NGREG];

typedef double vki_elf_fpreg_t;
typedef vki_elf_fpreg_t vki_elf_fpregset_t[VKI_ELF_NFPREG];

typedef struct vki_user_fxsr_struct vki_elf_fpxregset_t;

#define VKI_AT_SYSINFO		32
#define HAVE_ARCH_SIGINFO_T

typedef union vki_sigval {
	int sival_int;
	void __user *sival_ptr;
} vki_sigval_t;

#ifndef __VKI_ARCH_SI_PREAMBLE_SIZE
#define __VKI_ARCH_SI_PREAMBLE_SIZE	(3 * sizeof(int))
#endif

#define VKI_SI_MAX_SIZE	128

#ifndef VKI_SI_PAD_SIZE
#define VKI_SI_PAD_SIZE	((VKI_SI_MAX_SIZE - __VKI_ARCH_SI_PREAMBLE_SIZE) / sizeof(int))
#endif

#ifndef __VKI_ARCH_SI_UID_T
#define __VKI_ARCH_SI_UID_T	vki_uid_t
#endif

#ifndef __VKI_ARCH_SI_BAND_T
#define __VKI_ARCH_SI_BAND_T long
#endif

typedef struct vki_siginfo {
        int si_signo;
        int si_code;
        int si_errno;
        int __pad0[VKI_SI_MAX_SIZE / sizeof(int) - VKI_SI_PAD_SIZE - 3];

        union {
                int _pad[VKI_SI_PAD_SIZE];

                
                struct {
                        vki_pid_t _pid;             
                        __VKI_ARCH_SI_UID_T _uid;   
                } _kill;

                
                struct {
                        vki_timer_t _tid;           
                        int _overrun;           
                        char _pad[sizeof( __VKI_ARCH_SI_UID_T) - sizeof(int)];
                        vki_sigval_t _sigval;       
                        int _sys_private;       
                } _timer;

                
                struct {
                        vki_pid_t _pid;             
                        __VKI_ARCH_SI_UID_T _uid;   
                        vki_sigval_t _sigval;
                } _rt;

                
                struct {
                        vki_pid_t _pid;             
                        __VKI_ARCH_SI_UID_T _uid;   
                        int _status;            
                        vki_clock_t _utime;
                        vki_clock_t _stime;
                } _sigchld;

                
                struct {
                        vki_pid_t _pid;             
                        vki_clock_t _utime;
                        int _status;            
                        vki_clock_t _stime;
                } _irix_sigchld;

                
                struct {
                        void __user *_addr; 
#ifdef __ARCH_SI_TRAPNO
                        int _trapno;    
#endif
                } _sigfault;

                
                struct {
                        __VKI_ARCH_SI_BAND_T _band; 
                        int _fd;
                } _sigpoll;
        } _sifields;
} vki_siginfo_t;

#define VKI_BRK_OVERFLOW         6    
#define VKI_BRK_DIVZERO          7    

enum vki_sock_type {
        VKI_SOCK_STREAM = 2,
        
};
#define ARCH_HAS_SOCKET_TYPES 1


#define	VKI_ENOSYS       89  
#define	VKI_EOVERFLOW    79  

#endif 


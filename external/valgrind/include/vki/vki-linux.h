

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2000-2013 Julian Seward 
      jseward@acm.org

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



#ifndef __VKI_LINUX_H
#define __VKI_LINUX_H


#if defined(VGA_x86)
#  include "vki-posixtypes-x86-linux.h"
#elif defined(VGA_amd64)
#  include "vki-posixtypes-amd64-linux.h"
#elif defined(VGA_ppc32)
#  include "vki-posixtypes-ppc32-linux.h"
#elif defined(VGA_ppc64be) || defined(VGA_ppc64le)
#  include "vki-posixtypes-ppc64-linux.h"
#elif defined(VGA_arm)
#  include "vki-posixtypes-arm-linux.h"
#elif defined(VGA_arm64)
#  include "vki-posixtypes-arm64-linux.h"
#elif defined(VGA_s390x)
#  include "vki-posixtypes-s390x-linux.h"
#elif defined(VGA_mips32)
#  include "vki-posixtypes-mips32-linux.h"
#elif defined(VGA_mips64)
#  include "vki-posixtypes-mips64-linux.h"
#elif defined(VGA_tilegx)
#  include "vki-posixtypes-tilegx-linux.h"
#else
#  error Unknown platform
#endif


#ifdef __cplusplus
template <bool b> struct vki_static_assert { int m_bitfield:(2*b-1); };
#define VKI_STATIC_ASSERT(expr)                         \
    (sizeof(vki_static_assert<(expr)>) - sizeof(int))
#else
#define VKI_STATIC_ASSERT(expr) (sizeof(struct { int:-!(expr); }))
#endif


#define _VKI_IOC_TYPECHECK(t)                                           \
    (VKI_STATIC_ASSERT((sizeof(t) == sizeof(t[1])                       \
                        && sizeof(t) < (1 << _VKI_IOC_SIZEBITS)))       \
     + sizeof(t))


# define __user


#ifdef __GNUC__
#define __vki_packed			__attribute__((packed))
#endif


#undef __VKI_NFDBITS
#define __VKI_NFDBITS	(8 * sizeof(unsigned long))

#undef __VKI_FD_SETSIZE
#define __VKI_FD_SETSIZE	1024

#undef __VKI_FDSET_LONGS
#define __VKI_FDSET_LONGS	(__VKI_FD_SETSIZE/__VKI_NFDBITS)

#undef __VKI_FDELT
#define	__VKI_FDELT(d)	((d) / __VKI_NFDBITS)

#undef __VKI_FDMASK
#define	__VKI_FDMASK(d)	(1UL << ((d) % __VKI_NFDBITS))

typedef struct {
	unsigned long fds_bits [__VKI_FDSET_LONGS];
} __vki_kernel_fd_set;

typedef int __vki_kernel_key_t;
typedef int __vki_kernel_mqd_t;


typedef __vki_kernel_fd_set	vki_fd_set;
typedef __vki_kernel_mode_t	vki_mode_t;
typedef __vki_kernel_off_t	vki_off_t;
typedef __vki_kernel_pid_t	vki_pid_t;
typedef __vki_kernel_key_t	vki_key_t;
typedef __vki_kernel_suseconds_t	vki_suseconds_t;
typedef __vki_kernel_timer_t	vki_timer_t;
typedef __vki_kernel_clockid_t	vki_clockid_t;
typedef __vki_kernel_mqd_t	vki_mqd_t;

typedef __vki_kernel_uid32_t	vki_uid_t;
typedef __vki_kernel_gid32_t	vki_gid_t;

typedef __vki_kernel_old_uid_t	vki_old_uid_t;
typedef __vki_kernel_old_gid_t	vki_old_gid_t;

typedef __vki_kernel_loff_t	vki_loff_t;

typedef __vki_kernel_size_t	vki_size_t;
typedef __vki_kernel_time_t	vki_time_t;
typedef __vki_kernel_clock_t	vki_clock_t;
typedef __vki_kernel_caddr_t	vki_caddr_t;

typedef unsigned long           vki_u_long;

typedef unsigned int	        vki_uint;


#if defined(VGA_x86)
#  include "vki-x86-linux.h"
#elif defined(VGA_amd64)
#  include "vki-amd64-linux.h"
#elif defined(VGA_ppc32)
#  include "vki-ppc32-linux.h"
#elif defined(VGA_ppc64be) || defined(VGA_ppc64le)
#  include "vki-ppc64-linux.h"
#elif defined(VGA_arm)
#  include "vki-arm-linux.h"
#elif defined(VGA_arm64)
#  include "vki-arm64-linux.h"
#elif defined(VGA_s390x)
#  include "vki-s390x-linux.h"
#elif defined(VGA_mips32)
#  include "vki-mips32-linux.h"
#elif defined(VGA_mips64)
#  include "vki-mips64-linux.h"
#elif defined(VGA_tilegx)
#  include "vki-tilegx-linux.h"
#else
#  error Unknown platform
#endif


typedef		__vki_s32	vki_int32_t;
typedef		__vki_s16	vki_int16_t;
typedef		__vki_s64	vki_int64_t;

typedef		__vki_u8	vki_uint8_t;
typedef		__vki_u16	vki_uint16_t;
typedef		__vki_u32	vki_uint32_t;
typedef		__vki_u64	vki_uint64_t;

typedef		__vki_u16	__vki_le16;


#define VKI_PATH_MAX       4096	


struct vki_sysinfo {
	long uptime;			
	unsigned long loads[3];		
	unsigned long totalram;		
	unsigned long freeram;		
	unsigned long sharedram;	
	unsigned long bufferram;	
	unsigned long totalswap;	
	unsigned long freeswap;		
	unsigned short procs;		
	unsigned short pad;		
	unsigned long totalhigh;	
	unsigned long freehigh;		
	unsigned int mem_unit;		
	char _f[20-2*sizeof(long)-sizeof(int)];	
};


#define VKI_CLOCK_REALTIME            0
#define VKI_CLOCK_MONOTONIC           1
#define VKI_CLOCK_PROCESS_CPUTIME_ID  2
#define VKI_CLOCK_THREAD_CPUTIME_ID   3

struct vki_timespec {
	vki_time_t	tv_sec;		
	long		tv_nsec;	
};

struct vki_timeval {
	vki_time_t	tv_sec;		
	vki_suseconds_t	tv_usec;	
};

struct vki_timezone {
	int	tz_minuteswest;	
	int	tz_dsttime;	
};

struct  vki_itimerspec {
        struct  vki_timespec it_interval;    
        struct  vki_timespec it_value;       
};

struct	vki_itimerval {
	struct	vki_timeval it_interval;	
	struct	vki_timeval it_value;	
};


struct vki_timex {
	unsigned int modes;	
	long offset;		
	long freq;		
	long maxerror;		
	long esterror;		
	int status;		
	long constant;		
	long precision;		
	long tolerance;		
	struct vki_timeval time;	
	long tick;		

	long ppsfreq;           
	long jitter;            
	int shift;              
	long stabil;            
	long jitcnt;            
	long calcnt;            
	long errcnt;            
	long stbcnt;            

	int  :32; int  :32; int  :32; int  :32;
	int  :32; int  :32; int  :32; int  :32;
	int  :32; int  :32; int  :32; int  :32;
};

#define VKI_ADJ_OFFSET			0x0001	
#define VKI_ADJ_FREQUENCY		0x0002	
#define VKI_ADJ_MAXERROR		0x0004	
#define VKI_ADJ_ESTERROR		0x0008	
#define VKI_ADJ_STATUS			0x0010	
#define VKI_ADJ_TIMECONST		0x0020	
#define VKI_ADJ_TAI			0x0080	
#define VKI_ADJ_TICK			0x4000	
#define VKI_ADJ_ADJTIME			0x8000	
#define VKI_ADJ_OFFSET_READONLY		0x2000	


struct vki_tms {
	vki_clock_t tms_utime;
	vki_clock_t tms_stime;
	vki_clock_t tms_cutime;
	vki_clock_t tms_cstime;
};


struct vki_utimbuf {
	vki_time_t actime;
	vki_time_t modtime;
};


#define VKI_CSIGNAL		0x000000ff	
#define VKI_CLONE_VM		0x00000100	
#define VKI_CLONE_FS		0x00000200	
#define VKI_CLONE_FILES		0x00000400	
#define VKI_CLONE_SIGHAND	0x00000800	
#define VKI_CLONE_VFORK		0x00004000	
#define VKI_CLONE_PARENT	0x00008000	
#define VKI_CLONE_THREAD	0x00010000	
#define VKI_CLONE_SYSVSEM	0x00040000	
#define VKI_CLONE_SETTLS	0x00080000	
#define VKI_CLONE_PARENT_SETTID	0x00100000	
#define VKI_CLONE_CHILD_CLEARTID	0x00200000	
#define VKI_CLONE_DETACHED	0x00400000	
#define VKI_CLONE_CHILD_SETTID	0x01000000	

struct vki_sched_param {
	int sched_priority;
};

#define VKI_TASK_COMM_LEN 16


#ifndef HAVE_ARCH_SIGINFO_T
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
	int si_errno;
	int si_code;

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
#endif

#define __VKI_SI_FAULT	0

#define VKI_SI_USER	0		
#define VKI_SI_TKILL	-6		

#define VKI_ILL_ILLOPC	(__VKI_SI_FAULT|1)	
#define VKI_ILL_ILLOPN	(__VKI_SI_FAULT|2)	
#define VKI_ILL_ILLADR	(__VKI_SI_FAULT|3)	
#define VKI_ILL_ILLTRP	(__VKI_SI_FAULT|4)	
#define VKI_ILL_PRVOPC	(__VKI_SI_FAULT|5)	
#define VKI_ILL_PRVREG	(__VKI_SI_FAULT|6)	
#define VKI_ILL_COPROC	(__VKI_SI_FAULT|7)	
#define VKI_ILL_BADSTK	(__VKI_SI_FAULT|8)	

#define VKI_FPE_INTDIV	(__VKI_SI_FAULT|1)	
#define VKI_FPE_INTOVF	(__VKI_SI_FAULT|2)	
#define VKI_FPE_FLTDIV	(__VKI_SI_FAULT|3)	
#define VKI_FPE_FLTOVF	(__VKI_SI_FAULT|4)	
#define VKI_FPE_FLTUND	(__VKI_SI_FAULT|5)	
#define VKI_FPE_FLTRES	(__VKI_SI_FAULT|6)	
#define VKI_FPE_FLTINV	(__VKI_SI_FAULT|7)	
#define VKI_FPE_FLTSUB	(__VKI_SI_FAULT|8)	

#define VKI_SEGV_MAPERR	(__VKI_SI_FAULT|1)	
#define VKI_SEGV_ACCERR	(__VKI_SI_FAULT|2)	

#define VKI_BUS_ADRALN	(__VKI_SI_FAULT|1)	
#define VKI_BUS_ADRERR	(__VKI_SI_FAULT|2)	
#define VKI_BUS_OBJERR	(__VKI_SI_FAULT|3)	

#define VKI_TRAP_BRKPT      (__VKI_SI_FAULT|1)  
#define VKI_TRAP_TRACE      (__VKI_SI_FAULT|2)  

#ifndef VKI___ARCH_SIGEV_PREAMBLE_SIZE
#define VKI___ARCH_SIGEV_PREAMBLE_SIZE	(sizeof(int) * 2 + sizeof(vki_sigval_t))
#endif

#define VKI_SIGEV_MAX_SIZE	64
#define VKI_SIGEV_PAD_SIZE	((VKI_SIGEV_MAX_SIZE - VKI___ARCH_SIGEV_PREAMBLE_SIZE) \
		/ sizeof(int))

#define VKI_SIGEV_THREAD_ID	4

typedef struct vki_sigevent {
	vki_sigval_t sigev_value;
	int sigev_signo;
	int sigev_notify;
	union {
		int _pad[VKI_SIGEV_PAD_SIZE];
		 int _tid;

		struct {
			void (*_function)(vki_sigval_t);
			void *_attribute;	
		} _sigev_thread;
	} _sigev_un;
} vki_sigevent_t;

#define vki_sigev_notify_thread_id	_sigev_un._tid


#define VKI_SEEK_SET              0
#define VKI_SEEK_CUR              1
#define VKI_SEEK_END              2


#define VKI_SYS_SOCKET		1	
#define VKI_SYS_BIND		2	
#define VKI_SYS_CONNECT		3	
#define VKI_SYS_LISTEN		4	
#define VKI_SYS_ACCEPT		5	
#define VKI_SYS_GETSOCKNAME	6	
#define VKI_SYS_GETPEERNAME	7	
#define VKI_SYS_SOCKETPAIR	8	
#define VKI_SYS_SEND		9	
#define VKI_SYS_RECV		10	
#define VKI_SYS_SENDTO		11	
#define VKI_SYS_RECVFROM	12	
#define VKI_SYS_SHUTDOWN	13	
#define VKI_SYS_SETSOCKOPT	14	
#define VKI_SYS_GETSOCKOPT	15	
#define VKI_SYS_SENDMSG		16	
#define VKI_SYS_RECVMSG		17	
#define VKI_SYS_ACCEPT4		18	
#define VKI_SYS_RECVMMSG	19	
#define VKI_SYS_SENDMMSG	20	

#ifndef ARCH_HAS_SOCKET_TYPES
enum vki_sock_type {
	VKI_SOCK_STREAM	= 1,
	
};
#endif 


struct vki_iovec
{
	void __user *iov_base;	
	__vki_kernel_size_t iov_len; 
};


#define __KINLINE static __inline__

typedef unsigned short	vki_sa_family_t;

struct vki_sockaddr {
	vki_sa_family_t	sa_family;	
	char		sa_data[14];	
};

struct vki_msghdr {
	void	*	msg_name;	
	int		msg_namelen;	
	struct vki_iovec *	msg_iov;	
	__vki_kernel_size_t	msg_iovlen;	
	void 	*	msg_control;	
	__vki_kernel_size_t	msg_controllen;	
	unsigned	msg_flags;
};

struct vki_mmsghdr {
	struct vki_msghdr   msg_hdr;
	unsigned        msg_len;
};

struct vki_cmsghdr {
	__vki_kernel_size_t	cmsg_len;	
        int		cmsg_level;	
        int		cmsg_type;	
};

#define __VKI_CMSG_NXTHDR(ctl, len, cmsg) __vki_cmsg_nxthdr((ctl),(len),(cmsg))
#define VKI_CMSG_NXTHDR(mhdr, cmsg) vki_cmsg_nxthdr((mhdr), (cmsg))

#define VKI_CMSG_ALIGN(len) ( ((len)+sizeof(long)-1) & ~(sizeof(long)-1) )

#define VKI_CMSG_DATA(cmsg)	((void *)((char *)(cmsg) + VKI_CMSG_ALIGN(sizeof(struct vki_cmsghdr))))

#define __VKI_CMSG_FIRSTHDR(ctl,len) ((len) >= sizeof(struct vki_cmsghdr) ? \
				  (struct vki_cmsghdr *)(ctl) : \
				  (struct vki_cmsghdr *)NULL)
#define VKI_CMSG_FIRSTHDR(msg)	__VKI_CMSG_FIRSTHDR((msg)->msg_control, (msg)->msg_controllen)

__KINLINE struct vki_cmsghdr * __vki_cmsg_nxthdr(void *__ctl, __vki_kernel_size_t __size,
					       struct vki_cmsghdr *__cmsg)
{
	struct vki_cmsghdr * __ptr;

	__ptr = (struct vki_cmsghdr*)(((unsigned char *) __cmsg) +  VKI_CMSG_ALIGN(__cmsg->cmsg_len));
	if ((unsigned long)((char*)(__ptr+1) - (char *) __ctl) > __size)
		return (struct vki_cmsghdr *)0;

	return __ptr;
}

__KINLINE struct vki_cmsghdr * vki_cmsg_nxthdr (struct vki_msghdr *__msg, struct vki_cmsghdr *__cmsg)
{
	return __vki_cmsg_nxthdr(__msg->msg_control, __msg->msg_controllen, __cmsg);
}

#define	VKI_SCM_RIGHTS	0x01		

#define VKI_AF_UNSPEC   0
#define VKI_AF_UNIX	1	
#define VKI_AF_INET	2	
#define VKI_AF_INET6	10	
#define VKI_AF_NETLINK  16
#define VKI_AF_BLUETOOTH 31	

#define VKI_MSG_NOSIGNAL	0x4000	

#define VKI_SOL_SCTP	132


struct vki_in_addr {
	__vki_u32	s_addr;
};

#define __VKI_SOCK_SIZE__	16	
struct vki_sockaddr_in {
  vki_sa_family_t	sin_family;	
  unsigned short int	sin_port;	
  struct vki_in_addr	sin_addr;	

  
  unsigned char		__pad[__VKI_SOCK_SIZE__ - sizeof(short int) -
			sizeof(unsigned short int) - sizeof(struct vki_in_addr)];
};

#define VKI_IPPROTO_TCP 6       


struct vki_in6_addr
{
	union 
	{
		__vki_u8	u6_addr8[16];
		__vki_u16	u6_addr16[8];
		__vki_u32	u6_addr32[4];
	} vki_in6_u;
#define vki_s6_addr		vki_in6_u.u6_addr8
#define vki_s6_addr16		vki_in6_u.u6_addr16
#define vki_s6_addr32		vki_in6_u.u6_addr32
};

struct vki_sockaddr_in6 {
	unsigned short int	sin6_family;    
	__vki_u16		sin6_port;      
	__vki_u32		sin6_flowinfo;  
	struct vki_in6_addr	sin6_addr;      
	__vki_u32		sin6_scope_id;  
};


#define VKI_TCP_NODELAY    1       



#define VKI_UNIX_PATH_MAX	108

struct vki_sockaddr_un {
	vki_sa_family_t sun_family;	
	char sun_path[VKI_UNIX_PATH_MAX];	
};


struct vki_sockaddr_nl {
        vki_sa_family_t    nl_family;      
        unsigned short     nl_pad;         
        __vki_u32          nl_pid;         
        __vki_u32          nl_groups;      
};


#define	VKI_IFNAMSIZ	16

struct vki_ifmap 
{
	unsigned long mem_start;
	unsigned long mem_end;
	unsigned short base_addr; 
	unsigned char irq;
	unsigned char dma;
	unsigned char port;
	
};

struct vki_if_settings
{
	unsigned int type;	
	unsigned int size;	
	union {
                
                
		
		void __user *raw_hdlc;
		void __user *cisco;
		void __user *fr;
		void __user *fr_pvc;
		void __user *fr_pvc_info;

		
		void __user *sync;
		void __user *te1;
	} ifs_ifsu;
};

struct vki_ifreq 
{
#define VKI_IFHWADDRLEN	6
	union
	{
		char	ifrn_name[VKI_IFNAMSIZ];		
	} ifr_ifrn;
	
	union {
		struct	vki_sockaddr ifru_addr;
		struct	vki_sockaddr ifru_dstaddr;
		struct	vki_sockaddr ifru_broadaddr;
		struct	vki_sockaddr ifru_netmask;
		struct  vki_sockaddr ifru_hwaddr;
		short	ifru_flags;
		int	ifru_ivalue;
		int	ifru_mtu;
		struct  vki_ifmap ifru_map;
		char	ifru_slave[VKI_IFNAMSIZ];	
		char	ifru_newname[VKI_IFNAMSIZ];
		void __user *	ifru_data;
		struct	vki_if_settings ifru_settings;
	} ifr_ifru;
};

#define vki_ifr_name	ifr_ifrn.ifrn_name	
#define ifr_hwaddr	ifr_ifru.ifru_hwaddr	
#define	ifr_addr	ifr_ifru.ifru_addr	
#define	ifr_dstaddr	ifr_ifru.ifru_dstaddr	
#define	ifr_broadaddr	ifr_ifru.ifru_broadaddr	
#define	ifr_netmask	ifr_ifru.ifru_netmask	
#define	vki_ifr_flags	ifr_ifru.ifru_flags	
#define	vki_ifr_metric	ifr_ifru.ifru_ivalue	
#define	vki_ifr_mtu		ifr_ifru.ifru_mtu	
#define ifr_map		ifr_ifru.ifru_map	
#define ifr_slave	ifr_ifru.ifru_slave	
#define	vki_ifr_data	ifr_ifru.ifru_data	
#define vki_ifr_ifindex	ifr_ifru.ifru_ivalue	
#define ifr_bandwidth	ifr_ifru.ifru_ivalue    
#define ifr_qlen	ifr_ifru.ifru_ivalue	
#define ifr_newname	ifr_ifru.ifru_newname	
#define ifr_settings	ifr_ifru.ifru_settings	

struct vki_ifconf 
{
	int	ifc_len;			
	union 
	{
		char __user *ifcu_buf;
		struct vki_ifreq __user *ifcu_req;
	} ifc_ifcu;
};
#define	vki_ifc_buf	ifc_ifcu.ifcu_buf	


struct vki_arpreq {
  struct vki_sockaddr	arp_pa;		
  struct vki_sockaddr	arp_ha;		
  int			arp_flags;	
  struct vki_sockaddr   arp_netmask;    
  char			arp_dev[16];
};


struct vki_rtentry 
{
	unsigned long	rt_pad1;
	struct vki_sockaddr	rt_dst;		
	struct vki_sockaddr	rt_gateway;	
	struct vki_sockaddr	rt_genmask;	
	unsigned short	rt_flags;
	short		rt_pad2;
	unsigned long	rt_pad3;
	void		*rt_pad4;
	short		rt_metric;	
	char __user	*rt_dev;	
	unsigned long	rt_mtu;		
	unsigned long	rt_window;	
	unsigned short	rt_irtt;	
};


typedef __vki_s32 vki_sctp_assoc_t;

enum vki_sctp_optname {
	VKI_SCTP_RTOINFO,
#define VKI_SCTP_RTOINFO VKI_SCTP_RTOINFO
	VKI_SCTP_ASSOCINFO,
#define VKI_SCTP_ASSOCINFO VKI_SCTP_ASSOCINFO
	VKI_SCTP_INITMSG,
#define VKI_SCTP_INITMSG VKI_SCTP_INITMSG
	VKI_SCTP_NODELAY, 	
#define VKI_SCTP_NODELAY	VKI_SCTP_NODELAY
	VKI_SCTP_AUTOCLOSE,
#define VKI_SCTP_AUTOCLOSE VKI_SCTP_AUTOCLOSE
	VKI_SCTP_SET_PEER_PRIMARY_ADDR, 
#define VKI_SCTP_SET_PEER_PRIMARY_ADDR VKI_SCTP_SET_PEER_PRIMARY_ADDR
	VKI_SCTP_PRIMARY_ADDR,
#define VKI_SCTP_PRIMARY_ADDR VKI_SCTP_PRIMARY_ADDR
	VKI_SCTP_ADAPTION_LAYER,      
#define VKI_SCTP_ADAPTION_LAYER VKI_SCTP_ADAPTION_LAYER
	VKI_SCTP_DISABLE_FRAGMENTS,
#define VKI_SCTP_DISABLE_FRAGMENTS VKI_SCTP_DISABLE_FRAGMENTS
	VKI_SCTP_PEER_ADDR_PARAMS,
#define VKI_SCTP_PEER_ADDR_PARAMS VKI_SCTP_PEER_ADDR_PARAMS
	VKI_SCTP_DEFAULT_SEND_PARAM,
#define VKI_SCTP_DEFAULT_SEND_PARAM VKI_SCTP_DEFAULT_SEND_PARAM
	VKI_SCTP_EVENTS,
#define VKI_SCTP_EVENTS VKI_SCTP_EVENTS
	VKI_SCTP_I_WANT_MAPPED_V4_ADDR,  
#define VKI_SCTP_I_WANT_MAPPED_V4_ADDR VKI_SCTP_I_WANT_MAPPED_V4_ADDR
	VKI_SCTP_MAXSEG, 	
#define VKI_SCTP_MAXSEG 	VKI_SCTP_MAXSEG
	VKI_SCTP_STATUS,
#define VKI_SCTP_STATUS VKI_SCTP_STATUS
	VKI_SCTP_GET_PEER_ADDR_INFO,
#define VKI_SCTP_GET_PEER_ADDR_INFO VKI_SCTP_GET_PEER_ADDR_INFO

	VKI_SCTP_SOCKOPT_BINDX_ADD = 100,
#define VKI_SCTP_SOCKOPT_BINDX_ADD	VKI_SCTP_SOCKOPT_BINDX_ADD
	VKI_SCTP_SOCKOPT_BINDX_REM, 
#define VKI_SCTP_SOCKOPT_BINDX_REM	VKI_SCTP_SOCKOPT_BINDX_REM
	VKI_SCTP_SOCKOPT_PEELOFF, 	
#define VKI_SCTP_SOCKOPT_PEELOFF	VKI_SCTP_SOCKOPT_PEELOFF
	VKI_SCTP_GET_PEER_ADDRS_NUM, 	
#define VKI_SCTP_GET_PEER_ADDRS_NUM	VKI_SCTP_GET_PEER_ADDRS_NUM
	VKI_SCTP_GET_PEER_ADDRS, 	
#define VKI_SCTP_GET_PEER_ADDRS	VKI_SCTP_GET_PEER_ADDRS
	VKI_SCTP_GET_LOCAL_ADDRS_NUM, 	
#define VKI_SCTP_GET_LOCAL_ADDRS_NUM	VKI_SCTP_GET_LOCAL_ADDRS_NUM
	VKI_SCTP_GET_LOCAL_ADDRS, 	
#define VKI_SCTP_GET_LOCAL_ADDRS	VKI_SCTP_GET_LOCAL_ADDRS
	VKI_SCTP_SOCKOPT_CONNECTX, 
#define VKI_SCTP_SOCKOPT_CONNECTX	VKI_SCTP_SOCKOPT_CONNECTX
};

struct vki_sctp_getaddrs {
	vki_sctp_assoc_t        assoc_id;
	int			addr_num;
	struct vki_sockaddr	*addrs;
};


struct	vki_rusage {
	struct vki_timeval ru_utime;	
	struct vki_timeval ru_stime;	
	long	ru_maxrss;		
	long	ru_ixrss;		
	long	ru_idrss;		
	long	ru_isrss;		
	long	ru_minflt;		
	long	ru_majflt;		
	long	ru_nswap;		
	long	ru_inblock;		
	long	ru_oublock;		
	long	ru_msgsnd;		
	long	ru_msgrcv;		
	long	ru_nsignals;		
	long	ru_nvcsw;		
	long	ru_nivcsw;		
};

struct vki_rlimit {
	unsigned long	rlim_cur;
	unsigned long	rlim_max;
};

struct vki_rlimit64 {
	__vki_u64 rlim_cur;
	__vki_u64 rlim_max;
};


struct vki_elf_siginfo
{
	int	si_signo;			
	int	si_code;			
	int	si_errno;			
};

struct vki_elf_prstatus
{
	struct vki_elf_siginfo pr_info;	
	short	pr_cursig;		
	unsigned long pr_sigpend;	
	unsigned long pr_sighold;	
	vki_pid_t	pr_pid;
	vki_pid_t	pr_ppid;
	vki_pid_t	pr_pgrp;
	vki_pid_t	pr_sid;
	struct vki_timeval pr_utime;	
	struct vki_timeval pr_stime;	
	struct vki_timeval pr_cutime;	
	struct vki_timeval pr_cstime;	
	vki_elf_gregset_t pr_reg;	
	int pr_fpvalid;		
};

#define VKI_ELF_PRARGSZ	(80)	

struct vki_elf_prpsinfo
{
	char	pr_state;	
	char	pr_sname;	
	char	pr_zomb;	
	char	pr_nice;	
	unsigned long pr_flag;	
	__vki_kernel_uid_t	pr_uid;
	__vki_kernel_gid_t	pr_gid;
	vki_pid_t	pr_pid, pr_ppid, pr_pgrp, pr_sid;
	
	char	pr_fname[16];	
	char	pr_psargs[VKI_ELF_PRARGSZ];	
};


#define VKI_EPOLL_CTL_ADD 1
#define VKI_EPOLL_CTL_DEL 2
#define VKI_EPOLL_CTL_MOD 3

#ifdef __x86_64__
#define VKI_EPOLL_PACKED __attribute__((packed))
#else
#define VKI_EPOLL_PACKED
#endif

struct vki_epoll_event {
	__vki_u32 events;
	__vki_u64 data;
} VKI_EPOLL_PACKED;



struct vki_mq_attr {
	long	mq_flags;	
	long	mq_maxmsg;	
	long	mq_msgsize;	
	long	mq_curmsgs;	
	long	__reserved[4];	
};


struct vki_new_utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
	char domainname[65];
};


struct vki_mii_ioctl_data {
	vki_u16		phy_id;
	vki_u16		reg_num;
	vki_u16		val_in;
	vki_u16		val_out;
};



typedef struct __vki_user_cap_header_struct {
	__vki_u32 version;
	int pid;
} __user *vki_cap_user_header_t;
 
typedef struct __vki_user_cap_data_struct {
        __vki_u32 effective;
        __vki_u32 permitted;
        __vki_u32 inheritable;
} __user *vki_cap_user_data_t;
  




struct vki_ipc_perm
{
	__vki_kernel_key_t	key;
	__vki_kernel_uid_t	uid;
	__vki_kernel_gid_t	gid;
	__vki_kernel_uid_t	cuid;
	__vki_kernel_gid_t	cgid;
	__vki_kernel_mode_t	mode; 
	unsigned short	seq;
};

#define VKI_IPC_CREAT  00001000   
#define VKI_IPC_EXCL   00002000   
#define VKI_IPC_NOWAIT 00004000   

#define VKI_IPC_SET  1     
#define VKI_IPC_STAT 2     
#define VKI_IPC_INFO 3     

#define VKI_IPC_64  0x0100  
# define VKI_SHM_HUGETLB   04000



#define VKI_GETALL  13       
#define VKI_SETVAL  16       
#define VKI_SETALL  17       

#define VKI_SEM_STAT 18
#define VKI_SEM_INFO 19

struct vki_semid_ds {
	struct vki_ipc_perm	sem_perm;		
	__vki_kernel_time_t	sem_otime;		
	__vki_kernel_time_t	sem_ctime;		
        
	void*sem_base;		
	void*sem_pending;		
	void**sem_pending_last;	
	void*undo;			
	unsigned short	sem_nsems;		
};

struct vki_sembuf {
	unsigned short  sem_num;	
	short		sem_op;		
	short		sem_flg;	
};

union vki_semun {
	int val;			
	struct vki_semid_ds __user *buf;	
	unsigned short __user *array;	
	struct vki_seminfo __user *__buf;	
	void __user *__pad;
};

struct  vki_seminfo {
	int semmap;
	int semmni;
	int semmns;
	int semmnu;
	int semmsl;
	int semopm;
	int semume;
	int semusz;
	int semvmx;
	int semaem;
};


#define	VKI_EPERM		 1	
#define	VKI_ENOENT		 2	
#define	VKI_ESRCH		 3	
#define	VKI_EINTR		 4	
#define	VKI_EIO			 5	
#define	VKI_ENXIO		 6	
#define	VKI_E2BIG		 7	
#define	VKI_ENOEXEC		 8	
#define	VKI_EBADF		 9	
#define	VKI_ECHILD		10	
#define	VKI_EAGAIN		11	
#define	VKI_ENOMEM		12	
#define	VKI_EACCES		13	
#define	VKI_EFAULT		14	
#define	VKI_ENOTBLK		15	
#define	VKI_EBUSY		16	
#define	VKI_EEXIST		17	
#define	VKI_EXDEV		18	
#define	VKI_ENODEV		19	
#define	VKI_ENOTDIR		20	
#define	VKI_EISDIR		21	
#define	VKI_EINVAL		22	
#define	VKI_ENFILE		23	
#define	VKI_EMFILE		24	
#define	VKI_ENOTTY		25	
#define	VKI_ETXTBSY		26	
#define	VKI_EFBIG		27	
#define	VKI_ENOSPC		28	
#define	VKI_ESPIPE		29	
#define	VKI_EROFS		30	
#define	VKI_EMLINK		31	
#define	VKI_EPIPE		32	
#define	VKI_EDOM		33	
#define	VKI_ERANGE		34	


#define VKI_EWOULDBLOCK		VKI_EAGAIN


#define VKI_WNOHANG	0x00000001

#define __VKI_WALL	0x40000000	
#define __VKI_WCLONE	0x80000000	


#define VKI_MREMAP_MAYMOVE	1
#define VKI_MREMAP_FIXED	2


#define VKI_FUTEX_WAIT (0)
#define VKI_FUTEX_WAKE (1)
#define VKI_FUTEX_FD (2)
#define VKI_FUTEX_REQUEUE (3)
#define VKI_FUTEX_CMP_REQUEUE (4)
#define VKI_FUTEX_WAKE_OP (5)
#define VKI_FUTEX_LOCK_PI (6)
#define VKI_FUTEX_UNLOCK_PI (7)
#define VKI_FUTEX_TRYLOCK_PI (8)
#define VKI_FUTEX_WAIT_BITSET (9)
#define VKI_FUTEX_WAKE_BITSET (10)
#define VKI_FUTEX_WAIT_REQUEUE_PI (11)
#define VKI_FUTEX_CMP_REQUEUE_PI (12)
#define VKI_FUTEX_PRIVATE_FLAG (128)
#define VKI_FUTEX_CLOCK_REALTIME (256)

struct vki_robust_list {
	struct vki_robust_list __user *next;
};

struct vki_robust_list_head {
	struct vki_robust_list list;

	long futex_offset;

	struct vki_robust_list __user *list_op_pending;
};


#define VKI_ERESTARTSYS	512


#define VKI_S_IFMT  00170000
#define VKI_S_IFSOCK 0140000
#define VKI_S_IFLNK  0120000
#define VKI_S_IFREG  0100000
#define VKI_S_IFBLK  0060000
#define VKI_S_IFDIR  0040000
#define VKI_S_IFCHR  0020000
#define VKI_S_IFIFO  0010000
#define VKI_S_ISUID  0004000
#define VKI_S_ISGID  0002000
#define VKI_S_ISVTX  0001000

#define VKI_S_ISLNK(m)	(((m) & VKI_S_IFMT) == VKI_S_IFLNK)
#define VKI_S_ISREG(m)	(((m) & VKI_S_IFMT) == VKI_S_IFREG)
#define VKI_S_ISDIR(m)	(((m) & VKI_S_IFMT) == VKI_S_IFDIR)
#define VKI_S_ISCHR(m)	(((m) & VKI_S_IFMT) == VKI_S_IFCHR)
#define VKI_S_ISBLK(m)	(((m) & VKI_S_IFMT) == VKI_S_IFBLK)
#define VKI_S_ISFIFO(m)	(((m) & VKI_S_IFMT) == VKI_S_IFIFO)
#define VKI_S_ISSOCK(m)	(((m) & VKI_S_IFMT) == VKI_S_IFSOCK)

#define VKI_S_IRWXU 00700
#define VKI_S_IRUSR 00400
#define VKI_S_IWUSR 00200
#define VKI_S_IXUSR 00100

#define VKI_S_IRWXG 00070
#define VKI_S_IRGRP 00040
#define VKI_S_IWGRP 00020
#define VKI_S_IXGRP 00010

#define VKI_S_IRWXO 00007
#define VKI_S_IROTH 00004
#define VKI_S_IWOTH 00002
#define VKI_S_IXOTH 00001


struct vki_dirent {
	long		d_ino;
	__vki_kernel_off_t	d_off;
	unsigned short	d_reclen;
	char		d_name[256]; 
};

struct vki_dirent64 {
	__vki_u64 d_ino;
	__vki_s64 d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[256]; 
};


#define VKI_F_SETLEASE      (VKI_F_LINUX_SPECIFIC_BASE + 0)
#define VKI_F_GETLEASE      (VKI_F_LINUX_SPECIFIC_BASE + 1)

#define VKI_F_CANCELLK      (VKI_F_LINUX_SPECIFIC_BASE + 5)

#define VKI_F_DUPFD_CLOEXEC (VKI_F_LINUX_SPECIFIC_BASE + 6)

#define VKI_F_NOTIFY        (VKI_F_LINUX_SPECIFIC_BASE + 2)

#define VKI_F_SETPIPE_SZ    (VKI_F_LINUX_SPECIFIC_BASE + 7)
#define VKI_F_GETPIPE_SZ    (VKI_F_LINUX_SPECIFIC_BASE + 8)


struct __vki_sysctl_args {
	int __user *name;
	int nlen;
	void __user *oldval;
	vki_size_t __user *oldlenp;
	void __user *newval;
	vki_size_t newlen;
	unsigned long __unused0[4];
};


typedef unsigned long	vki_aio_context_t;

enum {
	VKI_IOCB_CMD_PREAD = 0,
	VKI_IOCB_CMD_PWRITE = 1,
	VKI_IOCB_CMD_FSYNC = 2,
	VKI_IOCB_CMD_FDSYNC = 3,
	VKI_IOCB_CMD_PREADV = 7,
	VKI_IOCB_CMD_PWRITEV = 8,
};

struct vki_io_event {
	__vki_u64	data;		
	__vki_u64	obj;		
        
        
	__vki_s64	result;		
	__vki_s64	result2;	
};

#if defined(VKI_LITTLE_ENDIAN)
#  define VKI_PADDED(x,y)	x, y
#elif defined(VKI_BIG_ENDIAN)
#  define VKI_PADDED(x,y)	y, x
#else
#error edit for your odd byteorder.
#endif

struct vki_iocb {
	
	__vki_u64	aio_data;	
	__vki_u32	VKI_PADDED(aio_key, aio_reserved1);
				

	
	__vki_u16	aio_lio_opcode;	
	__vki_s16	aio_reqprio;
	__vki_u32	aio_fildes;

	__vki_u64	aio_buf;
	__vki_u64	aio_nbytes;
	__vki_s64	aio_offset;

	
	__vki_u64	aio_reserved2;	
	__vki_u64	aio_reserved3;
}; 


struct vki_aio_ring {
	unsigned	id;	
	unsigned	nr;	
	unsigned	head;
	unsigned	tail;

	unsigned	magic;
	unsigned	compat_features;
	unsigned	incompat_features;
	unsigned	header_length;	

	struct vki_io_event		io_events[0];
}; 


#define VKI_MSG_STAT 11
#define VKI_MSG_INFO 12

struct vki_msqid_ds {
	struct vki_ipc_perm msg_perm;
	struct vki_msg *msg_first;		
	struct vki_msg *msg_last;		
	__vki_kernel_time_t msg_stime;	
	__vki_kernel_time_t msg_rtime;	
	__vki_kernel_time_t msg_ctime;	
	unsigned long  msg_lcbytes;	
	unsigned long  msg_lqbytes;	
	unsigned short msg_cbytes;	
	unsigned short msg_qnum;	
	unsigned short msg_qbytes;	
	__vki_kernel_ipc_pid_t msg_lspid;	
	__vki_kernel_ipc_pid_t msg_lrpid;	
};

struct vki_msgbuf {
	long mtype;         
	char mtext[1];      
};

struct vki_msginfo {
	int msgpool;
	int msgmap; 
	int msgmax; 
	int msgmnb; 
	int msgmni; 
	int msgssz; 
	int msgtql; 
	unsigned short  msgseg; 
};


struct vki_shmid_ds {
	struct vki_ipc_perm		shm_perm;	
	int			shm_segsz;	
	__vki_kernel_time_t		shm_atime;	
	__vki_kernel_time_t		shm_dtime;	
	__vki_kernel_time_t		shm_ctime;	
	__vki_kernel_ipc_pid_t	shm_cpid;	
	__vki_kernel_ipc_pid_t	shm_lpid;	
	unsigned short		shm_nattch;	
	unsigned short 		shm_unused;	
	void 			*shm_unused2;	
	void			*shm_unused3;	
};

#define VKI_SHM_RDONLY  010000  
#define VKI_SHM_RND     020000  

#define VKI_SHM_STAT 	13
#define VKI_SHM_INFO 	14

struct	vki_shminfo {
	int shmmax;
	int shmmin;
	int shmmni;
	int shmseg;
	int shmall;
};

struct vki_shm_info {
	int used_ids;
	unsigned long shm_tot;	
	unsigned long shm_rss;	
	unsigned long shm_swp;	
	unsigned long swap_attempts;
	unsigned long swap_successes;
};


struct vki_rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

#define VKI_RTC_AIE_ON	_VKI_IO('p', 0x01)	
#define VKI_RTC_AIE_OFF	_VKI_IO('p', 0x02)	
#define VKI_RTC_UIE_ON	_VKI_IO('p', 0x03)	
#define VKI_RTC_UIE_OFF	_VKI_IO('p', 0x04)	
#define VKI_RTC_PIE_ON	_VKI_IO('p', 0x05)	
#define VKI_RTC_PIE_OFF	_VKI_IO('p', 0x06)	

#define VKI_RTC_ALM_SET		_VKI_IOW('p', 0x07, struct vki_rtc_time) 
#define VKI_RTC_ALM_READ	_VKI_IOR('p', 0x08, struct vki_rtc_time) 
#define VKI_RTC_RD_TIME		_VKI_IOR('p', 0x09, struct vki_rtc_time) 
#define VKI_RTC_IRQP_READ	_VKI_IOR('p', 0x0b, unsigned long)	 
#define VKI_RTC_IRQP_SET	_VKI_IOW('p', 0x0c, unsigned long)	 


#define VKI_ISDN_MAX_CHANNELS   64

#define VKI_IIOCGETCPS  _VKI_IO('I',21)

#define VKI_IIOCNETGPN  _VKI_IO('I',34)

#define VKI_ISDN_MSNLEN          32

typedef struct {
  char name[10];
  char phone[VKI_ISDN_MSNLEN];
  int  outgoing;
} vki_isdn_net_ioctl_phone;


#define VKI_SIOCOUTQ		VKI_TIOCOUTQ

#define VKI_SIOCADDRT		0x890B	
#define VKI_SIOCDELRT		0x890C	

#define VKI_SIOCGIFNAME		0x8910	
#define VKI_SIOCGIFCONF		0x8912	
#define VKI_SIOCGIFFLAGS	0x8913	
#define VKI_SIOCSIFFLAGS	0x8914	
#define VKI_SIOCGIFADDR		0x8915	
#define VKI_SIOCSIFADDR		0x8916	
#define VKI_SIOCGIFDSTADDR	0x8917	
#define VKI_SIOCSIFDSTADDR	0x8918	
#define VKI_SIOCGIFBRDADDR	0x8919	
#define VKI_SIOCSIFBRDADDR	0x891a	
#define VKI_SIOCGIFNETMASK	0x891b	
#define VKI_SIOCSIFNETMASK	0x891c	
#define VKI_SIOCGIFMETRIC	0x891d	
#define VKI_SIOCSIFMETRIC	0x891e	
#define VKI_SIOCGIFMTU		0x8921	
#define VKI_SIOCSIFMTU		0x8922	
#define	VKI_SIOCSIFHWADDR	0x8924	
#define VKI_SIOCGIFHWADDR	0x8927	
#define VKI_SIOCGIFINDEX	0x8933	

#define VKI_SIOCGIFTXQLEN	0x8942	
#define VKI_SIOCSIFTXQLEN	0x8943	

#define VKI_SIOCETHTOOL		0x8946	

#define VKI_SIOCGMIIPHY		0x8947	
#define VKI_SIOCGMIIREG		0x8948	
#define VKI_SIOCSMIIREG		0x8949	

#define VKI_SIOCDARP		0x8953	
#define VKI_SIOCGARP		0x8954	
#define VKI_SIOCSARP		0x8955	

#define VKI_SIOCDRARP		0x8960	
#define VKI_SIOCGRARP		0x8961	
#define VKI_SIOCSRARP		0x8962	

#define VKI_SIOCGIFMAP		0x8970	
#define VKI_SIOCSIFMAP		0x8971	

#define VKI_SIOCSHWTSTAMP	0x89B0	


#define VKI_PP_MAJOR	99

#define VKI_PP_IOCTL	'p'

#define VKI_PPSETMODE	_VKI_IOW(VKI_PP_IOCTL, 0x80, int)

#define VKI_PPRSTATUS	_VKI_IOR(VKI_PP_IOCTL, 0x81, unsigned char)

#define VKI_PPRCONTROL	_VKI_IOR(VKI_PP_IOCTL, 0x83, unsigned char)
#define VKI_PPWCONTROL	_VKI_IOW(VKI_PP_IOCTL, 0x84, unsigned char)

struct vki_ppdev_frob_struct {
	unsigned char mask;
	unsigned char val;
};
#define VKI_PPFCONTROL      _VKI_IOW(VKI_PP_IOCTL, 0x8e, struct vki_ppdev_frob_struct)

#define VKI_PPRDATA		_VKI_IOR(VKI_PP_IOCTL, 0x85, unsigned char)
#define VKI_PPWDATA		_VKI_IOW(VKI_PP_IOCTL, 0x86, unsigned char)

#define VKI_PPCLAIM		_VKI_IO(VKI_PP_IOCTL, 0x8b)

#define VKI_PPRELEASE	_VKI_IO(VKI_PP_IOCTL, 0x8c)

#define VKI_PPYIELD		_VKI_IO(VKI_PP_IOCTL, 0x8d)

#define VKI_PPEXCL		_VKI_IO(VKI_PP_IOCTL, 0x8f)

#define VKI_PPDATADIR	_VKI_IOW(VKI_PP_IOCTL, 0x90, int)

#define VKI_PPNEGOT	_VKI_IOW(VKI_PP_IOCTL, 0x91, int)

#define VKI_PPWCTLONIRQ	_VKI_IOW(VKI_PP_IOCTL, 0x92, unsigned char)

#define VKI_PPCLRIRQ	_VKI_IOR(VKI_PP_IOCTL, 0x93, int)

#define VKI_PPSETPHASE	_VKI_IOW(VKI_PP_IOCTL, 0x94, int)

#define VKI_PPGETTIME	_VKI_IOR(VKI_PP_IOCTL, 0x95, struct vki_timeval)
#define VKI_PPSETTIME	_VKI_IOW(VKI_PP_IOCTL, 0x96, struct vki_timeval)

#define VKI_PPGETMODES	_VKI_IOR(VKI_PP_IOCTL, 0x97, unsigned int)

#define VKI_PPGETMODE	_VKI_IOR(VKI_PP_IOCTL, 0x98, int)
#define VKI_PPGETPHASE	_VKI_IOR(VKI_PP_IOCTL, 0x99, int)

#define VKI_PPGETFLAGS	_VKI_IOR(VKI_PP_IOCTL, 0x9a, int)
#define VKI_PPSETFLAGS	_VKI_IOW(VKI_PP_IOCTL, 0x9b, int)


#define VKI_BLKROSET   _VKI_IO(0x12,93)	
#define VKI_BLKROGET   _VKI_IO(0x12,94)	
#define VKI_BLKGETSIZE _VKI_IO(0x12,96) 
#define VKI_BLKRASET   _VKI_IO(0x12,98)	
#define VKI_BLKRAGET   _VKI_IO(0x12,99)	
#define VKI_BLKFRASET  _VKI_IO(0x12,100)
#define VKI_BLKFRAGET  _VKI_IO(0x12,101)
#define VKI_BLKSECTGET _VKI_IO(0x12,103)
#define VKI_BLKSSZGET  _VKI_IO(0x12,104)
#define VKI_BLKBSZGET  _VKI_IOR(0x12,112,vki_size_t)
#define VKI_BLKBSZSET  _VKI_IOW(0x12,113,vki_size_t)
#define VKI_BLKGETSIZE64 _VKI_IOR(0x12,114,vki_size_t) 
#define VKI_BLKPBSZGET _VKI_IO(0x12,123)
#define VKI_BLKDISCARDZEROES _VKI_IO(0x12,124)

#define VKI_FIBMAP	_VKI_IO(0x00,1)	
#define VKI_FIGETBSZ    _VKI_IO(0x00,2)	


typedef struct vki_sg_io_hdr
{
    int interface_id;           
    int dxfer_direction;        
    unsigned char cmd_len;      
    unsigned char mx_sb_len;    
    unsigned short iovec_count; 
    unsigned int dxfer_len;     
    void __user *dxferp;	
    unsigned char __user *cmdp; 
    void __user *sbp;		
    unsigned int timeout;       
    unsigned int flags;         
    int pack_id;                
    void __user * usr_ptr;      
    unsigned char status;       
    unsigned char masked_status;
    unsigned char msg_status;   
    unsigned char sb_len_wr;    /* [o] byte count actually written to sbp */
    unsigned short host_status; 
    unsigned short driver_status;
    int resid;                  
    unsigned int duration;      
    unsigned int info;          
} vki_sg_io_hdr_t;  

#define VKI_SG_DXFER_NONE -1        
#define VKI_SG_DXFER_TO_DEV -2      
#define VKI_SG_DXFER_FROM_DEV -3    
#define VKI_SG_DXFER_TO_FROM_DEV -4 

typedef struct vki_sg_scsi_id { 
    int host_no;        
    int channel;
    int scsi_id;        
    int lun;
    int scsi_type;      
    short h_cmd_per_lun;
    short d_queue_depth;
    int unused[2];      
} vki_sg_scsi_id_t; 

#define VKI_SG_EMULATED_HOST 0x2203 

#define VKI_SG_SET_RESERVED_SIZE 0x2275  
#define VKI_SG_GET_RESERVED_SIZE 0x2272  

#define VKI_SG_GET_SCSI_ID 0x2276   

#define VKI_SG_GET_SG_TABLESIZE 0x227F  

#define VKI_SG_GET_VERSION_NUM 0x2282 

#define VKI_SG_IO 0x2285   

#define VKI_SG_SET_TIMEOUT 0x2201  
#define VKI_SG_GET_TIMEOUT 0x2202  

#define VKI_SG_SET_COMMAND_Q 0x2271   


#define VKI_SCSI_IOCTL_DOORLOCK		0x5380 
#define VKI_SCSI_IOCTL_DOORUNLOCK	0x5381 
#define VKI_SCSI_IOCTL_GET_IDLUN	0x5382
#define VKI_SCSI_IOCTL_GET_BUS_NUMBER	0x5386

struct vki_scsi_idlun {
	__vki_u32 dev_id;
	__vki_u32 host_unique_id;
};


#define VKI_CDROMPLAYMSF		0x5303 
#define VKI_CDROMREADTOCHDR		0x5305 
#define VKI_CDROMREADTOCENTRY		0x5306 
#define VKI_CDROMSUBCHNL		0x530b 
#define VKI_CDROMREADMODE2		0x530c 
#define VKI_CDROMREADAUDIO		0x530e 
#define VKI_CDROMMULTISESSION		0x5310 
#define VKI_CDROM_GET_MCN		0x5311 
#define VKI_CDROMVOLREAD		0x5313 
#define VKI_CDROMREADRAW		0x5314	
#define VKI_CDROM_CLEAR_OPTIONS		0x5321  
#define VKI_CDROM_DRIVE_STATUS		0x5326  
#define VKI_CDROM_DISC_STATUS		0x5327	
#define VKI_CDROM_GET_CAPABILITY	0x5331	

#define VKI_CDROM_SEND_PACKET		0x5393	

struct vki_cdrom_msf0		
{
	__vki_u8	minute;
	__vki_u8	second;
	__vki_u8	frame;
};

union vki_cdrom_addr		
{
	struct vki_cdrom_msf0	msf;
	int			lba;
};

struct vki_cdrom_msf 
{
	__vki_u8	cdmsf_min0;	
	__vki_u8	cdmsf_sec0;	
	__vki_u8	cdmsf_frame0;	
	__vki_u8	cdmsf_min1;	
	__vki_u8	cdmsf_sec1;	
	__vki_u8	cdmsf_frame1;	
};

struct vki_cdrom_tochdr 	
{
	__vki_u8	cdth_trk0;	
	__vki_u8	cdth_trk1;	
};

struct vki_cdrom_volctrl
{
	__vki_u8	channel0;
	__vki_u8	channel1;
	__vki_u8	channel2;
	__vki_u8	channel3;
};

struct vki_cdrom_subchnl 
{
	__vki_u8	cdsc_format;
	__vki_u8	cdsc_audiostatus;
	__vki_u8	cdsc_adr:	4;
	__vki_u8	cdsc_ctrl:	4;
	__vki_u8	cdsc_trk;
	__vki_u8	cdsc_ind;
	union vki_cdrom_addr cdsc_absaddr;
	union vki_cdrom_addr cdsc_reladdr;
};

struct vki_cdrom_tocentry 
{
	__vki_u8	cdte_track;
	__vki_u8	cdte_adr	:4;
	__vki_u8	cdte_ctrl	:4;
	__vki_u8	cdte_format;
	union vki_cdrom_addr cdte_addr;
	__vki_u8	cdte_datamode;
};

struct vki_cdrom_read      
{
	int	cdread_lba;
	char 	*cdread_bufaddr;
	int	cdread_buflen;
};

struct vki_cdrom_read_audio
{
	union vki_cdrom_addr addr; 
	__vki_u8 addr_format;      
	int nframes;           
	__vki_u8 __user *buf;      
};

struct vki_cdrom_multisession
{
	union vki_cdrom_addr addr; 
	__vki_u8 xa_flag;        
	__vki_u8 addr_format;    
};

struct vki_cdrom_mcn 
{
  __vki_u8 medium_catalog_number[14]; 
};

#define VKI_CDROM_PACKET_SIZE	12

struct vki_cdrom_generic_command
{
	unsigned char 		cmd[VKI_CDROM_PACKET_SIZE];
	unsigned char		__user *buffer;
	unsigned int 		buflen;
	int			stat;
        
	void__user *sense;
	unsigned char		data_direction;
	int			quiet;
	int			timeout;
	void			__user *reserved[1];	
};

#define VKI_CD_SYNC_SIZE         12 
#define VKI_CD_HEAD_SIZE          4 
#define VKI_CD_FRAMESIZE_RAW   2352 
#define VKI_CD_FRAMESIZE_RAW0 (VKI_CD_FRAMESIZE_RAW-VKI_CD_SYNC_SIZE-VKI_CD_HEAD_SIZE) 


#ifndef _VKI_SIOWR
#if defined(_VKI_IOWR) && (defined(_AIX) || (!defined(sun) && !defined(sparc) && !defined(__sparc__) && !defined(__INCioctlh) && !defined(__Lynx__)))
#define	_VKI_SIO		_VKI_IO
#define	_VKI_SIOR		_VKI_IOR
#define	_VKI_SIOW		_VKI_IOW
#define	_VKI_SIOWR		_VKI_IOWR
#else
#error Valgrind: Cannot handle sparc/sun case yet...
#  endif 
#endif  

#define VKI_SNDCTL_SEQ_CTRLRATE		_VKI_SIOWR('Q', 3, int)	
#define VKI_SNDCTL_SEQ_GETOUTCOUNT	_VKI_SIOR ('Q', 4, int)
#define VKI_SNDCTL_SEQ_GETINCOUNT	_VKI_SIOR ('Q', 5, int)
#define VKI_SNDCTL_SEQ_PERCMODE		_VKI_SIOW ('Q', 6, int)
#define VKI_SNDCTL_SEQ_TESTMIDI		_VKI_SIOW ('Q', 8, int)
#define VKI_SNDCTL_SEQ_RESETSAMPLES	_VKI_SIOW ('Q', 9, int)
#define VKI_SNDCTL_SEQ_NRSYNTHS		_VKI_SIOR ('Q',10, int)
#define VKI_SNDCTL_SEQ_NRMIDIS		_VKI_SIOR ('Q',11, int)
#define VKI_SNDCTL_SEQ_GETTIME		_VKI_SIOR ('Q',19, int)

#define VKI_SNDCTL_TMR_TIMEBASE		_VKI_SIOWR('T', 1, int)
#define VKI_SNDCTL_TMR_TEMPO		_VKI_SIOWR('T', 5, int)
#define VKI_SNDCTL_TMR_SOURCE		_VKI_SIOWR('T', 6, int)

#define VKI_SNDCTL_MIDI_PRETIME		_VKI_SIOWR('m', 0, int)
#define VKI_SNDCTL_MIDI_MPUMODE		_VKI_SIOWR('m', 1, int)

#define VKI_SNDCTL_DSP_RESET		_VKI_SIO  ('P', 0)
#define VKI_SNDCTL_DSP_SYNC		_VKI_SIO  ('P', 1)
#define VKI_SNDCTL_DSP_SPEED		_VKI_SIOWR('P', 2, int)
#define VKI_SNDCTL_DSP_STEREO		_VKI_SIOWR('P', 3, int)
#define VKI_SNDCTL_DSP_GETBLKSIZE	_VKI_SIOWR('P', 4, int)
#define VKI_SNDCTL_DSP_CHANNELS		_VKI_SIOWR('P', 6, int)
#define VKI_SOUND_PCM_WRITE_FILTER	_VKI_SIOWR('P', 7, int)
#define VKI_SNDCTL_DSP_POST		_VKI_SIO  ('P', 8)
#define VKI_SNDCTL_DSP_SUBDIVIDE	_VKI_SIOWR('P', 9, int)
#define VKI_SNDCTL_DSP_SETFRAGMENT	_VKI_SIOWR('P',10, int)

#define VKI_SNDCTL_DSP_GETFMTS		_VKI_SIOR ('P',11, int) 
#define VKI_SNDCTL_DSP_SETFMT		_VKI_SIOWR('P', 5, int) 

typedef struct vki_audio_buf_info {
			int fragments;	
			int fragstotal;	
			int fragsize;	

			int bytes;	
			
		} vki_audio_buf_info;

#define VKI_SNDCTL_DSP_GETOSPACE	_VKI_SIOR ('P',12, vki_audio_buf_info)
#define VKI_SNDCTL_DSP_GETISPACE	_VKI_SIOR ('P',13, vki_audio_buf_info)
#define VKI_SNDCTL_DSP_NONBLOCK		_VKI_SIO  ('P',14)
#define VKI_SNDCTL_DSP_GETCAPS		_VKI_SIOR ('P',15, int)

#define VKI_SNDCTL_DSP_GETTRIGGER	_VKI_SIOR ('P',16, int)
#define VKI_SNDCTL_DSP_SETTRIGGER	_VKI_SIOW ('P',16, int)

#define VKI_SNDCTL_DSP_SETSYNCRO	_VKI_SIO  ('P', 21)
#define VKI_SNDCTL_DSP_SETDUPLEX	_VKI_SIO  ('P', 22)
#define VKI_SNDCTL_DSP_GETODELAY	_VKI_SIOR ('P', 23, int)

#define VKI_SNDCTL_DSP_GETCHANNELMASK	_VKI_SIOWR('P', 64, int)
#define VKI_SNDCTL_DSP_BIND_CHANNEL	_VKI_SIOWR('P', 65, int)

#define VKI_SNDCTL_DSP_SETSPDIF		_VKI_SIOW ('P', 66, int)
#define VKI_SNDCTL_DSP_GETSPDIF		_VKI_SIOR ('P', 67, int)

#define VKI_SOUND_PCM_READ_RATE		_VKI_SIOR ('P', 2, int)
#define VKI_SOUND_PCM_READ_CHANNELS	_VKI_SIOR ('P', 6, int)
#define VKI_SOUND_PCM_READ_BITS		_VKI_SIOR ('P', 5, int)
#define VKI_SOUND_PCM_READ_FILTER	_VKI_SIOR ('P', 7, int)



struct vki_hd_geometry {
      unsigned char heads;
      unsigned char sectors;
      unsigned short cylinders;
      unsigned long start;
};

#define VKI_HDIO_GETGEO		0x0301	
#define VKI_HDIO_GET_DMA	0x030b	
#define VKI_HDIO_GET_IDENTITY	0x030d	

#define VKI_SIZEOF_STRUCT_HD_DRIVEID   512


#define VKI_FBIOGET_VSCREENINFO	0x4600
#define VKI_FBIOPUT_VSCREENINFO	0x4601
#define VKI_FBIOGET_FSCREENINFO	0x4602
#define VKI_FBIOPAN_DISPLAY	0x4606

struct vki_fb_fix_screeninfo {
	char id[16];			
	unsigned long smem_start;	
					
	__vki_u32 smem_len;			
	__vki_u32 type;			
	__vki_u32 type_aux;		
	__vki_u32 visual;		 
	__vki_u16 xpanstep;		
	__vki_u16 ypanstep;		
	__vki_u16 ywrapstep;		
	__vki_u32 line_length;		
	unsigned long mmio_start;	
					
	__vki_u32 mmio_len;		
	__vki_u32 accel;		
					
	__vki_u16 reserved[3];		
};

struct vki_fb_bitfield {
	__vki_u32 offset;		
	__vki_u32 length;		
	__vki_u32 msb_right;		 
					 
};

struct vki_fb_var_screeninfo {
	__vki_u32 xres;			
	__vki_u32 yres;
	__vki_u32 xres_virtual;		
	__vki_u32 yres_virtual;
	__vki_u32 xoffset;		
	__vki_u32 yoffset;		

	__vki_u32 bits_per_pixel;	
	__vki_u32 grayscale;		

	struct vki_fb_bitfield red;	
	struct vki_fb_bitfield green;	
	struct vki_fb_bitfield blue;
	struct vki_fb_bitfield transp;		

	__vki_u32 nonstd;		

	__vki_u32 activate;		

	__vki_u32 height;		
	__vki_u32 width;		

	__vki_u32 accel_flags;		

	
	__vki_u32 pixclock;		
	__vki_u32 left_margin;		
	__vki_u32 right_margin;		
	__vki_u32 upper_margin;		
	__vki_u32 lower_margin;
	__vki_u32 hsync_len;		
	__vki_u32 vsync_len;		
	__vki_u32 sync;			
	__vki_u32 vmode;		
	__vki_u32 rotate;		
	__vki_u32 reserved[5];		
};


#define VKI_GIO_FONT       0x4B60  
#define VKI_PIO_FONT       0x4B61  

#define VKI_GIO_FONTX      0x4B6B  
#define VKI_PIO_FONTX      0x4B6C  
struct vki_consolefontdesc {
	unsigned short charcount;	
	unsigned short charheight;	
	char __user *chardata;		
};

#define VKI_PIO_FONTRESET  0x4B6D  

#define VKI_GIO_CMAP       0x4B70  
#define VKI_PIO_CMAP       0x4B71  

#define VKI_KIOCSOUND      0x4B2F  
#define VKI_KDMKTONE       0x4B30  

#define VKI_KDGETLED       0x4B31  
#define VKI_KDSETLED       0x4B32  

#define VKI_KDGKBTYPE      0x4B33  

#define VKI_KDADDIO        0x4B34  
#define VKI_KDDELIO        0x4B35  
#define VKI_KDENABIO       0x4B36  
#define VKI_KDDISABIO      0x4B37  

#define VKI_KDSETMODE      0x4B3A  
#define VKI_KDGETMODE      0x4B3B  

#define VKI_KDMAPDISP      0x4B3C  
#define VKI_KDUNMAPDISP    0x4B3D  

#define		VKI_E_TABSZ		256
#define VKI_GIO_SCRNMAP    0x4B40  
#define VKI_PIO_SCRNMAP	   0x4B41  
#define VKI_GIO_UNISCRNMAP 0x4B69  
#define VKI_PIO_UNISCRNMAP 0x4B6A  

#define VKI_GIO_UNIMAP     0x4B66  
struct vki_unipair {
	unsigned short unicode;
	unsigned short fontpos;
};
struct vki_unimapdesc {
	unsigned short entry_ct;
	struct vki_unipair __user *entries;
};
#define VKI_PIO_UNIMAP     0x4B67  
#define VKI_PIO_UNIMAPCLR  0x4B68  
struct vki_unimapinit {
	unsigned short advised_hashsize;  
	unsigned short advised_hashstep;  
	unsigned short advised_hashlevel; 
};

#define VKI_KDGKBMODE      0x4B44  
#define VKI_KDSKBMODE      0x4B45  

#define VKI_KDGKBMETA      0x4B62  
#define VKI_KDSKBMETA      0x4B63  

#define VKI_KDGKBLED       0x4B64  
#define VKI_KDSKBLED       0x4B65  

struct vki_kbentry {
	unsigned char kb_table;
	unsigned char kb_index;
	unsigned short kb_value;
};
#define VKI_KDGKBENT       0x4B46  
#define VKI_KDSKBENT       0x4B47  

struct vki_kbsentry {
	unsigned char kb_func;
	unsigned char kb_string[512];
};
#define VKI_KDGKBSENT      0x4B48  
#define VKI_KDSKBSENT      0x4B49  

struct vki_kbdiacr {
        unsigned char diacr, base, result;
};
struct vki_kbdiacrs {
        unsigned int kb_cnt;    
	struct vki_kbdiacr kbdiacr[256];    
};
#define VKI_KDGKBDIACR     0x4B4A  
#define VKI_KDSKBDIACR     0x4B4B  

struct vki_kbkeycode {
	unsigned int scancode, keycode;
};
#define VKI_KDGETKEYCODE   0x4B4C  
#define VKI_KDSETKEYCODE   0x4B4D  

#define VKI_KDSIGACCEPT    0x4B4E  

struct vki_kbd_repeat {
	int delay;	
	int period;	
			
};
#define VKI_KDKBDREP       0x4B52  

#define VKI_KDFONTOP       0x4B72  

struct vki_console_font_op {
	unsigned int op;	
	unsigned int flags;	
	unsigned int width, height;	
	unsigned int charcount;
	unsigned char __user *data;	
};

#define VKI_KD_FONT_OP_SET		0	
#define VKI_KD_FONT_OP_GET		1	
#define VKI_KD_FONT_OP_SET_DEFAULT	2	
#define VKI_KD_FONT_OP_COPY		3	


typedef __vki_kernel_uid32_t vki_qid_t; 


#define VKI_PTRACE_TRACEME         0
#define VKI_PTRACE_PEEKTEXT	   1
#define VKI_PTRACE_PEEKDATA	   2
#define VKI_PTRACE_PEEKUSR	   3
#define VKI_PTRACE_POKEUSR	   6

#define VKI_PTRACE_DETACH         17

#define VKI_PTRACE_GETEVENTMSG	0x4201
#define VKI_PTRACE_GETSIGINFO	0x4202
#define VKI_PTRACE_SETSIGINFO	0x4203
#define VKI_PTRACE_GETREGSET	0x4204
#define VKI_PTRACE_SETREGSET	0x4205


enum {
	VKI_SNDRV_PCM_IOCTL_HW_FREE = _VKI_IO('A', 0x12),
	VKI_SNDRV_PCM_IOCTL_HWSYNC = _VKI_IO('A', 0x22),
	VKI_SNDRV_PCM_IOCTL_PREPARE = _VKI_IO('A', 0x40),
	VKI_SNDRV_PCM_IOCTL_RESET = _VKI_IO('A', 0x41),
	VKI_SNDRV_PCM_IOCTL_START = _VKI_IO('A', 0x42),
	VKI_SNDRV_PCM_IOCTL_DROP = _VKI_IO('A', 0x43),
	VKI_SNDRV_PCM_IOCTL_DRAIN = _VKI_IO('A', 0x44),
	VKI_SNDRV_PCM_IOCTL_PAUSE = _VKI_IOW('A', 0x45, int),
	VKI_SNDRV_PCM_IOCTL_RESUME = _VKI_IO('A', 0x47),
	VKI_SNDRV_PCM_IOCTL_XRUN = _VKI_IO('A', 0x48),
	VKI_SNDRV_PCM_IOCTL_LINK = _VKI_IOW('A', 0x60, int),
	VKI_SNDRV_PCM_IOCTL_UNLINK = _VKI_IO('A', 0x61),
};

enum {
	VKI_SNDRV_TIMER_IOCTL_START = _VKI_IO('T', 0xa0),
	VKI_SNDRV_TIMER_IOCTL_STOP = _VKI_IO('T', 0xa1),
	VKI_SNDRV_TIMER_IOCTL_CONTINUE = _VKI_IO('T', 0xa2),
	VKI_SNDRV_TIMER_IOCTL_PAUSE = _VKI_IO('T', 0xa3),
};

struct vki_snd_ctl_card_info {
	int card;			
	int pad;			
	unsigned char id[16];		
	unsigned char driver[16];	
	unsigned char name[32];		
	unsigned char longname[80];	
	unsigned char reserved_[16];	
	unsigned char mixername[80];	
	unsigned char components[128];	
};

typedef int vki_snd_ctl_elem_iface_t;
#define	VKI_SNDRV_CTL_ELEM_IFACE_CARD		((vki_snd_ctl_elem_iface_t) 0) 
#define	VKI_SNDRV_CTL_ELEM_IFACE_HWDEP		((vki_snd_ctl_elem_iface_t) 1) 
#define	VKI_SNDRV_CTL_ELEM_IFACE_MIXER		((vki_snd_ctl_elem_iface_t) 2) 
#define	VKI_SNDRV_CTL_ELEM_IFACE_PCM		((vki_snd_ctl_elem_iface_t) 3) 
#define	VKI_SNDRV_CTL_ELEM_IFACE_RAWMIDI	((vki_snd_ctl_elem_iface_t) 4) 
#define	VKI_SNDRV_CTL_ELEM_IFACE_TIMER		((vki_snd_ctl_elem_iface_t) 5) 
#define	VKI_SNDRV_CTL_ELEM_IFACE_SEQUENCER	((vki_snd_ctl_elem_iface_t) 6) 
#define	VKI_SNDRV_CTL_ELEM_IFACE_LAST		VKI_SNDRV_CTL_ELEM_IFACE_SEQUENCER

struct vki_snd_ctl_elem_id {
	unsigned int numid;		
	vki_snd_ctl_elem_iface_t iface;	
	unsigned int device;		
	unsigned int subdevice;		
	unsigned char name[44];		
	unsigned int index;		
};

struct vki_snd_ctl_elem_list {
	unsigned int offset;		
	unsigned int space;		
	unsigned int used;		
	unsigned int count;		
	struct vki_snd_ctl_elem_id __user *pids; 
	unsigned char reserved[50];
};

struct vki_snd_ctl_tlv {
    unsigned int numid;	
    unsigned int length;	
    unsigned int tlv[0];	
};

#define VKI_SNDRV_CTL_IOCTL_PVERSION	_VKI_IOR('U', 0x00, int)
#define VKI_SNDRV_CTL_IOCTL_CARD_INFO	_VKI_IOR('U', 0x01, struct vki_snd_ctl_card_info)
#define VKI_SNDRV_CTL_IOCTL_ELEM_LIST	_VKI_IOWR('U', 0x10, struct vki_snd_ctl_elem_list)
#define VKI_SNDRV_CTL_IOCTL_TLV_READ	_VKI_IOWR('U', 0x1a, struct vki_snd_ctl_tlv)
#define VKI_SNDRV_CTL_IOCTL_TLV_WRITE	_VKI_IOWR('U', 0x1b, struct vki_snd_ctl_tlv)
#define VKI_SNDRV_CTL_IOCTL_TLV_COMMAND	_VKI_IOWR('U', 0x1c, struct vki_snd_ctl_tlv)


struct vki_serial_icounter_struct {
	int cts, dsr, rng, dcd;
	int rx, tx;
	int frame, overrun, parity, brk;
	int buf_overrun;
	int reserved[9];
};


#define VKI_VT_OPENQRY	0x5600	

struct vki_vt_mode {
	char mode;		
	char waitv;		
	short relsig;		
	short acqsig;		
	short frsig;		
};
#define VKI_VT_GETMODE	0x5601	
#define VKI_VT_SETMODE	0x5602	

struct vki_vt_stat {
	unsigned short v_active;	
	unsigned short v_signal;	
	unsigned short v_state;		
};
#define VKI_VT_GETSTATE	0x5603	
#define VKI_VT_SENDSIG	0x5604	

#define VKI_VT_RELDISP	0x5605	

#define VKI_VT_ACTIVATE	0x5606	
#define VKI_VT_WAITACTIVE	0x5607	
#define VKI_VT_DISALLOCATE	0x5608  

struct vki_vt_sizes {
	unsigned short v_rows;		
	unsigned short v_cols;		
	unsigned short v_scrollsize;	
};
#define VKI_VT_RESIZE	0x5609	

struct vki_vt_consize {
	unsigned short v_rows;	
	unsigned short v_cols;	
	unsigned short v_vlin;	
	unsigned short v_clin;	
	unsigned short v_vcol;	
	unsigned short v_ccol;	
};
#define VKI_VT_RESIZEX      0x560A  
#define VKI_VT_LOCKSWITCH   0x560B  
#define VKI_VT_UNLOCKSWITCH 0x560C  


#define VKI_PR_SET_PDEATHSIG  1  
#define VKI_PR_GET_PDEATHSIG  2  

#define VKI_PR_GET_DUMPABLE   3
#define VKI_PR_SET_DUMPABLE   4

#define VKI_PR_GET_UNALIGN	  5
#define VKI_PR_SET_UNALIGN	  6
# define VKI_PR_UNALIGN_NOPRINT	1	
# define VKI_PR_UNALIGN_SIGBUS	2	

#define VKI_PR_GET_KEEPCAPS   7
#define VKI_PR_SET_KEEPCAPS   8

#define VKI_PR_GET_FPEMU  9
#define VKI_PR_SET_FPEMU 10
# define VKI_PR_FPEMU_NOPRINT	1	
# define VKI_PR_FPEMU_SIGFPE	2	

#define VKI_PR_GET_FPEXC	11
#define VKI_PR_SET_FPEXC	12
# define VKI_PR_FP_EXC_SW_ENABLE	0x80	
# define VKI_PR_FP_EXC_DIV		0x010000	
# define VKI_PR_FP_EXC_OVF		0x020000	
# define VKI_PR_FP_EXC_UND		0x040000	
# define VKI_PR_FP_EXC_RES		0x080000	
# define VKI_PR_FP_EXC_INV		0x100000	
# define VKI_PR_FP_EXC_DISABLED	0	
# define VKI_PR_FP_EXC_NONRECOV	1	
# define VKI_PR_FP_EXC_ASYNC	2	
# define VKI_PR_FP_EXC_PRECISE	3	

#define VKI_PR_GET_TIMING   13
#define VKI_PR_SET_TIMING   14
# define VKI_PR_TIMING_STATISTICAL  0       
# define VKI_PR_TIMING_TIMESTAMP    1       

#define VKI_PR_SET_NAME    15		
#define VKI_PR_GET_NAME    16		

#define VKI_PR_GET_ENDIAN	19
#define VKI_PR_SET_ENDIAN	20
# define VKI_PR_ENDIAN_BIG		0
# define VKI_PR_ENDIAN_LITTLE	1	
# define VKI_PR_ENDIAN_PPC_LITTLE	2	

#define VKI_PR_SET_SECCOMP 22

#define VKI_PR_SET_PTRACER 0x59616d61


struct vki_usbdevfs_ctrltransfer {
	__vki_u8 bRequestType;
	__vki_u8 bRequest;
	__vki_u16 wValue;
	__vki_u16 wIndex;
	__vki_u16 wLength;
	__vki_u32 timeout;  
 	void __user *data;
};

struct vki_usbdevfs_bulktransfer {
	unsigned int ep;
	unsigned int len;
	unsigned int timeout; 
	void __user *data;
};

#define VKI_USBDEVFS_MAXDRIVERNAME 255

struct vki_usbdevfs_getdriver {
	unsigned int interface;
	char driver[VKI_USBDEVFS_MAXDRIVERNAME + 1];
};

struct vki_usbdevfs_connectinfo {
	unsigned int devnum;
	unsigned char slow;
};

struct vki_usbdevfs_iso_packet_desc {
	unsigned int length;
	unsigned int actual_length;
	unsigned int status;
};

struct vki_usbdevfs_urb {
	unsigned char type;
	unsigned char endpoint;
	int status;
	unsigned int flags;
	void __user *buffer;
	int buffer_length;
	int actual_length;
	int start_frame;
	int number_of_packets;
	int error_count;
	unsigned int signr;  
	void *usercontext;
	struct vki_usbdevfs_iso_packet_desc iso_frame_desc[0];
};

struct vki_usbdevfs_ioctl {
	int	ifno;		
	int	ioctl_code;	
	void __user *data;	
};

#define VKI_USBDEVFS_CONTROL           _VKI_IOWR('U', 0, struct vki_usbdevfs_ctrltransfer)
#define VKI_USBDEVFS_BULK              _VKI_IOWR('U', 2, struct vki_usbdevfs_bulktransfer)
#define VKI_USBDEVFS_GETDRIVER         _VKI_IOW('U', 8, struct vki_usbdevfs_getdriver)
#define VKI_USBDEVFS_SUBMITURB         _VKI_IOR('U', 10, struct vki_usbdevfs_urb)
#define VKI_USBDEVFS_DISCARDURB        _VKI_IO('U', 11)
#define VKI_USBDEVFS_REAPURB           _VKI_IOW('U', 12, void *)
#define VKI_USBDEVFS_REAPURBNDELAY     _VKI_IOW('U', 13, void *)
#define VKI_USBDEVFS_CONNECTINFO       _VKI_IOW('U', 17, struct vki_usbdevfs_connectinfo)
#define VKI_USBDEVFS_IOCTL             _VKI_IOWR('U', 18, struct vki_usbdevfs_ioctl)
#define VKI_USBDEVFS_RESET             _VKI_IO('U', 20)

#define VKI_USBDEVFS_URB_TYPE_ISO              0
#define VKI_USBDEVFS_URB_TYPE_INTERRUPT        1
#define VKI_USBDEVFS_URB_TYPE_CONTROL          2
#define VKI_USBDEVFS_URB_TYPE_BULK             3

struct vki_usbdevfs_setuppacket {
       __vki_u8 bRequestType;
       __vki_u8 bRequest;
       __vki_u16 wValue;
       __vki_u16 wIndex;
       __vki_u16 wLength;
};


#define VKI_I2C_SMBUS_QUICK             0
#define VKI_I2C_SMBUS_BYTE              1
#define VKI_I2C_SMBUS_BYTE_DATA         2
#define VKI_I2C_SMBUS_WORD_DATA         3
#define VKI_I2C_SMBUS_PROC_CALL         4
#define VKI_I2C_SMBUS_BLOCK_DATA        5
#define VKI_I2C_SMBUS_I2C_BLOCK_BROKEN  6
#define VKI_I2C_SMBUS_BLOCK_PROC_CALL   7           
#define VKI_I2C_SMBUS_I2C_BLOCK_DATA    8

#define VKI_I2C_SMBUS_READ  1
#define VKI_I2C_SMBUS_WRITE 0

#define VKI_I2C_SLAVE        0x0703  
                                     
#define VKI_I2C_SLAVE_FORCE  0x0706  
                                     
                                     
                                     
#define VKI_I2C_TENBIT       0x0704  
#define VKI_I2C_FUNCS        0x0705  
#define VKI_I2C_RDWR         0x0707  
#define VKI_I2C_PEC          0x0708  
#define VKI_I2C_SMBUS        0x0720  

#define VKI_I2C_SMBUS_BLOCK_MAX  32  
union vki_i2c_smbus_data {
        __vki_u8 byte;
        __vki_u16 word;
        __vki_u8 block[VKI_I2C_SMBUS_BLOCK_MAX + 2];
                 
                 
};

struct vki_i2c_smbus_ioctl_data {
        __vki_u8 read_write;
        __vki_u8 command;
        __vki_u32 size;
        union vki_i2c_smbus_data __user *data;
};

struct vki_i2c_msg {
	__vki_u16 addr;		
	__vki_u16 flags;
#define VKI_I2C_M_TEN		0x0010	
#define VKI_I2C_M_RD		0x0001	
#define VKI_I2C_M_NOSTART	0x4000	
#define VKI_I2C_M_REV_DIR_ADDR	0x2000	
#define VKI_I2C_M_IGNORE_NAK	0x1000	
#define VKI_I2C_M_NO_RD_ACK	0x0800	
#define VKI_I2C_M_RECV_LEN	0x0400	
	__vki_u16 len;		
	__vki_u8 *buf;		
};

struct vki_i2c_rdwr_ioctl_data {
	struct vki_i2c_msg *msgs;	
	__vki_u32 nmsgs;		
};


#define VKI_KEYCTL_GET_KEYRING_ID	0	
#define VKI_KEYCTL_JOIN_SESSION_KEYRING	1	
#define VKI_KEYCTL_UPDATE		2	
#define VKI_KEYCTL_REVOKE		3	
#define VKI_KEYCTL_CHOWN		4	
#define VKI_KEYCTL_SETPERM		5	
#define VKI_KEYCTL_DESCRIBE		6	
#define VKI_KEYCTL_CLEAR		7	
#define VKI_KEYCTL_LINK			8	
#define VKI_KEYCTL_UNLINK		9	
#define VKI_KEYCTL_SEARCH		10	
#define VKI_KEYCTL_READ			11	
#define VKI_KEYCTL_INSTANTIATE		12	
#define VKI_KEYCTL_NEGATE		13	
#define VKI_KEYCTL_SET_REQKEY_KEYRING	14	
#define VKI_KEYCTL_SET_TIMEOUT		15	
#define VKI_KEYCTL_ASSUME_AUTHORITY	16	


typedef vki_int32_t vki_key_serial_t;

typedef vki_uint32_t vki_key_perm_t;



#define VKI_SIOCSIWCOMMIT	0x8B00	
#define VKI_SIOCGIWNAME		0x8B01	

#define VKI_SIOCSIWNWID		0x8B02	
#define VKI_SIOCGIWNWID		0x8B03	
#define VKI_SIOCSIWFREQ		0x8B04	
#define VKI_SIOCGIWFREQ		0x8B05	
#define VKI_SIOCSIWMODE		0x8B06	
#define VKI_SIOCGIWMODE		0x8B07	
#define VKI_SIOCSIWSENS		0x8B08	
#define VKI_SIOCGIWSENS		0x8B09	

#define VKI_SIOCSIWRANGE	0x8B0A	
#define VKI_SIOCGIWRANGE	0x8B0B	
#define VKI_SIOCSIWPRIV		0x8B0C	
#define VKI_SIOCGIWPRIV		0x8B0D	
#define VKI_SIOCSIWSTATS	0x8B0E	
#define VKI_SIOCGIWSTATS	0x8B0F	

#define VKI_SIOCSIWSPY		0x8B10	
#define VKI_SIOCGIWSPY		0x8B11	
#define VKI_SIOCSIWTHRSPY	0x8B12	
#define VKI_SIOCGIWTHRSPY	0x8B13	

#define VKI_SIOCSIWAP		0x8B14	
#define VKI_SIOCGIWAP		0x8B15	
#define VKI_SIOCGIWAPLIST	0x8B17	
#define VKI_SIOCSIWSCAN         0x8B18	
#define VKI_SIOCGIWSCAN         0x8B19	

#define VKI_SIOCSIWESSID	0x8B1A	
#define VKI_SIOCGIWESSID	0x8B1B	
#define VKI_SIOCSIWNICKN	0x8B1C	
#define VKI_SIOCGIWNICKN	0x8B1D	

#define VKI_SIOCSIWRATE		0x8B20	
#define VKI_SIOCGIWRATE		0x8B21	
#define VKI_SIOCSIWRTS		0x8B22	
#define VKI_SIOCGIWRTS		0x8B23	
#define VKI_SIOCSIWFRAG		0x8B24	
#define VKI_SIOCGIWFRAG		0x8B25	
#define VKI_SIOCSIWTXPOW	0x8B26	
#define VKI_SIOCGIWTXPOW	0x8B27	
#define VKI_SIOCSIWRETRY	0x8B28	
#define VKI_SIOCGIWRETRY	0x8B29	

#define VKI_SIOCSIWENCODE	0x8B2A	
#define VKI_SIOCGIWENCODE	0x8B2B	

#define VKI_SIOCSIWPOWER	0x8B2C	
#define VKI_SIOCGIWPOWER	0x8B2D	

#define VKI_SIOCSIWGENIE	0x8B30		
#define VKI_SIOCGIWGENIE	0x8B31		

#define VKI_SIOCSIWMLME		0x8B16	
#define VKI_SIOCSIWAUTH		0x8B32	
#define VKI_SIOCGIWAUTH		0x8B33	

#define VKI_SIOCSIWENCODEEXT	0x8B34	
#define VKI_SIOCGIWENCODEEXT	0x8B35	

#define VKI_SIOCSIWPMKSA	0x8B36	


struct	vki_iw_param
{
  __vki_s32	value;		
  __vki_u8	fixed;		
  __vki_u8	disabled;	
  __vki_u16	flags;		
};

struct	vki_iw_point
{
  void __user	*pointer;	
  __vki_u16	length;		
  __vki_u16	flags;		
};

struct	vki_iw_freq
{
	__vki_s32	m;		
	__vki_s16	e;		
	__vki_u8	i;		
	__vki_u8	flags;		
};

struct	vki_iw_quality
{
	__vki_u8	qual;		
	__vki_u8	level;		
	__vki_u8	noise;		
	__vki_u8	updated;	
};

union	vki_iwreq_data
{
	
	char		name[VKI_IFNAMSIZ];

	struct vki_iw_point	essid;	
	struct vki_iw_param	nwid;	
	struct vki_iw_freq	freq;	

	struct vki_iw_param	sens;	
	struct vki_iw_param	bitrate;
	struct vki_iw_param	txpower;
	struct vki_iw_param	rts;	
	struct vki_iw_param	frag;	
	__vki_u32		mode;	
	struct vki_iw_param	retry;	

	struct vki_iw_point	encoding; 
	struct vki_iw_param	power;	
	struct vki_iw_quality	qual;	

	struct vki_sockaddr ap_addr;	
	struct vki_sockaddr addr;	

	struct vki_iw_param	param;	
	struct vki_iw_point	data;	
};

struct	vki_iwreq 
{
	union
	{
		char ifrn_name[VKI_IFNAMSIZ];	
	} ifr_ifrn;

	
	union	vki_iwreq_data	u;
};


struct vki_perf_event_attr {

	__vki_u32			type;

	__vki_u32			size;

	__vki_u64			config;

	union {
		__vki_u64		sample_period;
		__vki_u64		sample_freq;
	};

	__vki_u64			sample_type;
	__vki_u64			read_format;

	__vki_u64			disabled       :  1, 
					inherit	       :  1, 
					pinned	       :  1, 
					exclusive      :  1, 
					exclude_user   :  1, 
					exclude_kernel :  1, 
					exclude_hv     :  1, 
					exclude_idle   :  1, 
					mmap           :  1, 
					comm	       :  1, 
					freq           :  1, 
					inherit_stat   :  1, 
					enable_on_exec :  1, 
					task           :  1, 
					watermark      :  1, 
					precise_ip     :  2, 
					mmap_data      :  1, 
					sample_id_all  :  1, 

					__reserved_1   : 45;

	union {
		__vki_u32		wakeup_events;	  
		__vki_u32		wakeup_watermark; 
	};

	__vki_u32			bp_type;
	union {
		__vki_u64		bp_addr;
		__vki_u64		config1; 
	};
	union {
		__vki_u64		bp_len;
		__vki_u64		config2; 
	};
};


struct vki_getcpu_cache {
	unsigned long blob[128 / sizeof(long)];
};



#define VKI_EVIOCGNAME(len)	_VKI_IOC(_VKI_IOC_READ, 'E', 0x06, len)		
#define VKI_EVIOCGPHYS(len)	_VKI_IOC(_VKI_IOC_READ, 'E', 0x07, len)		
#define VKI_EVIOCGUNIQ(len)	_VKI_IOC(_VKI_IOC_READ, 'E', 0x08, len)		

#define VKI_EVIOCGKEY(len)	_VKI_IOC(_VKI_IOC_READ, 'E', 0x18, len)		
#define VKI_EVIOCGLED(len)	_VKI_IOC(_VKI_IOC_READ, 'E', 0x19, len)		
#define VKI_EVIOCGSND(len)	_VKI_IOC(_VKI_IOC_READ, 'E', 0x1a, len)		
#define VKI_EVIOCGSW(len)	_VKI_IOC(_VKI_IOC_READ, 'E', 0x1b, len)		

#define VKI_EVIOCGBIT(ev,len)	_VKI_IOC(_VKI_IOC_READ, 'E', 0x20 + ev, len)	


#define VKI_EV_SYN		0x00
#define VKI_EV_KEY		0x01
#define VKI_EV_REL		0x02
#define VKI_EV_ABS		0x03
#define VKI_EV_MSC		0x04
#define VKI_EV_SW		0x05
#define VKI_EV_LED		0x11
#define VKI_EV_SND		0x12
#define VKI_EV_REP		0x14
#define VKI_EV_FF		0x15
#define VKI_EV_PWR		0x16
#define VKI_EV_FF_STATUS	0x17
#define VKI_EV_MAX		0x1f
#define VKI_EV_CNT		(VKI_EV_MAX+1)


#ifndef VKI_FIOQSIZE
#define VKI_FIOQSIZE 0x5460     
#endif

#ifndef VKI_TIOCSIG
#define VKI_TIOCSIG _VKI_IOW('T', 0x36, int) 
#endif


#if defined(VGPV_arm_linux_android) || defined(VGPV_x86_linux_android) \
    || defined(VGPV_mips32_linux_android)

#define VKI_ASHMEM_NAME_LEN 256

#define VKI_ASHMEM_NAME_DEF "dev/ashmem"

#define VKI_ASHMEM_NOT_PURGED 0
#define VKI_ASHMEM_WAS_PURGED 1

#define VKI_ASHMEM_IS_UNPINNED 0
#define VKI_ASHMEM_IS_PINNED 1

struct vki_ashmem_pin {
   vki_uint32_t offset;
   vki_uint32_t len;
};

#define __VKI_ASHMEMIOC 0x77

#define VKI_ASHMEM_SET_NAME _VKI_IOW(__VKI_ASHMEMIOC, 1, char[VKI_ASHMEM_NAME_LEN])
#define VKI_ASHMEM_GET_NAME _VKI_IOR(__VKI_ASHMEMIOC, 2, char[VKI_ASHMEM_NAME_LEN])
#define VKI_ASHMEM_SET_SIZE _VKI_IOW(__VKI_ASHMEMIOC, 3, vki_size_t)
#define VKI_ASHMEM_GET_SIZE _VKI_IO(__VKI_ASHMEMIOC, 4)
#define VKI_ASHMEM_SET_PROT_MASK _VKI_IOW(__VKI_ASHMEMIOC, 5, unsigned long)
#define VKI_ASHMEM_GET_PROT_MASK _VKI_IO(__VKI_ASHMEMIOC, 6)
#define VKI_ASHMEM_PIN _VKI_IOW(__VKI_ASHMEMIOC, 7, struct vki_ashmem_pin)
#define VKI_ASHMEM_UNPIN _VKI_IOW(__VKI_ASHMEMIOC, 8, struct vki_ashmem_pin)
#define VKI_ASHMEM_GET_PIN_STATUS _VKI_IO(__VKI_ASHMEMIOC, 9)
#define VKI_ASHMEM_PURGE_ALL_CACHES _VKI_IO(__VKI_ASHMEMIOC, 10)


struct vki_binder_write_read {
 signed long write_size;
 signed long write_consumed;
 unsigned long write_buffer;
 signed long read_size;
 signed long read_consumed;
 unsigned long read_buffer;
};

struct vki_binder_version {
 signed long protocol_version;
};

#define VKI_BINDER_WRITE_READ _VKI_IOWR('b', 1, struct vki_binder_write_read)
#define VKI_BINDER_SET_IDLE_TIMEOUT _VKI_IOW('b', 3, vki_int64_t)
#define VKI_BINDER_SET_MAX_THREADS _VKI_IOW('b', 5, vki_size_t)
#define VKI_BINDER_SET_IDLE_PRIORITY _VKI_IOW('b', 6, int)
#define VKI_BINDER_SET_CONTEXT_MGR _VKI_IOW('b', 7, int)
#define VKI_BINDER_THREAD_EXIT _VKI_IOW('b', 8, int)
#define VKI_BINDER_VERSION _VKI_IOWR('b', 9, struct vki_binder_version)

#endif 


typedef struct {
   __vki_u8 b[6];
} __vki_packed vki_bdaddr_t;


#define VKI_HCIDEVUP        _VKI_IOW('H', 201, int)
#define VKI_HCIDEVDOWN      _VKI_IOW('H', 202, int)
#define VKI_HCIDEVRESET     _VKI_IOW('H', 203, int)
#define VKI_HCIDEVRESTAT    _VKI_IOW('H', 204, int)

#define VKI_HCIGETDEVLIST   _VKI_IOR('H', 210, int)

struct vki_hci_dev_req {
	__vki_u16  dev_id;
	__vki_u32  dev_opt;
};

struct vki_hci_dev_list_req {
	__vki_u16  dev_num;
	struct vki_hci_dev_req dev_req[0];	
};

#define VKI_HCIGETDEVINFO   _VKI_IOR('H', 211, int)
#define VKI_HCIGETCONNLIST  _VKI_IOR('H', 212, int)
#define VKI_HCIGETCONNINFO  _VKI_IOR('H', 213, int)
#define VKI_HCIGETAUTHINFO  _VKI_IOR('H', 215, int)

#define VKI_HCISETRAW       _VKI_IOW('H', 220, int)
#define VKI_HCISETSCAN      _VKI_IOW('H', 221, int)
#define VKI_HCISETAUTH      _VKI_IOW('H', 222, int)
#define VKI_HCISETENCRYPT   _VKI_IOW('H', 223, int)
#define VKI_HCISETPTYPE     _VKI_IOW('H', 224, int)
#define VKI_HCISETLINKPOL   _VKI_IOW('H', 225, int)
#define VKI_HCISETLINKMODE  _VKI_IOW('H', 226, int)
#define VKI_HCISETACLMTU    _VKI_IOW('H', 227, int)
#define VKI_HCISETSCOMTU    _VKI_IOW('H', 228, int)

#define VKI_HCIBLOCKADDR    _VKI_IOW('H', 230, int)
#define VKI_HCIUNBLOCKADDR  _VKI_IOW('H', 231, int)

#define VKI_HCIINQUIRY      _VKI_IOR('H', 240, int)

struct vki_inquiry_info {
   vki_bdaddr_t bdaddr;
   __vki_u8     pscan_rep_mode;
   __vki_u8     pscan_period_mode;
   __vki_u8     pscan_mode;
   __vki_u8     dev_class[3];
   __vki_le16   clock_offset;
} __vki_packed;

struct vki_hci_inquiry_req {
   __vki_u16 dev_id;
   __vki_u16 flags;
   __vki_u8  lap[3];
   __vki_u8  length;
   __vki_u8  num_rsp;
};


struct vki_sockaddr_rc {
        vki_sa_family_t     rc_family;
        vki_bdaddr_t        rc_bdaddr;
        __vki_u8            rc_channel;
};

#define KVMIO 0xAE

#define VKI_KVM_GET_API_VERSION       _VKI_IO(KVMIO,   0x00)
#define VKI_KVM_CREATE_VM             _VKI_IO(KVMIO,   0x01) 
#define VKI_KVM_CHECK_EXTENSION       _VKI_IO(KVMIO,   0x03)
#define VKI_KVM_GET_VCPU_MMAP_SIZE    _VKI_IO(KVMIO,   0x04) 
#define VKI_KVM_S390_ENABLE_SIE       _VKI_IO(KVMIO,   0x06)
#define VKI_KVM_CREATE_VCPU           _VKI_IO(KVMIO,   0x41)
#define VKI_KVM_SET_NR_MMU_PAGES      _VKI_IO(KVMIO,   0x44)
#define VKI_KVM_GET_NR_MMU_PAGES      _VKI_IO(KVMIO,   0x45)
#define VKI_KVM_SET_TSS_ADDR          _VKI_IO(KVMIO,   0x47)
#define VKI_KVM_CREATE_IRQCHIP        _VKI_IO(KVMIO,   0x60)
#define VKI_KVM_CREATE_PIT            _VKI_IO(KVMIO,   0x64)
#define VKI_KVM_REINJECT_CONTROL      _VKI_IO(KVMIO,   0x71)
#define VKI_KVM_SET_BOOT_CPU_ID       _VKI_IO(KVMIO,   0x78)
#define VKI_KVM_SET_TSC_KHZ           _VKI_IO(KVMIO,  0xa2)
#define VKI_KVM_GET_TSC_KHZ           _VKI_IO(KVMIO,  0xa3)
#define VKI_KVM_RUN                   _VKI_IO(KVMIO,   0x80)
#define VKI_KVM_S390_INITIAL_RESET    _VKI_IO(KVMIO,   0x97)
#define VKI_KVM_NMI                   _VKI_IO(KVMIO,   0x9a)
#define VKI_KVM_KVMCLOCK_CTRL         _VKI_IO(KVMIO,   0xad)


struct vki_hwtstamp_config {
	int flags;
	int tx_type;
	int rx_filter;
};


#define VKI_UINPUT_IOCTL_BASE       'U'
#define VKI_UI_DEV_CREATE		_VKI_IO(VKI_UINPUT_IOCTL_BASE, 1)
#define VKI_UI_DEV_DESTROY		_VKI_IO(VKI_UINPUT_IOCTL_BASE, 2)

#define VKI_UI_SET_EVBIT		_VKI_IOW(VKI_UINPUT_IOCTL_BASE, 100, int)
#define VKI_UI_SET_KEYBIT		_VKI_IOW(VKI_UINPUT_IOCTL_BASE, 101, int)
#define VKI_UI_SET_RELBIT		_VKI_IOW(VKI_UINPUT_IOCTL_BASE, 102, int)
#define VKI_UI_SET_ABSBIT		_VKI_IOW(VKI_UINPUT_IOCTL_BASE, 103, int)
#define VKI_UI_SET_MSCBIT		_VKI_IOW(VKI_UINPUT_IOCTL_BASE, 104, int)
#define VKI_UI_SET_LEDBIT		_VKI_IOW(VKI_UINPUT_IOCTL_BASE, 105, int)
#define VKI_UI_SET_SNDBIT		_VKI_IOW(VKI_UINPUT_IOCTL_BASE, 106, int)
#define VKI_UI_SET_FFBIT		_VKI_IOW(VKI_UINPUT_IOCTL_BASE, 107, int)
#define VKI_UI_SET_SWBIT		_VKI_IOW(VKI_UINPUT_IOCTL_BASE, 109, int)
#define VKI_UI_SET_PROPBIT		_VKI_IOW(VKI_UINPUT_IOCTL_BASE, 110, int)


#define VKI_IB_IOCTL_MAGIC          0x1b

#define VKI_IB_USER_MAD_REGISTER_AGENT    _VKI_IOWR(VKI_IB_IOCTL_MAGIC, 1, \
                                              struct ib_user_mad_reg_req)

#define VKI_IB_USER_MAD_UNREGISTER_AGENT  _VKI_IOW(VKI_IB_IOCTL_MAGIC, 2, __u32)

#define VKI_IB_USER_MAD_ENABLE_PKEY       _VKI_IO(VKI_IB_IOCTL_MAGIC, 3)


#define VKI_TUNSETNOCSUM  _VKI_IOW('T', 200, int) 
#define VKI_TUNSETDEBUG   _VKI_IOW('T', 201, int) 
#define VKI_TUNSETIFF     _VKI_IOW('T', 202, int) 
#define VKI_TUNSETPERSIST _VKI_IOW('T', 203, int) 
#define VKI_TUNSETOWNER   _VKI_IOW('T', 204, int)
#define VKI_TUNSETLINK    _VKI_IOW('T', 205, int)
#define VKI_TUNSETGROUP   _VKI_IOW('T', 206, int)
#define VKI_TUNGETFEATURES _VKI_IOR('T', 207, unsigned int)
#define VKI_TUNSETOFFLOAD  _VKI_IOW('T', 208, unsigned int)
#define VKI_TUNSETTXFILTER _VKI_IOW('T', 209, unsigned int)
#define VKI_TUNGETIFF      _VKI_IOR('T', 210, unsigned int)
#define VKI_TUNGETSNDBUF   _VKI_IOR('T', 211, int)
#define VKI_TUNSETSNDBUF   _VKI_IOW('T', 212, int)
#define VKI_TUNGETVNETHDRSZ _VKI_IOR('T', 215, int)
#define VKI_TUNSETVNETHDRSZ _VKI_IOW('T', 216, int)
#define VKI_TUNSETQUEUE  _VKI_IOW('T', 217, int)
#define VKI_TUNSETIFINDEX	_VKI_IOW('T', 218, unsigned int)


#define VKI_VHOST_VIRTIO 0xAF
#define VKI_VHOST_SET_OWNER _VKI_IO(VKI_VHOST_VIRTIO, 0x01)
#define VKI_VHOST_RESET_OWNER _VKI_IO(VKI_VHOST_VIRTIO, 0x02)


typedef unsigned long __vki_xen_pfn_t;

struct vki_xen_privcmd_hypercall {
       __vki_u64 op;
       __vki_u64 arg[5];
};

struct vki_xen_privcmd_mmap_entry {
        __vki_u64 va;
        __vki_u64 mfn;
        __vki_u64 npages;
};

struct vki_xen_privcmd_mmap {
        int num;
        __vki_u16 dom; 
        struct vki_xen_privcmd_mmap_entry *entry;
};

struct vki_xen_privcmd_mmapbatch {
        int num;     
        __vki_u16 dom; 
        __vki_u64 addr;  
        __vki_xen_pfn_t *arr; 
};

struct vki_xen_privcmd_mmapbatch_v2 {
        unsigned int num; 
        __vki_u16 dom;      
        __vki_u64 addr;       
        const __vki_xen_pfn_t *arr; 
        int __user *err;  
};

#define VKI_XEN_IOCTL_PRIVCMD_HYPERCALL    _VKI_IOC(_VKI_IOC_NONE, 'P', 0, sizeof(struct vki_xen_privcmd_hypercall))
#define VKI_XEN_IOCTL_PRIVCMD_MMAP         _VKI_IOC(_VKI_IOC_NONE, 'P', 2, sizeof(struct vki_xen_privcmd_mmap))

#define VKI_XEN_IOCTL_PRIVCMD_MMAPBATCH    _VKI_IOC(_VKI_IOC_NONE, 'P', 3, sizeof(struct vki_xen_privcmd_mmapbatch))
#define VKI_XEN_IOCTL_PRIVCMD_MMAPBATCH_V2 _VKI_IOC(_VKI_IOC_NONE, 'P', 4, sizeof(struct vki_xen_privcmd_mmapbatch_v2))


#define VKI_XEN_IOCTL_EVTCHN_BIND_VIRQ				\
	_VKI_IOC(_VKI_IOC_NONE, 'E', 0, sizeof(struct vki_xen_ioctl_evtchn_bind_virq))
struct vki_xen_ioctl_evtchn_bind_virq {
	vki_uint32_t virq;
	vki_uint32_t port;
};

#define VKI_XEN_IOCTL_EVTCHN_BIND_INTERDOMAIN			\
	_VKI_IOC(_VKI_IOC_NONE, 'E', 1, sizeof(struct vki_xen_ioctl_evtchn_bind_interdomain))
struct vki_xen_ioctl_evtchn_bind_interdomain {
	vki_uint32_t remote_domain;
	vki_uint32_t remote_port;
};

#define VKI_XEN_IOCTL_EVTCHN_BIND_UNBOUND_PORT			\
	_VKI_IOC(_VKI_IOC_NONE, 'E', 2, sizeof(struct vki_xen_ioctl_evtchn_bind_unbound_port))
struct vki_xen_ioctl_evtchn_bind_unbound_port {
	vki_uint32_t remote_domain;
};

#define VKI_XEN_IOCTL_EVTCHN_UNBIND				\
	_VKI_IOC(_VKI_IOC_NONE, 'E', 3, sizeof(struct vki_xen_ioctl_evtchn_unbind))
struct vki_xen_ioctl_evtchn_unbind {
	vki_uint32_t port;
};

#define VKI_XEN_IOCTL_EVTCHN_NOTIFY				\
	_VKI_IOC(_VKI_IOC_NONE, 'E', 4, sizeof(struct vki_xen_ioctl_evtchn_notify))
struct vki_xen_ioctl_evtchn_notify {
	vki_uint32_t port;
};

#define VKI_XEN_IOCTL_EVTCHN_RESET				\
	_VKI_IOC(_VKI_IOC_NONE, 'E', 5, 0)



struct vki_file_handle {
   __vki_u32 handle_bytes;
   int handle_type;
   
   unsigned char f_handle[0];
};


struct vki_sock_filter {
	__vki_u16 code; 
	__vki_u8 jt;    
	__vki_u8 jf;    
	__vki_u32 k;    
};

struct vki_sock_fprog {
	__vki_u16 len;  
	struct vki_sock_filter *filter;
};
   

struct vki_ethtool_cmd {
	__vki_u32	cmd;
	__vki_u32	supported;
	__vki_u32	advertising;
	__vki_u16	speed;
	__vki_u8	duplex;
	__vki_u8	port;
	__vki_u8	phy_address;
	__vki_u8	transceiver;
	__vki_u8	autoneg;
	__vki_u8	mdio_support;
	__vki_u32	maxtxpkt;
	__vki_u32	maxrxpkt;
	__vki_u16	speed_hi;
	__vki_u8	eth_tp_mdix;
	__vki_u8	eth_tp_mdix_ctrl;
	__vki_u32	lp_advertising;
	__vki_u32	reserved[2];
};

#define VKI_ETHTOOL_FWVERS_LEN	32
#define VKI_ETHTOOL_BUSINFO_LEN	32

struct vki_ethtool_drvinfo {
	__vki_u32	cmd;
	char	driver[32];
	char	version[32];
	char	fw_version[VKI_ETHTOOL_FWVERS_LEN];
	char	bus_info[VKI_ETHTOOL_BUSINFO_LEN];
	char	reserved1[32];
	char	reserved2[12];
	__vki_u32	n_priv_flags;
	__vki_u32	n_stats;
	__vki_u32	testinfo_len;
	__vki_u32	eedump_len;
	__vki_u32	regdump_len;
};

#define VKI_SOPASS_MAX	6

struct vki_ethtool_wolinfo {
	__vki_u32	cmd;
	__vki_u32	supported;
	__vki_u32	wolopts;
	__vki_u8	sopass[VKI_SOPASS_MAX];
};

struct vki_ethtool_value {
	__vki_u32	cmd;
	__vki_u32	data;
};

struct vki_ethtool_regs {
	__vki_u32	cmd;
	__vki_u32	version;
	__vki_u32	len;
	__vki_u8	data[0];
};

struct vki_ethtool_ringparam {
	__vki_u32	cmd;
	__vki_u32	rx_max_pending;
	__vki_u32	rx_mini_max_pending;
	__vki_u32	rx_jumbo_max_pending;
	__vki_u32	tx_max_pending;
	__vki_u32	rx_pending;
	__vki_u32	rx_mini_pending;
	__vki_u32	rx_jumbo_pending;
	__vki_u32	tx_pending;
};

struct vki_ethtool_channels {
	__vki_u32	cmd;
	__vki_u32	max_rx;
	__vki_u32	max_tx;
	__vki_u32	max_other;
	__vki_u32	max_combined;
	__vki_u32	rx_count;
	__vki_u32	tx_count;
	__vki_u32	other_count;
	__vki_u32	combined_count;
};

struct vki_ethtool_sset_info {
	__vki_u32	cmd;
	__vki_u32	reserved;
	__vki_u64	sset_mask;
	__vki_u32	data[0];
};

struct vki_ethtool_test {
	__vki_u32	cmd;
	__vki_u32	flags;
	__vki_u32	reserved;
	__vki_u32	len;
	__vki_u64	data[0];
};

struct vki_ethtool_perm_addr {
	__vki_u32	cmd;
	__vki_u32	size;
	__vki_u8	data[0];
};

struct vki_ethtool_get_features_block {
	__vki_u32	available;
	__vki_u32	requested;
	__vki_u32	active;
	__vki_u32	never_changed;
};

struct vki_ethtool_gfeatures {
	__vki_u32	cmd;
	__vki_u32	size;
	struct vki_ethtool_get_features_block features[0];
};

struct vki_ethtool_set_features_block {
	__vki_u32	valid;
	__vki_u32	requested;
};

struct vki_ethtool_sfeatures {
	__vki_u32	cmd;
	__vki_u32	size;
	struct vki_ethtool_set_features_block features[0];
};

struct vki_ethtool_ts_info {
	__vki_u32	cmd;
	__vki_u32	so_timestamping;
	__vki_s32	phc_index;
	__vki_u32	tx_types;
	__vki_u32	tx_reserved[3];
	__vki_u32	rx_filters;
	__vki_u32	rx_reserved[3];
};

#define VKI_ETHTOOL_GSET	0x00000001 
#define VKI_ETHTOOL_SSET	0x00000002 
#define VKI_ETHTOOL_GDRVINFO	0x00000003 
#define VKI_ETHTOOL_GREGS	0x00000004 
#define VKI_ETHTOOL_GWOL	0x00000005 
#define VKI_ETHTOOL_SWOL	0x00000006 
#define VKI_ETHTOOL_GMSGLVL	0x00000007 
#define VKI_ETHTOOL_SMSGLVL	0x00000008 
#define VKI_ETHTOOL_NWAY_RST	0x00000009 
#define VKI_ETHTOOL_GLINK	0x0000000a
#define VKI_ETHTOOL_GRINGPARAM	0x00000010 
#define VKI_ETHTOOL_SRINGPARAM	0x00000011 
#define VKI_ETHTOOL_GRXCSUM	0x00000014 
#define VKI_ETHTOOL_SRXCSUM	0x00000015 
#define VKI_ETHTOOL_GTXCSUM	0x00000016 
#define VKI_ETHTOOL_STXCSUM	0x00000017 
#define VKI_ETHTOOL_GSG		0x00000018 
#define VKI_ETHTOOL_SSG		0x00000019 
#define VKI_ETHTOOL_TEST	0x0000001a 
#define VKI_ETHTOOL_PHYS_ID	0x0000001c 
#define VKI_ETHTOOL_GTSO	0x0000001e 
#define VKI_ETHTOOL_STSO	0x0000001f 
#define VKI_ETHTOOL_GPERMADDR	0x00000020 
#define VKI_ETHTOOL_GUFO	0x00000021 
#define VKI_ETHTOOL_SUFO	0x00000022 
#define VKI_ETHTOOL_GGSO	0x00000023 
#define VKI_ETHTOOL_SGSO	0x00000024 
#define VKI_ETHTOOL_GFLAGS	0x00000025 
#define VKI_ETHTOOL_SFLAGS	0x00000026 
#define VKI_ETHTOOL_GGRO	0x0000002b 
#define VKI_ETHTOOL_SGRO	0x0000002c 
#define VKI_ETHTOOL_RESET	0x00000034 
#define VKI_ETHTOOL_GSSET_INFO	0x00000037 
#define VKI_ETHTOOL_GFEATURES	0x0000003a 
#define VKI_ETHTOOL_SFEATURES	0x0000003b 
#define VKI_ETHTOOL_GCHANNELS	0x0000003c 
#define VKI_ETHTOOL_SCHANNELS	0x0000003d 
#define VKI_ETHTOOL_GET_TS_INFO	0x00000041 


typedef int vki_ion_user_handle_t;

struct vki_ion_allocation_data {
        vki_size_t len;
        vki_size_t align;
        unsigned int heap_id_mask;
        unsigned int flags;
        vki_ion_user_handle_t handle;
};

struct vki_ion_fd_data {
        vki_ion_user_handle_t handle;
        int fd;
};

struct vki_ion_handle_data {
        vki_ion_user_handle_t handle;
};

struct vki_ion_custom_data {
        unsigned int cmd;
        unsigned long arg;
};

#define VKI_ION_IOC_MAGIC   'I'

#define VKI_ION_IOC_ALLOC \
   _VKI_IOWR(VKI_ION_IOC_MAGIC, 0, struct vki_ion_allocation_data)

#define VKI_ION_IOC_FREE \
   _VKI_IOWR(VKI_ION_IOC_MAGIC, 1, struct vki_ion_handle_data)

#define VKI_ION_IOC_MAP \
   _VKI_IOWR(VKI_ION_IOC_MAGIC, 2, struct vki_ion_fd_data)

#define VKI_ION_IOC_SHARE \
   _VKI_IOWR(VKI_ION_IOC_MAGIC, 4, struct vki_ion_fd_data)

#define VKI_ION_IOC_IMPORT \
   _VKI_IOWR(VKI_ION_IOC_MAGIC, 5, struct vki_ion_fd_data)

#define VKI_ION_IOC_SYNC \
   _VKI_IOWR(VKI_ION_IOC_MAGIC, 7, struct vki_ion_fd_data)

#define VKI_ION_IOC_CUSTOM \
   _VKI_IOWR(VKI_ION_IOC_MAGIC, 6, struct vki_ion_custom_data)


struct vki_sync_merge_data {
        __vki_s32 fd2;
        char      name[32];
        __vki_s32 fence;
};

struct vki_sync_pt_info {
        __vki_u32 len;
        char      obj_name[32];
        char      driver_name[32];
        __vki_s32 status;
        __vki_u64 timestamp_ns;
        __vki_u8  driver_data[0];
};

struct vki_sync_fence_info_data {
        __vki_u32 len;
        char      name[32];
        __vki_s32 status;
        __vki_u8  pt_info[0];
};

#define VKI_SYNC_IOC_MAGIC   '>'

#define VKI_SYNC_IOC_WAIT \
   _VKI_IOW(VKI_SYNC_IOC_MAGIC, 0, __vki_s32)

#define VKI_SYNC_IOC_MERGE \
   _VKI_IOWR(VKI_SYNC_IOC_MAGIC, 1, struct vki_sync_merge_data)

#define VKI_SYNC_IOC_FENCE_INFO \
   _VKI_IOWR(VKI_SYNC_IOC_MAGIC, 2, struct vki_sync_fence_info_data)


struct vki_lu_fid {
	__vki_u64	f_seq;
	__vki_u32	f_oid;
	__vki_u32	f_ver;
};


struct vki_getinfo_fid2path {
	struct vki_lu_fid	gf_fid;
	__vki_u64		gf_recno;
	__vki_u32		gf_linkno;
	__vki_u32		gf_pathlen;
	char			gf_path[0];
} __attribute__((packed));


#define VKI_OBD_IOC_DATA_TYPE               long


#define VKI_OBD_IOC_FID2PATH \
           _VKI_IOWR ('f', 150, VKI_OBD_IOC_DATA_TYPE)
#define VKI_LL_IOC_PATH2FID \
           _VKI_IOR ('f', 173, long)


struct vki_getparent {
    struct vki_lu_fid   gp_fid;
    __vki_u32       gp_linkno;
    __vki_u32       gp_name_size;
    char            gp_name[0];
} __attribute__((packed));

#define VKI_LL_IOC_GETPARENT \
           _VKI_IOWR('f', 249, struct vki_getparent)


struct vki_v4l2_rect {
	__vki_s32   left;
	__vki_s32   top;
	__vki_u32   width;
	__vki_u32   height;
};

struct vki_v4l2_fract {
	__vki_u32   numerator;
	__vki_u32   denominator;
};

struct vki_v4l2_capability {
	__vki_u8	driver[16];
	__vki_u8	card[32];
	__vki_u8	bus_info[32];
	__vki_u32   version;
	__vki_u32	capabilities;
	__vki_u32	device_caps;
	__vki_u32	reserved[3];
};

struct vki_v4l2_pix_format {
	__vki_u32         		width;
	__vki_u32			height;
	__vki_u32			pixelformat;
	__vki_u32			field;		
	__vki_u32            	bytesperline;	
	__vki_u32          		sizeimage;
	__vki_u32			colorspace;	
	__vki_u32			priv;		
	__vki_u32			flags;		
	__vki_u32			ycbcr_enc;
	__vki_u32			quantization;
};

struct vki_v4l2_fmtdesc {
	__vki_u32		    index;             
	__vki_u32		    type;              
	__vki_u32               flags;
	__vki_u8		    description[32];   
	__vki_u32		    pixelformat;       
	__vki_u32		    reserved[4];
};

struct vki_v4l2_frmsize_discrete {
	__vki_u32			width;		
	__vki_u32			height;		
};

struct vki_v4l2_frmsize_stepwise {
	__vki_u32			min_width;	
	__vki_u32			max_width;	
	__vki_u32			step_width;	
	__vki_u32			min_height;	
	__vki_u32			max_height;	
	__vki_u32			step_height;	
};

struct vki_v4l2_frmsizeenum {
	__vki_u32			index;		
	__vki_u32			pixel_format;	
	__vki_u32			type;		

	union {					
		struct vki_v4l2_frmsize_discrete	discrete;
		struct vki_v4l2_frmsize_stepwise	stepwise;
	};

	__vki_u32   reserved[2];			
};

struct vki_v4l2_frmival_stepwise {
	struct vki_v4l2_fract	min;
	struct vki_v4l2_fract	max;
	struct vki_v4l2_fract	step;
};

struct vki_v4l2_frmivalenum {
	__vki_u32			index;
	__vki_u32			pixel_format;
	__vki_u32			width;
	__vki_u32			height;
	__vki_u32			type;

	union {
		struct vki_v4l2_fract		discrete;
		struct vki_v4l2_frmival_stepwise	stepwise;
	};

	__vki_u32	reserved[2];
};

struct vki_v4l2_timecode {
	__vki_u32	type;
	__vki_u32	flags;
	__vki_u8	frames;
	__vki_u8	seconds;
	__vki_u8	minutes;
	__vki_u8	hours;
	__vki_u8	userbits[4];
};

struct vki_v4l2_jpegcompression {
	int quality;
	int  APPn;
	int  APP_len;
	char APP_data[60];
	int  COM_len;
	char COM_data[60];
	__vki_u32 jpeg_markers;
};

struct vki_v4l2_requestbuffers {
	__vki_u32			count;
	__vki_u32			type;
	__vki_u32			memory;
	__vki_u32			reserved[2];
};

struct vki_v4l2_plane {
	__vki_u32			bytesused;
	__vki_u32			length;
	union {
		__vki_u32		mem_offset;
		unsigned long	userptr;
		__vki_s32		fd;
	} m;
	__vki_u32			data_offset;
	__vki_u32			reserved[11];
};

#define VKI_V4L2_MEMORY_MMAP             1
#define VKI_V4L2_MEMORY_DMABUF           4
#define VKI_V4L2_BUF_FLAG_TIMESTAMP_MASK		0x0000e000
#define VKI_V4L2_BUF_FLAG_TIMESTAMP_COPY		0x00004000
struct vki_v4l2_buffer {
	__vki_u32			index;
	__vki_u32			type;
	__vki_u32			bytesused;
	__vki_u32			flags;
	__vki_u32			field;
	struct vki_timeval		timestamp;
	struct vki_v4l2_timecode	timecode;
	__vki_u32			sequence;

	
	__vki_u32			memory;
	union {
		__vki_u32           offset;
		unsigned long   userptr;
		struct vki_v4l2_plane *planes;
		__vki_s32		fd;
	} m;
	__vki_u32			length;
	__vki_u32			reserved2;
	__vki_u32			reserved;
};

struct vki_v4l2_exportbuffer {
	__vki_u32		type; 
	__vki_u32		index;
	__vki_u32		plane;
	__vki_u32		flags;
	__vki_s32		fd;
	__vki_u32		reserved[11];
};

struct vki_v4l2_framebuffer {
	__vki_u32			capability;
	__vki_u32			flags;
	void                    *base;
	struct {
		__vki_u32		width;
		__vki_u32		height;
		__vki_u32		pixelformat;
		__vki_u32		field;		
		__vki_u32		bytesperline;	
		__vki_u32		sizeimage;
		__vki_u32		colorspace;	
		__vki_u32		priv;		
	} fmt;
};

struct vki_v4l2_clip {
	struct vki_v4l2_rect        c;
	struct vki_v4l2_clip	__user *next;
};

struct vki_v4l2_window {
	struct vki_v4l2_rect        w;
	__vki_u32			field;	 
	__vki_u32			chromakey;
	struct vki_v4l2_clip	__user *clips;
	__vki_u32			clipcount;
	void			__user *bitmap;
	__vki_u8                    global_alpha;
};

struct vki_v4l2_captureparm {
	__vki_u32		   capability;	  
	__vki_u32		   capturemode;	  
	struct vki_v4l2_fract  timeperframe;  
	__vki_u32		   extendedmode;  
	__vki_u32              readbuffers;   
	__vki_u32		   reserved[4];
};

struct vki_v4l2_outputparm {
	__vki_u32		   capability;	 
	__vki_u32		   outputmode;	 
	struct vki_v4l2_fract  timeperframe; 
	__vki_u32		   extendedmode; 
	__vki_u32              writebuffers; 
	__vki_u32		   reserved[4];
};

struct vki_v4l2_cropcap {
	__vki_u32			type;	
	struct vki_v4l2_rect        bounds;
	struct vki_v4l2_rect        defrect;
	struct vki_v4l2_fract       pixelaspect;
};

struct vki_v4l2_crop {
	__vki_u32			type;	
	struct vki_v4l2_rect        c;
};

struct vki_v4l2_selection {
	__vki_u32			type;
	__vki_u32			target;
	__vki_u32                   flags;
	struct vki_v4l2_rect        r;
	__vki_u32                   reserved[9];
};

typedef __vki_u64 vki_v4l2_std_id;

struct vki_v4l2_standard {
	__vki_u32		     index;
	vki_v4l2_std_id          id;
	__vki_u8		     name[24];
	struct vki_v4l2_fract    frameperiod; 
	__vki_u32		     framelines;
	__vki_u32		     reserved[4];
};

struct vki_v4l2_bt_timings {
	__vki_u32	width;
	__vki_u32	height;
	__vki_u32	interlaced;
	__vki_u32	polarities;
	__vki_u64	pixelclock;
	__vki_u32	hfrontporch;
	__vki_u32	hsync;
	__vki_u32	hbackporch;
	__vki_u32	vfrontporch;
	__vki_u32	vsync;
	__vki_u32	vbackporch;
	__vki_u32	il_vfrontporch;
	__vki_u32	il_vsync;
	__vki_u32	il_vbackporch;
	__vki_u32	standards;
	__vki_u32	flags;
	__vki_u32	reserved[14];
} __attribute__ ((packed));

struct vki_v4l2_dv_timings {
	__vki_u32 type;
	union {
		struct vki_v4l2_bt_timings	bt;
		__vki_u32	reserved[32];
	};
} __attribute__ ((packed));

struct vki_v4l2_enum_dv_timings {
	__vki_u32 index;
	__vki_u32 pad;
	__vki_u32 reserved[2];
	struct vki_v4l2_dv_timings timings;
};

struct vki_v4l2_bt_timings_cap {
	__vki_u32	min_width;
	__vki_u32	max_width;
	__vki_u32	min_height;
	__vki_u32	max_height;
	__vki_u64	min_pixelclock;
	__vki_u64	max_pixelclock;
	__vki_u32	standards;
	__vki_u32	capabilities;
	__vki_u32	reserved[16];
} __attribute__ ((packed));

struct vki_v4l2_dv_timings_cap {
	__vki_u32 type;
	__vki_u32 pad;
	__vki_u32 reserved[2];
	union {
		struct vki_v4l2_bt_timings_cap bt;
		__vki_u32 raw_data[32];
	};
};

struct vki_v4l2_input {
	__vki_u32	     index;		
	__vki_u8	     name[32];		
	__vki_u32	     type;		
	__vki_u32	     audioset;		
	__vki_u32        tuner;             
	vki_v4l2_std_id  std;
	__vki_u32	     status;
	__vki_u32	     capabilities;
	__vki_u32	     reserved[3];
};

struct vki_v4l2_output {
	__vki_u32	     index;		
	__vki_u8	     name[32];		
	__vki_u32	     type;		
	__vki_u32	     audioset;		
	__vki_u32	     modulator;         
	vki_v4l2_std_id  std;
	__vki_u32	     capabilities;
	__vki_u32	     reserved[3];
};

struct vki_v4l2_control {
	__vki_u32		     id;
	__vki_s32		     value;
};

struct vki_v4l2_ext_control {
	__vki_u32 id;
	__vki_u32 size;
	__vki_u32 reserved2[1];
	union {
		__vki_s32 value;
		__vki_s64 value64;
		char *string;
		__vki_u8 *p_u8;
		__vki_u16 *p_u16;
		__vki_u32 *p_u32;
		void *ptr;
	};
} __attribute__ ((packed));

struct vki_v4l2_ext_controls {
	__vki_u32 ctrl_class;
	__vki_u32 count;
	__vki_u32 error_idx;
	__vki_u32 reserved[2];
	struct vki_v4l2_ext_control *controls;
};

struct vki_v4l2_queryctrl {
	__vki_u32		     id;
	__vki_u32		     type;	
	__vki_u8		     name[32];	
	__vki_s32		     minimum;	
	__vki_s32		     maximum;
	__vki_s32		     step;
	__vki_s32		     default_value;
	__vki_u32                flags;
	__vki_u32		     reserved[2];
};

#define VKI_V4L2_CTRL_MAX_DIMS	  (4)
struct vki_v4l2_query_ext_ctrl {
	__vki_u32		     id;
	__vki_u32		     type;
	char		     name[32];
	__vki_s64		     minimum;
	__vki_s64		     maximum;
	__vki_u64		     step;
	__vki_s64		     default_value;
	__vki_u32                flags;
	__vki_u32                elem_size;
	__vki_u32                elems;
	__vki_u32                nr_of_dims;
	__vki_u32                dims[VKI_V4L2_CTRL_MAX_DIMS];
	__vki_u32		     reserved[32];
};

struct vki_v4l2_querymenu {
	__vki_u32		id;
	__vki_u32		index;
	union {
		__vki_u8	name[32];	
		__vki_s64	value;
	};
	__vki_u32		reserved;
} __attribute__ ((packed));

struct vki_v4l2_tuner {
	__vki_u32                   index;
	__vki_u8			name[32];
	__vki_u32			type;	
	__vki_u32			capability;
	__vki_u32			rangelow;
	__vki_u32			rangehigh;
	__vki_u32			rxsubchans;
	__vki_u32			audmode;
	__vki_s32			signal;
	__vki_s32			afc;
	__vki_u32			reserved[4];
};

struct vki_v4l2_modulator {
	__vki_u32			index;
	__vki_u8			name[32];
	__vki_u32			capability;
	__vki_u32			rangelow;
	__vki_u32			rangehigh;
	__vki_u32			txsubchans;
	__vki_u32			reserved[4];
};

struct vki_v4l2_frequency {
	__vki_u32	tuner;
	__vki_u32	type;	
	__vki_u32	frequency;
	__vki_u32	reserved[8];
};

struct vki_v4l2_frequency_band {
	__vki_u32	tuner;
	__vki_u32	type;	
	__vki_u32	index;
	__vki_u32	capability;
	__vki_u32	rangelow;
	__vki_u32	rangehigh;
	__vki_u32	modulation;
	__vki_u32	reserved[9];
};

struct vki_v4l2_hw_freq_seek {
	__vki_u32	tuner;
	__vki_u32	type;	
	__vki_u32	seek_upward;
	__vki_u32	wrap_around;
	__vki_u32	spacing;
	__vki_u32	rangelow;
	__vki_u32	rangehigh;
	__vki_u32	reserved[5];
};

struct vki_v4l2_audio {
	__vki_u32	index;
	__vki_u8	name[32];
	__vki_u32	capability;
	__vki_u32	mode;
	__vki_u32	reserved[2];
};

struct vki_v4l2_audioout {
	__vki_u32	index;
	__vki_u8	name[32];
	__vki_u32	capability;
	__vki_u32	mode;
	__vki_u32	reserved[2];
};

struct vki_v4l2_enc_idx_entry {
	__vki_u64 offset;
	__vki_u64 pts;
	__vki_u32 length;
	__vki_u32 flags;
	__vki_u32 reserved[2];
};

#define VKI_V4L2_ENC_IDX_ENTRIES (64)
struct vki_v4l2_enc_idx {
	__vki_u32 entries;
	__vki_u32 entries_cap;
	__vki_u32 reserved[4];
	struct vki_v4l2_enc_idx_entry entry[VKI_V4L2_ENC_IDX_ENTRIES];
};

struct vki_v4l2_encoder_cmd {
	__vki_u32 cmd;
	__vki_u32 flags;
	union {
		struct {
			__vki_u32 data[8];
		} raw;
	};
};

struct vki_v4l2_decoder_cmd {
	__vki_u32 cmd;
	__vki_u32 flags;
	union {
		struct {
			__vki_u64 pts;
		} stop;

		struct {
			__vki_s32 speed;
			__vki_u32 format;
		} start;

		struct {
			__vki_u32 data[16];
		} raw;
	};
};

struct vki_v4l2_vbi_format {
	__vki_u32	sampling_rate;		
	__vki_u32	offset;
	__vki_u32	samples_per_line;
	__vki_u32	sample_format;		
	__vki_s32	start[2];
	__vki_u32	count[2];
	__vki_u32	flags;			
	__vki_u32	reserved[2];		
};

struct vki_v4l2_sliced_vbi_format {
	__vki_u16   service_set;
	__vki_u16   service_lines[2][24];
	__vki_u32   io_size;
	__vki_u32   reserved[2];            
};

struct vki_v4l2_sliced_vbi_cap {
	__vki_u16   service_set;
	__vki_u16   service_lines[2][24];
	__vki_u32	type;		
	__vki_u32   reserved[3];    
};

struct vki_v4l2_sliced_vbi_data {
	__vki_u32   id;
	__vki_u32   field;          
	__vki_u32   line;           
	__vki_u32   reserved;       
	__vki_u8    data[48];
};

struct vki_v4l2_plane_pix_format {
	__vki_u32		sizeimage;
	__vki_u16		bytesperline;
	__vki_u16		reserved[7];
} __attribute__ ((packed));

#define VKI_VIDEO_MAX_PLANES               8

struct vki_v4l2_pix_format_mplane {
	__vki_u32				width;
	__vki_u32				height;
	__vki_u32				pixelformat;
	__vki_u32				field;
	__vki_u32				colorspace;

	struct vki_v4l2_plane_pix_format	plane_fmt[VKI_VIDEO_MAX_PLANES];
	__vki_u8				num_planes;
	__vki_u8				flags;
	__vki_u8				ycbcr_enc;
	__vki_u8				quantization;
	__vki_u8				reserved[8];
} __attribute__ ((packed));

struct vki_v4l2_sdr_format {
	__vki_u32				pixelformat;
	__vki_u32				buffersize;
	__vki_u8				reserved[24];
} __attribute__ ((packed));

enum vki_v4l2_buf_type {
	VKI_V4L2_BUF_TYPE_VIDEO_CAPTURE        = 1,
	VKI_V4L2_BUF_TYPE_VIDEO_OUTPUT         = 2,
	VKI_V4L2_BUF_TYPE_VIDEO_OVERLAY        = 3,
	VKI_V4L2_BUF_TYPE_VBI_CAPTURE          = 4,
	VKI_V4L2_BUF_TYPE_VBI_OUTPUT           = 5,
	VKI_V4L2_BUF_TYPE_SLICED_VBI_CAPTURE   = 6,
	VKI_V4L2_BUF_TYPE_SLICED_VBI_OUTPUT    = 7,
	VKI_V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY = 8,
	VKI_V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 9,
	VKI_V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE  = 10,
	VKI_V4L2_BUF_TYPE_SDR_CAPTURE          = 11,
};

struct vki_v4l2_format {
	__vki_u32	 type;
	union {
		struct vki_v4l2_pix_format		pix;
		struct vki_v4l2_pix_format_mplane	pix_mp;
		struct vki_v4l2_window		win;
		struct vki_v4l2_vbi_format		vbi;
		struct vki_v4l2_sliced_vbi_format	sliced;
		struct vki_v4l2_sdr_format		sdr;
		__vki_u8	raw_data[200];
	} fmt;
};

struct vki_v4l2_streamparm {
	__vki_u32	 type;
	union {
		struct vki_v4l2_captureparm	capture;
		struct vki_v4l2_outputparm	output;
		__vki_u8	raw_data[200];  
	} parm;
};

struct vki_v4l2_event_vsync {
	__vki_u8 field;
} __attribute__ ((packed));

struct vki_v4l2_event_ctrl {
	__vki_u32 changes;
	__vki_u32 type;
	union {
		__vki_s32 value;
		__vki_s64 value64;
	};
	__vki_u32 flags;
	__vki_s32 minimum;
	__vki_s32 maximum;
	__vki_s32 step;
	__vki_s32 default_value;
};

struct vki_v4l2_event_frame_sync {
	__vki_u32 frame_sequence;
};

struct vki_v4l2_event_src_change {
	__vki_u32 changes;
};

struct vki_v4l2_event_motion_det {
	__vki_u32 flags;
	__vki_u32 frame_sequence;
	__vki_u32 region_mask;
};

struct vki_v4l2_event {
	__vki_u32				type;
	union {
		struct vki_v4l2_event_vsync		vsync;
		struct vki_v4l2_event_ctrl		ctrl;
		struct vki_v4l2_event_frame_sync	frame_sync;
		struct vki_v4l2_event_src_change	src_change;
		struct vki_v4l2_event_motion_det	motion_det;
		__vki_u8				data[64];
	} u;
	__vki_u32				pending;
	__vki_u32				sequence;
	struct vki_timespec			timestamp;
	__vki_u32				id;
	__vki_u32				reserved[8];
};

struct vki_v4l2_event_subscription {
	__vki_u32				type;
	__vki_u32				id;
	__vki_u32				flags;
	__vki_u32				reserved[5];
};

struct vki_v4l2_dbg_match {
	__vki_u32 type; 
	union {     
		__vki_u32 addr;
		char name[32];
	};
} __attribute__ ((packed));

struct vki_v4l2_dbg_register {
	struct vki_v4l2_dbg_match match;
	__vki_u32 size;	
	__vki_u64 reg;
	__vki_u64 val;
} __attribute__ ((packed));

struct vki_v4l2_dbg_chip_info {
	struct vki_v4l2_dbg_match match;
	char name[32];
	__vki_u32 flags;
	__vki_u32 reserved[32];
} __attribute__ ((packed));

struct vki_v4l2_create_buffers {
	__vki_u32			index;
	__vki_u32			count;
	__vki_u32			memory;
	struct vki_v4l2_format	format;
	__vki_u32			reserved[8];
};

struct vki_v4l2_edid {
	__vki_u32 pad;
	__vki_u32 start_block;
	__vki_u32 blocks;
	__vki_u32 reserved[5];
	__vki_u8  *edid;
};

#define VKI_V4L2_QUERYCAP		_VKI_IOR('V',  0, struct vki_v4l2_capability)
#define VKI_V4L2_ENUM_FMT		_VKI_IOWR('V',  2, struct vki_v4l2_fmtdesc)
#define VKI_V4L2_G_FMT			_VKI_IOWR('V',  4, struct vki_v4l2_format)
#define VKI_V4L2_S_FMT			_VKI_IOWR('V',  5, struct vki_v4l2_format)
#define VKI_V4L2_REQBUFS		_VKI_IOWR('V',  8, struct vki_v4l2_requestbuffers)
#define VKI_V4L2_QUERYBUF		_VKI_IOWR('V',  9, struct vki_v4l2_buffer)
#define VKI_V4L2_G_FBUF		 	_VKI_IOR('V', 10, struct vki_v4l2_framebuffer)
#define VKI_V4L2_S_FBUF		 	_VKI_IOW('V', 11, struct vki_v4l2_framebuffer)
#define VKI_V4L2_OVERLAY	 	_VKI_IOW('V', 14, int)
#define VKI_V4L2_QBUF			_VKI_IOWR('V', 15, struct vki_v4l2_buffer)
#define VKI_V4L2_EXPBUF			_VKI_IOWR('V', 16, struct vki_v4l2_exportbuffer)
#define VKI_V4L2_DQBUF			_VKI_IOWR('V', 17, struct vki_v4l2_buffer)
#define VKI_V4L2_STREAMON	 	_VKI_IOW('V', 18, int)
#define VKI_V4L2_STREAMOFF	 	_VKI_IOW('V', 19, int)
#define VKI_V4L2_G_PARM			_VKI_IOWR('V', 21, struct vki_v4l2_streamparm)
#define VKI_V4L2_S_PARM			_VKI_IOWR('V', 22, struct vki_v4l2_streamparm)
#define VKI_V4L2_G_STD			_VKI_IOR('V', 23, vki_v4l2_std_id)
#define VKI_V4L2_S_STD			_VKI_IOW('V', 24, vki_v4l2_std_id)
#define VKI_V4L2_ENUMSTD		_VKI_IOWR('V', 25, struct vki_v4l2_standard)
#define VKI_V4L2_ENUMINPUT		_VKI_IOWR('V', 26, struct vki_v4l2_input)
#define VKI_V4L2_G_CTRL			_VKI_IOWR('V', 27, struct vki_v4l2_control)
#define VKI_V4L2_S_CTRL			_VKI_IOWR('V', 28, struct vki_v4l2_control)
#define VKI_V4L2_G_TUNER		_VKI_IOWR('V', 29, struct vki_v4l2_tuner)
#define VKI_V4L2_S_TUNER		_VKI_IOW('V', 30, struct vki_v4l2_tuner)
#define VKI_V4L2_G_AUDIO		_VKI_IOR('V', 33, struct vki_v4l2_audio)
#define VKI_V4L2_S_AUDIO		_VKI_IOW('V', 34, struct vki_v4l2_audio)
#define VKI_V4L2_QUERYCTRL		_VKI_IOWR('V', 36, struct vki_v4l2_queryctrl)
#define VKI_V4L2_QUERYMENU		_VKI_IOWR('V', 37, struct vki_v4l2_querymenu)
#define VKI_V4L2_G_INPUT		_VKI_IOR('V', 38, int)
#define VKI_V4L2_S_INPUT		_VKI_IOWR('V', 39, int)
#define VKI_V4L2_G_EDID			_VKI_IOWR('V', 40, struct vki_v4l2_edid)
#define VKI_V4L2_S_EDID			_VKI_IOWR('V', 41, struct vki_v4l2_edid)
#define VKI_V4L2_G_OUTPUT		_VKI_IOR('V', 46, int)
#define VKI_V4L2_S_OUTPUT		_VKI_IOWR('V', 47, int)
#define VKI_V4L2_ENUMOUTPUT		_VKI_IOWR('V', 48, struct vki_v4l2_output)
#define VKI_V4L2_G_AUDOUT		_VKI_IOR('V', 49, struct vki_v4l2_audioout)
#define VKI_V4L2_S_AUDOUT		_VKI_IOW('V', 50, struct vki_v4l2_audioout)
#define VKI_V4L2_G_MODULATOR		_VKI_IOWR('V', 54, struct vki_v4l2_modulator)
#define VKI_V4L2_S_MODULATOR		_VKI_IOW('V', 55, struct vki_v4l2_modulator)
#define VKI_V4L2_G_FREQUENCY		_VKI_IOWR('V', 56, struct vki_v4l2_frequency)
#define VKI_V4L2_S_FREQUENCY		_VKI_IOW('V', 57, struct vki_v4l2_frequency)
#define VKI_V4L2_CROPCAP		_VKI_IOWR('V', 58, struct vki_v4l2_cropcap)
#define VKI_V4L2_G_CROP			_VKI_IOWR('V', 59, struct vki_v4l2_crop)
#define VKI_V4L2_S_CROP			_VKI_IOW('V', 60, struct vki_v4l2_crop)
#define VKI_V4L2_G_JPEGCOMP		_VKI_IOR('V', 61, struct vki_v4l2_jpegcompression)
#define VKI_V4L2_S_JPEGCOMP		_VKI_IOW('V', 62, struct vki_v4l2_jpegcompression)
#define VKI_V4L2_QUERYSTD      		_VKI_IOR('V', 63, vki_v4l2_std_id)
#define VKI_V4L2_TRY_FMT      		_VKI_IOWR('V', 64, struct vki_v4l2_format)
#define VKI_V4L2_ENUMAUDIO		_VKI_IOWR('V', 65, struct vki_v4l2_audio)
#define VKI_V4L2_ENUMAUDOUT		_VKI_IOWR('V', 66, struct vki_v4l2_audioout)
#define VKI_V4L2_G_PRIORITY		_VKI_IOR('V', 67, __vki_u32)
#define VKI_V4L2_S_PRIORITY		_VKI_IOW('V', 68, __vki_u32)
#define VKI_V4L2_G_SLICED_VBI_CAP 	_VKI_IOWR('V', 69, struct vki_v4l2_sliced_vbi_cap)
#define VKI_V4L2_LOG_STATUS     	_VKI_IO('V', 70)
#define VKI_V4L2_G_EXT_CTRLS		_VKI_IOWR('V', 71, struct vki_v4l2_ext_controls)
#define VKI_V4L2_S_EXT_CTRLS		_VKI_IOWR('V', 72, struct vki_v4l2_ext_controls)
#define VKI_V4L2_TRY_EXT_CTRLS		_VKI_IOWR('V', 73, struct vki_v4l2_ext_controls)
#define VKI_V4L2_ENUM_FRAMESIZES	_VKI_IOWR('V', 74, struct vki_v4l2_frmsizeenum)
#define VKI_V4L2_ENUM_FRAMEINTERVALS 	_VKI_IOWR('V', 75, struct vki_v4l2_frmivalenum)
#define VKI_V4L2_G_ENC_INDEX    	_VKI_IOR('V', 76, struct vki_v4l2_enc_idx)
#define VKI_V4L2_ENCODER_CMD    	_VKI_IOWR('V', 77, struct vki_v4l2_encoder_cmd)
#define VKI_V4L2_TRY_ENCODER_CMD 	_VKI_IOWR('V', 78, struct vki_v4l2_encoder_cmd)
#define	VKI_V4L2_DBG_S_REGISTER 	_VKI_IOW('V', 79, struct vki_v4l2_dbg_register)
#define	VKI_V4L2_DBG_G_REGISTER 	_VKI_IOWR('V', 80, struct vki_v4l2_dbg_register)
#define VKI_V4L2_S_HW_FREQ_SEEK		_VKI_IOW('V', 82, struct vki_v4l2_hw_freq_seek)
#define	VKI_V4L2_S_DV_TIMINGS		_VKI_IOWR('V', 87, struct vki_v4l2_dv_timings)
#define	VKI_V4L2_G_DV_TIMINGS		_VKI_IOWR('V', 88, struct vki_v4l2_dv_timings)
#define	VKI_V4L2_DQEVENT		_VKI_IOR('V', 89, struct vki_v4l2_event)
#define	VKI_V4L2_SUBSCRIBE_EVENT	_VKI_IOW('V', 90, struct vki_v4l2_event_subscription)
#define	VKI_V4L2_UNSUBSCRIBE_EVENT 	_VKI_IOW('V', 91, struct vki_v4l2_event_subscription)
#define VKI_V4L2_CREATE_BUFS		_VKI_IOWR('V', 92, struct vki_v4l2_create_buffers)
#define VKI_V4L2_PREPARE_BUF		_VKI_IOWR('V', 93, struct vki_v4l2_buffer)
#define VKI_V4L2_G_SELECTION		_VKI_IOWR('V', 94, struct vki_v4l2_selection)
#define VKI_V4L2_S_SELECTION		_VKI_IOWR('V', 95, struct vki_v4l2_selection)
#define VKI_V4L2_DECODER_CMD		_VKI_IOWR('V', 96, struct vki_v4l2_decoder_cmd)
#define VKI_V4L2_TRY_DECODER_CMD	_VKI_IOWR('V', 97, struct vki_v4l2_decoder_cmd)
#define VKI_V4L2_ENUM_DV_TIMINGS 	_VKI_IOWR('V', 98, struct vki_v4l2_enum_dv_timings)
#define VKI_V4L2_QUERY_DV_TIMINGS 	_VKI_IOR('V', 99, struct vki_v4l2_dv_timings)
#define VKI_V4L2_DV_TIMINGS_CAP   	_VKI_IOWR('V', 100, struct vki_v4l2_dv_timings_cap)
#define VKI_V4L2_ENUM_FREQ_BANDS	_VKI_IOWR('V', 101, struct vki_v4l2_frequency_band)
#define VKI_V4L2_DBG_G_CHIP_INFO 	_VKI_IOWR('V', 102, struct vki_v4l2_dbg_chip_info)
#define VKI_V4L2_QUERY_EXT_CTRL		_VKI_IOWR('V', 103, struct vki_v4l2_query_ext_ctrl)

struct vki_v4l2_mbus_framefmt {
	__vki_u32			width;
	__vki_u32			height;
	__vki_u32			code;
	__vki_u32			field;
	__vki_u32			colorspace;
	__vki_u16			ycbcr_enc;
	__vki_u16			quantization;
	__vki_u32			reserved[6];
};

struct vki_v4l2_subdev_format {
	__vki_u32 which;
	__vki_u32 pad;
	struct vki_v4l2_mbus_framefmt format;
	__vki_u32 reserved[8];
};

struct vki_v4l2_subdev_crop {
	__vki_u32 which;
	__vki_u32 pad;
	struct vki_v4l2_rect rect;
	__vki_u32 reserved[8];
};

struct vki_v4l2_subdev_mbus_code_enum {
	__vki_u32 pad;
	__vki_u32 index;
	__vki_u32 code;
	__vki_u32 reserved[9];
};

struct vki_v4l2_subdev_frame_size_enum {
	__vki_u32 index;
	__vki_u32 pad;
	__vki_u32 code;
	__vki_u32 min_width;
	__vki_u32 max_width;
	__vki_u32 min_height;
	__vki_u32 max_height;
	__vki_u32 reserved[9];
};

struct vki_v4l2_subdev_frame_interval {
	__vki_u32 pad;
	struct vki_v4l2_fract interval;
	__vki_u32 reserved[9];
};

struct vki_v4l2_subdev_frame_interval_enum {
	__vki_u32 index;
	__vki_u32 pad;
	__vki_u32 code;
	__vki_u32 width;
	__vki_u32 height;
	struct vki_v4l2_fract interval;
	__vki_u32 reserved[9];
};

struct vki_v4l2_subdev_selection {
	__vki_u32 which;
	__vki_u32 pad;
	__vki_u32 target;
	__vki_u32 flags;
	struct vki_v4l2_rect r;
	__vki_u32 reserved[8];
};

#define VKI_V4L2_SUBDEV_G_FMT			_VKI_IOWR('V',  4, struct vki_v4l2_subdev_format)
#define VKI_V4L2_SUBDEV_S_FMT			_VKI_IOWR('V',  5, struct vki_v4l2_subdev_format)
#define VKI_V4L2_SUBDEV_G_FRAME_INTERVAL	_VKI_IOWR('V', 21, struct vki_v4l2_subdev_frame_interval)
#define VKI_V4L2_SUBDEV_S_FRAME_INTERVAL	_VKI_IOWR('V', 22, struct vki_v4l2_subdev_frame_interval)
#define VKI_V4L2_SUBDEV_ENUM_MBUS_CODE		_VKI_IOWR('V',  2, struct vki_v4l2_subdev_mbus_code_enum)
#define VKI_V4L2_SUBDEV_ENUM_FRAME_SIZE		_VKI_IOWR('V', 74, struct vki_v4l2_subdev_frame_size_enum)
#define VKI_V4L2_SUBDEV_ENUM_FRAME_INTERVAL	_VKI_IOWR('V', 75, struct vki_v4l2_subdev_frame_interval_enum)
#define VKI_V4L2_SUBDEV_G_CROP			_VKI_IOWR('V', 59, struct vki_v4l2_subdev_crop)
#define VKI_V4L2_SUBDEV_S_CROP			_VKI_IOWR('V', 60, struct vki_v4l2_subdev_crop)
#define VKI_V4L2_SUBDEV_G_SELECTION		_VKI_IOWR('V', 61, struct vki_v4l2_subdev_selection)
#define VKI_V4L2_SUBDEV_S_SELECTION		_VKI_IOWR('V', 62, struct vki_v4l2_subdev_selection)

struct vki_media_device_info {
	char driver[16];
	char model[32];
	char serial[40];
	char bus_info[32];
	__vki_u32 media_version;
	__vki_u32 hw_revision;
	__vki_u32 driver_version;
	__vki_u32 reserved[31];
};

struct vki_media_entity_desc {
	__vki_u32 id;
	char name[32];
	__vki_u32 type;
	__vki_u32 revision;
	__vki_u32 flags;
	__vki_u32 group_id;
	__vki_u16 pads;
	__vki_u16 links;

	__vki_u32 reserved[4];

	union {
		
		struct {
			__vki_u32 major;
			__vki_u32 minor;
		} v4l;
		struct {
			__vki_u32 major;
			__vki_u32 minor;
		} fb;
		struct {
			__vki_u32 card;
			__vki_u32 device;
			__vki_u32 subdevice;
		} alsa;
		int dvb;

		
		
		__vki_u8 raw[184];
	};
};

struct vki_media_pad_desc {
	__vki_u32 entity;		
	__vki_u16 index;		
	__vki_u32 flags;		
	__vki_u32 reserved[2];
};

struct vki_media_link_desc {
	struct vki_media_pad_desc source;
	struct vki_media_pad_desc sink;
	__vki_u32 flags;
	__vki_u32 reserved[2];
};

struct vki_media_links_enum {
	__vki_u32 entity;
	struct vki_media_pad_desc __user *pads;
	struct vki_media_link_desc __user *links;
	__vki_u32 reserved[4];
};

#define VKI_MEDIA_IOC_DEVICE_INFO		_VKI_IOWR('|', 0x00, struct vki_media_device_info)
#define VKI_MEDIA_IOC_ENUM_ENTITIES		_VKI_IOWR('|', 0x01, struct vki_media_entity_desc)
#define VKI_MEDIA_IOC_ENUM_LINKS		_VKI_IOWR('|', 0x02, struct vki_media_links_enum)
#define VKI_MEDIA_IOC_SETUP_LINK		_VKI_IOWR('|', 0x03, struct vki_media_link_desc)

enum vki_kcmp_type {
   VKI_KCMP_FILE,
   VKI_KCMP_VM,
   VKI_KCMP_FILES,
   VKI_KCMP_FS,
   VKI_KCMP_SIGHAND,
   VKI_KCMP_IO,
   VKI_KCMP_SYSVSEM,

   VKI_KCMP_TYPES
};


#define VKI_SECCOMP_MODE_FILTER 2

#define VKI_BINPRM_BUF_SIZE 128

#endif 


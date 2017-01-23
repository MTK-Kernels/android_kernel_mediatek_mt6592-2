











#define GDB_PATH "/usr/bin/gdb"

#ifndef __ANDROID__
#define GLIBC_MANDATORY_INDEX_AND_STRLEN_REDIRECT 1
#endif

#ifndef __ANDROID__
#define GLIBC_MANDATORY_STRLEN_REDIRECT 1
#endif


#define HAVE_ASM_UNISTD_H 1



#define HAVE_BUILTIN_ATOMIC 1

#define HAVE_BUILTIN_ATOMIC_CXX 1

#define HAVE_BUILTIN_CLZ 1

#define HAVE_BUILTIN_CTZ 1

#define HAVE_BUILTIN_POPCOUT 1

#define HAVE_CLOCK_GETTIME 1

#define HAVE_CLOCK_MONOTONIC 1

#ifndef __ANDROID__
#define HAVE_DLINFO_RTLD_DI_TLS_MODID 1
#endif

#define HAVE_ENDIAN_H 1

#define HAVE_EPOLL_CREATE 1

#define HAVE_EPOLL_PWAIT 1

#define HAVE_EVENTFD 1

#define HAVE_EVENTFD_READ 1

#define HAVE_GETPAGESIZE 1

#define HAVE_INTTYPES_H 1

#define HAVE_KLOGCTL 1

#define HAVE_LIBPTHREAD 1

#define HAVE_LIBRT 1

#define HAVE_MALLINFO 1

#define HAVE_MEMCHR 1

#define HAVE_MEMORY_H 1

#define HAVE_MEMSET 1

#define HAVE_MKDIR 1

#define HAVE_MMAP 1

#define HAVE_MQUEUE_H 1

#define HAVE_MREMAP 1

#define HAVE_PPOLL 1

#ifndef __ANDROID__
#define HAVE_PROCESS_VM_READV 1
#endif

#ifndef __ANDROID__
#define HAVE_PROCESS_VM_WRITEV 1
#endif

#ifndef __ANDROID__
#define HAVE_PTHREAD_BARRIER_INIT 1
#endif

#define HAVE_PTHREAD_CONDATTR_SETCLOCK 1


#ifndef __ANDROID__
#define HAVE_PTHREAD_MUTEX_ADAPTIVE_NP 1
#endif

#define HAVE_PTHREAD_MUTEX_ERRORCHECK_NP 1

#define HAVE_PTHREAD_MUTEX_RECURSIVE_NP 1

#define HAVE_PTHREAD_MUTEX_TIMEDLOCK 1

#ifndef __ANDROID__
#define HAVE_PTHREAD_MUTEX_T__DATA__KIND 1
#endif


#define HAVE_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP 1

#define HAVE_PTHREAD_RWLOCK_T 1

#define HAVE_PTHREAD_RWLOCK_TIMEDRDLOCK 1

#define HAVE_PTHREAD_RWLOCK_TIMEDWRLOCK 1

#define HAVE_PTHREAD_SETNAME_NP 1

#ifndef __ANDROID__
#define HAVE_PTHREAD_SPIN_LOCK 1
#endif

#define HAVE_PTHREAD_YIELD 1

#define HAVE_PTRACE_GETREGS 1

#define HAVE_READLINKAT 1

#define HAVE_SEMTIMEDOP 1

#ifndef __ANDROID__
#define HAVE_SHARED_POINTER_ANNOTATION 1
#endif

#define HAVE_SIGNALFD 1

#define HAVE_SIGWAITINFO 1

#define HAVE_STDINT_H 1

#define HAVE_STDLIB_H 1

#define HAVE_STRCHR 1

#define HAVE_STRDUP 1

#define HAVE_STRINGS_H 1

#define HAVE_STRING_H 1

#define HAVE_STRPBRK 1

#define HAVE_STRRCHR 1

#define HAVE_STRSTR 1

#define HAVE_SYSCALL 1


#define HAVE_SYS_EPOLL_H 1

#define HAVE_SYS_EVENTFD_H 1

#define HAVE_SYS_KLOG_H 1

#define HAVE_SYS_PARAM_H 1

#define HAVE_SYS_POLL_H 1

#define HAVE_SYS_SIGNALFD_H 1

#define HAVE_SYS_SIGNAL_H 1

#define HAVE_SYS_STAT_H 1

#define HAVE_SYS_SYSCALL_H 1

#define HAVE_SYS_TIME_H 1

#define HAVE_SYS_TYPES_H 1

#define HAVE_SYS_USER_REGS 1

#define HAVE_TLS 1

#define HAVE_UNISTD_H 1

#define HAVE_USABLE_LINUX_FUTEX_H 1

#define HAVE_UTIMENSAT 1


#define KERNEL_2_6 1

#define MIPS_PAGE_SHIFT 12

#define PACKAGE "valgrind"

#define PACKAGE_BUGREPORT "valgrind-users@lists.sourceforge.net"

#define PACKAGE_NAME "Valgrind"

#define PACKAGE_STRING "Valgrind 3.11.0.SVN.aosp"

#define PACKAGE_TARNAME "valgrind"

#define PACKAGE_URL ""

#define PACKAGE_VERSION "3.11.0.SVN.aosp"

#define STDC_HEADERS 1

#define TIME_WITH_SYS_TIME 1

#define VERSION "3.11.0.SVN.aosp"

#ifdef __ANDROID__
#define VG_TMPDIR "/data/local/tmp"
#else
#define VG_TMPDIR "/tmp"
#endif

#define asm __asm__





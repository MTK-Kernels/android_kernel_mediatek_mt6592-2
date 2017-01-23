/* Common definitions for remote server for GDB.
   Copyright (C) 1993, 1995, 1997, 1998, 1999, 2000, 2002, 2003, 2004, 2005,
   2006, 2012
   Free Software Foundation, Inc.

   This file is part of GDB.
   It has been modified to integrate it in valgrind

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#ifndef SERVER_H
#define SERVER_H

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"
#include "pub_core_debuglog.h"
#include "pub_core_errormgr.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcprint.h"
#include "pub_core_mallocfree.h"
#include "pub_core_syscall.h"
#include "pub_core_libcproc.h"
#include "pub_core_tooliface.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcbase.h"
#include "pub_core_options.h"
#include "pub_core_threadstate.h"
#include "pub_core_gdbserver.h"
#include "pub_core_vki.h"
#include "pub_core_clreq.h"


extern void gdbserver_init (void);

extern void server_main (void);

extern void gdbserver_terminate (void);


extern void monitor_output (char *s);

extern int remote_desc_activity(const char *msg);

extern void remote_utils_output_status(void);

extern Bool remote_connected(void);

extern void remote_finish(FinishReason reason);

extern void reset_valgrind_sink(const char* info);

extern void print_to_initial_valgrind_sink (const char *msg);

extern Addr thumb_pc (Addr pc);

extern ThreadId vgdb_interrupted_tid;



#define dlog(level, ...) \
   do { if (UNLIKELY(VG_(debugLog_getLevel)() >= level))  \
         VG_(debugLog) (level, "gdbsrv",__VA_ARGS__); }   \
   while (0)


#ifndef VKI_POLLIN
#define VKI_POLLIN            0x0001
#endif
#define VKI_POLLPRI           0x0002
#define VKI_POLLOUT           0x0004
#define VKI_POLLERR           0x0008
#define VKI_POLLHUP           0x0010
#define VKI_POLLNVAL          0x0020

 
#define strcmp(s1,s2)         VG_(strcmp) ((s1),(s2))
#define strncmp(s1,s2,nmax)   VG_(strncmp) ((s1),(s2),nmax)
#define strcat(s1,s2)         VG_(strcat) ((s1),(s2))
#define strcpy(s1,s2)         VG_(strcpy) ((s1),(s2))
#define strncpy(s1,s2,nmax)   VG_(strncpy) ((s1),(s2),nmax)
#define strlen(s)             VG_(strlen) ((s))
#define strtok(p,s)           VG_(strtok) ((p),(s))
#define strtok_r(p,s,ss)      VG_(strtok_r) ((p),(s),(ss))
#define strchr(s,c)           VG_(strchr) ((s),c)
#define strtol(s,r,b)         ((b) == 16 ? \
                               VG_(strtoll16) ((s),(r)) \
                               : VG_(strtoll10) ((s),(r)))
#define strtoul(s,r,b)        ((b) == 16 ? \
                               VG_(strtoull16) ((s),(r)) \
                               : VG_(strtoull10) ((s),(r)))

#define malloc(sz)            VG_(malloc)  ("gdbsrv", sz)
#define calloc(n,sz)          VG_(calloc)  ("gdbsrv", n, sz)
#define realloc(p,size)       VG_(realloc) ("gdbsrv", p, size)
#define strdup(s)             VG_(strdup)  ("gdbsrv", (s))
#define free(b)               VG_(free)    (b)

#ifndef ATTR_NORETURN
#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7))
#define ATTR_NORETURN __attribute__ ((noreturn))
#else
#define ATTR_NORETURN           
#endif
#endif

#ifndef ATTR_FORMAT
#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 4))
#define ATTR_FORMAT(type, x, y) __attribute__ ((format(type, x, y)))
#else
#define ATTR_FORMAT(type, x, y) 
#endif
#endif

typedef unsigned char gdb_byte;

typedef Addr CORE_ADDR;

struct inferior_list
{
   struct inferior_list_entry *head;
   struct inferior_list_entry *tail;
};
struct inferior_list_entry
{
   unsigned long id;
   struct inferior_list_entry *next;
};

struct thread_info;

#include "regcache.h"
#include "gdb/signals.h"

extern void gdbserver_signal_encountered (const vki_siginfo_t *info);
extern Bool gdbserver_deliver_signal (vki_siginfo_t *info);

extern void gdbserver_pending_signal_to_report (vki_siginfo_t  *info);

extern void gdbserver_process_exit_encountered (unsigned char status, Int code);

extern int pass_signals[]; 


#include "target.h"



extern struct inferior_list all_threads;
void add_inferior_to_list (struct inferior_list *list,
			   struct inferior_list_entry *new_inferior);
void for_each_inferior (struct inferior_list *list,
			void (*action) (struct inferior_list_entry *));
extern struct thread_info *current_inferior;
void remove_inferior (struct inferior_list *list,
		      struct inferior_list_entry *entry);
void remove_thread (struct thread_info *thread);
void add_thread (unsigned long thread_id, void *target_data, unsigned int);
unsigned int thread_id_to_gdb_id (unsigned long);
unsigned int thread_to_gdb_id (struct thread_info *);
unsigned long gdb_id_to_thread_id (unsigned int);
struct thread_info *gdb_id_to_thread (unsigned int);
void clear_inferiors (void);
struct inferior_list_entry *find_inferior (struct inferior_list *,
                                           int (*func) (struct 
                                                        inferior_list_entry *,
                                                        void *),
                                           void *arg);
struct inferior_list_entry *find_inferior_id (struct inferior_list *list,
					      unsigned long id);
void *inferior_target_data (struct thread_info *);
void set_inferior_target_data (struct thread_info *, void *);
void *inferior_regcache_data (struct thread_info *);
void set_inferior_regcache_data (struct thread_info *, void *);
void change_inferior_id (struct inferior_list *list,
			 unsigned long new_id);


extern unsigned long cont_thread;
extern unsigned long general_thread;
extern unsigned long step_thread;
extern unsigned long thread_from_wait;
extern unsigned long old_thread_from_wait;

extern VG_MINIMAL_JMP_BUF(toplevel);


extern Bool noack_mode;
int putpkt (char *buf);
int putpkt_binary (char *buf, int len);
int getpkt (char *buf);
void remote_open (const HChar *name);
void remote_close (void);

void sync_gdb_connection (void);
void write_ok (char *buf);
void write_enn (char *buf);
void convert_ascii_to_int (const char *from, unsigned char *to, int n);
void convert_int_to_ascii (const unsigned char *from, char *to, int n);
void prepare_resume_reply (char *buf, char status, unsigned char sig);

void decode_address (CORE_ADDR *addrp, const char *start, int len);
void decode_m_packet (char *from, CORE_ADDR * mem_addr_ptr,
		      unsigned int *len_ptr);
void decode_M_packet (char *from, CORE_ADDR * mem_addr_ptr,
		      unsigned int *len_ptr, unsigned char *to);
int decode_X_packet (char *from, int packet_len, CORE_ADDR * mem_addr_ptr,
		     unsigned int *len_ptr, unsigned char *to);

int unhexify (char *bin, const char *hex, int count);
int hexify (char *hex, const char *bin, int count);
char* heximage (char *buf, char *bin, int count);

void* C2v(CORE_ADDR addr);


int remote_escape_output (const gdb_byte *buffer, int len,
			  gdb_byte *out_buf, int *out_len,
			  int out_maxlen);

enum target_signal target_signal_from_host (int hostsig);
int target_signal_to_host_p (enum target_signal oursig);
int target_signal_to_host (enum target_signal oursig);
const char *target_signal_to_name (enum target_signal);


void error (const char *string,...) ATTR_NORETURN ATTR_FORMAT (printf, 1, 2);
void sr_perror (SysRes sr,const char *string,...) ATTR_FORMAT (printf, 2, 3);
void fatal (const char *string,...) ATTR_NORETURN ATTR_FORMAT (printf, 1, 2);
void warning (const char *string,...) ATTR_FORMAT (printf, 1, 2);


void init_registers (void);

#define MAXBUFBYTES(N) (((N)-32)/2)


#define PBUFSIZ 16384
#define POVERHSIZ 5

#define DATASIZ ((PBUFSIZ-POVERHSIZ)/2)

extern const char version[];

#endif 

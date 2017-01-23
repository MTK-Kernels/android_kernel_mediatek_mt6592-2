

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

#if defined(VGO_linux) || defined(VGO_darwin)

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_vkiscnums.h"
#include "pub_core_threadstate.h"
#include "pub_core_debuginfo.h"     
#include "pub_core_aspacemgr.h"
#include "pub_core_transtab.h"      
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"   
#include "pub_core_debuglog.h"
#include "pub_core_errormgr.h"
#include "pub_core_gdbserver.h"     
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcsignal.h"
#include "pub_core_machine.h"       
#include "pub_core_mallocfree.h"
#include "pub_core_options.h"
#include "pub_core_scheduler.h"
#include "pub_core_signals.h"
#include "pub_core_stacktrace.h"    
#include "pub_core_syscall.h"
#include "pub_core_syswrap.h"
#include "pub_core_tooliface.h"
#include "pub_core_ume.h"
#include "pub_core_stacks.h"

#include "priv_types_n_macros.h"
#include "priv_syswrap-generic.h"

#include "config.h"


void ML_(guess_and_register_stack) (Addr sp, ThreadState* tst)
{
   Bool debug = False;
   NSegment const* seg;

   seg = VG_(am_find_nsegment)(sp);
   if (seg &&
       VG_(am_is_valid_for_client)(sp, 1, VKI_PROT_READ | VKI_PROT_WRITE)) {
      tst->client_stack_highest_byte = (Addr)VG_PGROUNDUP(sp)-1;
      tst->client_stack_szB = tst->client_stack_highest_byte - seg->start + 1;

      VG_(register_stack)(seg->start, tst->client_stack_highest_byte);

      if (debug)
	 VG_(printf)("tid %d: guessed client stack range [%#lx-%#lx]\n",
		     tst->tid, seg->start, tst->client_stack_highest_byte);
   } else {
      VG_(message)(Vg_UserMsg,
                   "!? New thread %d starts with SP(%#lx) unmapped\n",
		   tst->tid, sp);
      tst->client_stack_highest_byte = 0;
      tst->client_stack_szB  = 0;
   }
}


Bool ML_(valid_client_addr)(Addr start, SizeT size, ThreadId tid,
                                   const HChar *syscallname)
{
   Bool ret;

   if (size == 0)
      return True;

   ret = VG_(am_is_valid_for_client_or_free_or_resvn)
            (start,size,VKI_PROT_NONE);

   if (0)
      VG_(printf)("%s: test=%#lx-%#lx ret=%d\n",
		  syscallname, start, start+size-1, (Int)ret);

   if (!ret && syscallname != NULL) {
      VG_(message)(Vg_UserMsg, "Warning: client syscall %s tried "
                               "to modify addresses %#lx-%#lx\n",
                               syscallname, start, start+size-1);
      if (VG_(clo_verbosity) > 1) {
         VG_(get_and_pp_StackTrace)(tid, VG_(clo_backtrace_size));
      }
   }

   return ret;
}


Bool ML_(client_signal_OK)(Int sigNo)
{
   
   Bool ret = sigNo >= 0 && sigNo <= VG_SIGVGRTUSERMAX;

   

   return ret;
}



Bool ML_(safe_to_deref) ( void* start, SizeT size )
{
   return VG_(am_is_valid_for_client)( (Addr)start, size, VKI_PROT_READ );
}




static 
void page_align_addr_and_len( Addr* a, SizeT* len)
{
   Addr ra;
   
   ra = VG_PGROUNDDN(*a);
   *len = VG_PGROUNDUP(*a + *len) - ra;
   *a = ra;
}

static void notify_core_of_mmap(Addr a, SizeT len, UInt prot,
                                UInt flags, Int fd, Off64T offset)
{
   Bool d;

   
   vg_assert(VG_IS_PAGE_ALIGNED(a));
   
   len = VG_PGROUNDUP(len);

   d = VG_(am_notify_client_mmap)( a, len, prot, flags, fd, offset );

   if (d)
      VG_(discard_translations)( a, (ULong)len,
                                 "notify_core_of_mmap" );
}

static void notify_tool_of_mmap(Addr a, SizeT len, UInt prot, ULong di_handle)
{
   Bool rr, ww, xx;

   
   vg_assert(VG_IS_PAGE_ALIGNED(a));
   
   len = VG_PGROUNDUP(len);

   rr = toBool(prot & VKI_PROT_READ);
   ww = toBool(prot & VKI_PROT_WRITE);
   xx = toBool(prot & VKI_PROT_EXEC);

   VG_TRACK( new_mem_mmap, a, len, rr, ww, xx, di_handle );
}


void 
ML_(notify_core_and_tool_of_mmap) ( Addr a, SizeT len, UInt prot, 
                                    UInt flags, Int fd, Off64T offset )
{
   
   
   
   notify_core_of_mmap(a, len, prot, flags, fd, offset);
   notify_tool_of_mmap(a, len, prot, 0);
}

void 
ML_(notify_core_and_tool_of_munmap) ( Addr a, SizeT len )
{
   Bool d;

   page_align_addr_and_len(&a, &len);
   d = VG_(am_notify_munmap)(a, len);
   VG_TRACK( die_mem_munmap, a, len );
   VG_(di_notify_munmap)( a, len );
   if (d)
      VG_(discard_translations)( a, (ULong)len, 
                                 "ML_(notify_core_and_tool_of_munmap)" );
}

void 
ML_(notify_core_and_tool_of_mprotect) ( Addr a, SizeT len, Int prot )
{
   Bool rr = toBool(prot & VKI_PROT_READ);
   Bool ww = toBool(prot & VKI_PROT_WRITE);
   Bool xx = toBool(prot & VKI_PROT_EXEC);
   Bool d;

   page_align_addr_and_len(&a, &len);
   d = VG_(am_notify_mprotect)(a, len, prot);
   VG_TRACK( change_mem_mprotect, a, len, rr, ww, xx );
   VG_(di_notify_mprotect)( a, len, prot );
   if (d)
      VG_(discard_translations)( a, (ULong)len, 
                                 "ML_(notify_core_and_tool_of_mprotect)" );
}



#if HAVE_MREMAP
static
SysRes do_mremap( Addr old_addr, SizeT old_len, 
                  Addr new_addr, SizeT new_len,
                  UWord flags, ThreadId tid )
{
#  define MIN_SIZET(_aa,_bb) (_aa) < (_bb) ? (_aa) : (_bb)

   Bool      ok, d;
   NSegment const* old_seg;
   Addr      advised;
   Bool      f_fixed   = toBool(flags & VKI_MREMAP_FIXED);
   Bool      f_maymove = toBool(flags & VKI_MREMAP_MAYMOVE);

   if (0)
      VG_(printf)("do_remap (old %#lx %ld) (new %#lx %ld) %s %s\n",
                  old_addr,old_len,new_addr,new_len, 
                  flags & VKI_MREMAP_MAYMOVE ? "MAYMOVE" : "",
                  flags & VKI_MREMAP_FIXED ? "FIXED" : "");
   if (0)
      VG_(am_show_nsegments)(0, "do_remap: before");

   if (flags & ~(VKI_MREMAP_FIXED | VKI_MREMAP_MAYMOVE))
      goto eINVAL;

   if (!VG_IS_PAGE_ALIGNED(old_addr))
      goto eINVAL;

   old_len = VG_PGROUNDUP(old_len);
   new_len = VG_PGROUNDUP(new_len);

   if (new_len == 0)
      goto eINVAL;

   
   if (old_len == 0)
      goto eINVAL;

   
   if (old_addr + old_len < old_addr)
      goto eINVAL;
   if (f_fixed == True && new_addr + new_len < new_len)
      goto eINVAL;

   if (f_fixed == True && f_maymove == False)
      goto eINVAL;

   
   if (!ML_(valid_client_addr)(old_addr, old_len, tid, "mremap(old_addr)"))
      goto eINVAL;

   old_seg = VG_(am_find_nsegment)( old_addr );
   if (old_addr < old_seg->start || old_addr+old_len-1 > old_seg->end)
      goto eINVAL;
   if (old_seg->kind != SkAnonC && old_seg->kind != SkFileC &&
       old_seg->kind != SkShmC)
      goto eINVAL;

   vg_assert(old_len > 0);
   vg_assert(new_len > 0);
   vg_assert(VG_IS_PAGE_ALIGNED(old_len));
   vg_assert(VG_IS_PAGE_ALIGNED(new_len));
   vg_assert(VG_IS_PAGE_ALIGNED(old_addr));


   if (f_maymove == False) {
      
      if (new_len < old_len)
         goto shrink_in_place;
      if (new_len > old_len)
         goto grow_in_place_or_fail;
      goto same_in_place;
   }

   if (f_maymove == True && f_fixed == False) {
      
      if (new_len < old_len)
         goto shrink_in_place;
      if (new_len > old_len)
         goto grow_in_place_or_move_anywhere_or_fail;
      goto same_in_place;
   }

   if (f_maymove == True && f_fixed == True) {
      
      if (!VG_IS_PAGE_ALIGNED(new_addr)) 
         goto eINVAL;
      if (new_addr+new_len-1 < old_addr || new_addr > old_addr+old_len-1) {
         
      } else {
         goto eINVAL;
      }
      if (new_addr == 0) 
         goto eINVAL; 
      advised = VG_(am_get_advisory_client_simple)(new_addr, new_len, &ok);
      if (!ok || advised != new_addr)
         goto eNOMEM;
      ok = VG_(am_relocate_nooverlap_client)
              ( &d, old_addr, old_len, new_addr, new_len );
      if (ok) {
         VG_TRACK( copy_mem_remap, old_addr, new_addr, 
                                   MIN_SIZET(old_len,new_len) );
         if (new_len > old_len)
            VG_TRACK( new_mem_mmap, new_addr+old_len, new_len-old_len,
                      old_seg->hasR, old_seg->hasW, old_seg->hasX,
                      0 );
         VG_TRACK(die_mem_munmap, old_addr, old_len);
         if (d) {
            VG_(discard_translations)( old_addr, old_len, "do_remap(1)" );
            VG_(discard_translations)( new_addr, new_len, "do_remap(2)" );
         }
         return VG_(mk_SysRes_Success)( new_addr );
      }
      goto eNOMEM;
   }

   
    vg_assert(0);

  grow_in_place_or_move_anywhere_or_fail: 
   { 
   
   Addr   needA = old_addr + old_len;
   SSizeT needL = new_len - old_len;

   vg_assert(needL > 0);
   vg_assert(needA > 0);

   advised = VG_(am_get_advisory_client_simple)( needA, needL, &ok );
   if (ok) {
      
      ok = VG_(am_covered_by_single_free_segment) ( needA, needL );
   }
   if (ok && advised == needA) {
      const NSegment *new_seg = VG_(am_extend_map_client)( old_addr, needL );
      if (new_seg) {
         VG_TRACK( new_mem_mmap, needA, needL, 
                                 new_seg->hasR, 
                                 new_seg->hasW, new_seg->hasX,
                                 0 );
         return VG_(mk_SysRes_Success)( old_addr );
      }
   }

   
   advised = VG_(am_get_advisory_client_simple)( 0, new_len, &ok );
   if (ok) {
      Bool oldR = old_seg->hasR;
      Bool oldW = old_seg->hasW;
      Bool oldX = old_seg->hasX;
      
      vg_assert(advised+new_len-1 < old_addr 
                || advised > old_addr+old_len-1);
      ok = VG_(am_relocate_nooverlap_client)
              ( &d, old_addr, old_len, advised, new_len );
      if (ok) {
         VG_TRACK( copy_mem_remap, old_addr, advised, 
                                   MIN_SIZET(old_len,new_len) );
         if (new_len > old_len)
            VG_TRACK( new_mem_mmap, advised+old_len, new_len-old_len,
                      oldR, oldW, oldX, 0 );
         VG_TRACK(die_mem_munmap, old_addr, old_len);
         if (d) {
            VG_(discard_translations)( old_addr, old_len, "do_remap(4)" );
            VG_(discard_translations)( advised, new_len, "do_remap(5)" );
         }
         return VG_(mk_SysRes_Success)( advised );
      }
   }
   goto eNOMEM;
   }
    vg_assert(0);

  grow_in_place_or_fail:
   {
   Addr  needA = old_addr + old_len;
   SizeT needL = new_len - old_len;

   vg_assert(needA > 0);

   advised = VG_(am_get_advisory_client_simple)( needA, needL, &ok );
   if (ok) {
      
      ok = VG_(am_covered_by_single_free_segment) ( needA, needL );
   }
   if (!ok || advised != needA)
      goto eNOMEM;
   const NSegment *new_seg = VG_(am_extend_map_client)( old_addr, needL );
   if (!new_seg)
      goto eNOMEM;
   VG_TRACK( new_mem_mmap, needA, needL, 
                           new_seg->hasR, new_seg->hasW, new_seg->hasX,
                           0 );

   return VG_(mk_SysRes_Success)( old_addr );
   }
    vg_assert(0);

  shrink_in_place:
   {
   SysRes sres = VG_(am_munmap_client)( &d, old_addr+new_len, old_len-new_len );
   if (sr_isError(sres))
      return sres;
   VG_TRACK( die_mem_munmap, old_addr+new_len, old_len-new_len );
   if (d)
      VG_(discard_translations)( old_addr+new_len, old_len-new_len, 
                                 "do_remap(7)" );
   return VG_(mk_SysRes_Success)( old_addr );
   }
    vg_assert(0);

  same_in_place:
   return VG_(mk_SysRes_Success)( old_addr );
    vg_assert(0);

  eINVAL:
   return VG_(mk_SysRes_Error)( VKI_EINVAL );
  eNOMEM:
   return VG_(mk_SysRes_Error)( VKI_ENOMEM );

#  undef MIN_SIZET
}
#endif 



typedef struct OpenFd
{
   Int fd;                        
   HChar *pathname;               
   ExeContext *where;             
   struct OpenFd *next, *prev;
} OpenFd;

static OpenFd *allocated_fds = NULL;

static Int fd_count = 0;


static
void record_fd_close(Int fd)
{
   OpenFd *i = allocated_fds;

   if (fd >= VG_(fd_hard_limit))
      return;			

   while(i) {
      if(i->fd == fd) {
         if(i->prev)
            i->prev->next = i->next;
         else
            allocated_fds = i->next;
         if(i->next)
            i->next->prev = i->prev;
         if(i->pathname) 
            VG_(free) (i->pathname);
         VG_(free) (i);
         fd_count--;
         break;
      }
      i = i->next;
   }
}

void ML_(record_fd_open_with_given_name)(ThreadId tid, Int fd,
                                         const HChar *pathname)
{
   OpenFd *i;

   if (fd >= VG_(fd_hard_limit))
      return;			

   
   i = allocated_fds;
   while (i) {
      if (i->fd == fd) {
         if (i->pathname) VG_(free)(i->pathname);
         break;
      }
      i = i->next;
   }

   
   if (i == NULL) {
      i = VG_(malloc)("syswrap.rfdowgn.1", sizeof(OpenFd));

      i->prev = NULL;
      i->next = allocated_fds;
      if(allocated_fds) allocated_fds->prev = i;
      allocated_fds = i;
      fd_count++;
   }

   i->fd = fd;
   i->pathname = VG_(strdup)("syswrap.rfdowgn.2", pathname);
   i->where = (tid == -1) ? NULL : VG_(record_ExeContext)(tid, 0);
}

void ML_(record_fd_open_named)(ThreadId tid, Int fd)
{
   const HChar* buf;
   const HChar* name;
   if (VG_(resolve_filename)(fd, &buf))
      name = buf;
   else
      name = NULL;
   
   ML_(record_fd_open_with_given_name)(tid, fd, name);
}

void ML_(record_fd_open_nameless)(ThreadId tid, Int fd)
{
   ML_(record_fd_open_with_given_name)(tid, fd, NULL);
}

static
HChar *unix_to_name(struct vki_sockaddr_un *sa, UInt len, HChar *name)
{
   if (sa == NULL || len == 0 || sa->sun_path[0] == '\0') {
      VG_(sprintf)(name, "<unknown>");
   } else {
      VG_(sprintf)(name, "%s", sa->sun_path);
   }

   return name;
}

static
HChar *inet_to_name(struct vki_sockaddr_in *sa, UInt len, HChar *name)
{
   if (sa == NULL || len == 0) {
      VG_(sprintf)(name, "<unknown>");
   } else if (sa->sin_port == 0) {
      VG_(sprintf)(name, "<unbound>");
   } else {
      UInt addr = VG_(ntohl)(sa->sin_addr.s_addr);
      VG_(sprintf)(name, "%u.%u.%u.%u:%u",
                   (addr>>24) & 0xFF, (addr>>16) & 0xFF,
                   (addr>>8) & 0xFF, addr & 0xFF,
                   VG_(ntohs)(sa->sin_port));
   }

   return name;
}

static
void inet6_format(HChar *s, const UChar ip[16])
{
   static const unsigned char V4mappedprefix[12] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};

   if (!VG_(memcmp)(ip, V4mappedprefix, 12)) {
      const struct vki_in_addr *sin_addr =
          (const struct vki_in_addr *)(ip + 12);
      UInt addr = VG_(ntohl)(sin_addr->s_addr);

      VG_(sprintf)(s, "::ffff:%u.%u.%u.%u",
                   (addr>>24) & 0xFF, (addr>>16) & 0xFF,
                   (addr>>8) & 0xFF, addr & 0xFF);
   } else {
      Bool compressing = False;
      Bool compressed = False;
      Int len = 0;
      Int i;

      for (i = 0; i < 16; i += 2) {
         UInt word = ((UInt)ip[i] << 8) | (UInt)ip[i+1];
         if (word == 0 && !compressed) {
            compressing = True;
         } else {
            if (compressing) {
               compressing = False;
               compressed = True;
               s[len++] = ':';
            }
            if (i > 0) {
               s[len++] = ':';
            }
            len += VG_(sprintf)(s + len, "%x", word);
         }
      }

      if (compressing) {
         s[len++] = ':';
         s[len++] = ':';
      }

      s[len++] = 0;
   }

   return;
}

static
HChar *inet6_to_name(struct vki_sockaddr_in6 *sa, UInt len, HChar *name)
{
   if (sa == NULL || len == 0) {
      VG_(sprintf)(name, "<unknown>");
   } else if (sa->sin6_port == 0) {
      VG_(sprintf)(name, "<unbound>");
   } else {
      HChar addr[100];    
      inet6_format(addr, (void *)&(sa->sin6_addr));
      VG_(sprintf)(name, "[%s]:%u", addr, VG_(ntohs)(sa->sin6_port));
   }

   return name;
}

static void
getsockdetails(Int fd)
{
   union u {
      struct vki_sockaddr a;
      struct vki_sockaddr_in in;
      struct vki_sockaddr_in6 in6;
      struct vki_sockaddr_un un;
   } laddr;
   Int llen;

   llen = sizeof(laddr);
   VG_(memset)(&laddr, 0, llen);

   if(VG_(getsockname)(fd, (struct vki_sockaddr *)&(laddr.a), &llen) != -1) {
      switch(laddr.a.sa_family) {
      case VKI_AF_INET: {
         HChar lname[32];   
         HChar pname[32];   
         struct vki_sockaddr_in paddr;
         Int plen = sizeof(struct vki_sockaddr_in);

         if (VG_(getpeername)(fd, (struct vki_sockaddr *)&paddr, &plen) != -1) {
            VG_(message)(Vg_UserMsg, "Open AF_INET socket %d: %s <-> %s\n", fd,
                         inet_to_name(&(laddr.in), llen, lname),
                         inet_to_name(&paddr, plen, pname));
         } else {
            VG_(message)(Vg_UserMsg, "Open AF_INET socket %d: %s <-> unbound\n",
                         fd, inet_to_name(&(laddr.in), llen, lname));
         }
         return;
         }
      case VKI_AF_INET6: {
         HChar lname[128];  
         HChar pname[128];  
         struct vki_sockaddr_in6 paddr;
         Int plen = sizeof(struct vki_sockaddr_in6);

         if (VG_(getpeername)(fd, (struct vki_sockaddr *)&paddr, &plen) != -1) {
            VG_(message)(Vg_UserMsg, "Open AF_INET6 socket %d: %s <-> %s\n", fd,
                         inet6_to_name(&(laddr.in6), llen, lname),
                         inet6_to_name(&paddr, plen, pname));
         } else {
            VG_(message)(Vg_UserMsg, "Open AF_INET6 socket %d: %s <-> unbound\n",
                         fd, inet6_to_name(&(laddr.in6), llen, lname));
         }
         return;
         }
      case VKI_AF_UNIX: {
         static char lname[256];
         VG_(message)(Vg_UserMsg, "Open AF_UNIX socket %d: %s\n", fd,
                      unix_to_name(&(laddr.un), llen, lname));
         return;
         }
      default:
         VG_(message)(Vg_UserMsg, "Open pf-%d socket %d:\n",
                      laddr.a.sa_family, fd);
         return;
      }
   }

   VG_(message)(Vg_UserMsg, "Open socket %d:\n", fd);
}


void VG_(show_open_fds) (const HChar* when)
{
   OpenFd *i = allocated_fds;

   VG_(message)(Vg_UserMsg, "FILE DESCRIPTORS: %d open %s.\n", fd_count, when);

   while (i) {
      if (i->pathname) {
         VG_(message)(Vg_UserMsg, "Open file descriptor %d: %s\n", i->fd,
                      i->pathname);
      } else {
         Int val;
         Int len = sizeof(val);

         if (VG_(getsockopt)(i->fd, VKI_SOL_SOCKET, VKI_SO_TYPE, &val, &len)
             == -1) {
            VG_(message)(Vg_UserMsg, "Open file descriptor %d:\n", i->fd);
         } else {
            getsockdetails(i->fd);
         }
      }

      if(i->where) {
         VG_(pp_ExeContext)(i->where);
         VG_(message)(Vg_UserMsg, "\n");
      } else {
         VG_(message)(Vg_UserMsg, "   <inherited from parent>\n");
         VG_(message)(Vg_UserMsg, "\n");
      }

      i = i->next;
   }

   VG_(message)(Vg_UserMsg, "\n");
}

static
void init_preopened_fds_without_proc_self_fd(void)
{
   struct vki_rlimit lim;
   UInt count;
   Int i;

   if (VG_(getrlimit) (VKI_RLIMIT_NOFILE, &lim) == -1) {
      count = 1024;
   } else {
      count = lim.rlim_cur;
   }

   for (i = 0; i < count; i++)
      if (VG_(fcntl)(i, VKI_F_GETFL, 0) != -1)
         ML_(record_fd_open_named)(-1, i);
}


void VG_(init_preopened_fds)(void)
{
#if defined(VGO_linux)
   Int ret;
   struct vki_dirent64 d;
   SysRes f;

   f = VG_(open)("/proc/self/fd", VKI_O_RDONLY, 0);
   if (sr_isError(f)) {
      init_preopened_fds_without_proc_self_fd();
      return;
   }

   while ((ret = VG_(getdents64)(sr_Res(f), &d, sizeof(d))) != 0) {
      if (ret == -1)
         goto out;

      if (VG_(strcmp)(d.d_name, ".") && VG_(strcmp)(d.d_name, "..")) {
         HChar* s;
         Int fno = VG_(strtoll10)(d.d_name, &s);
         if (*s == '\0') {
            if (fno != sr_Res(f))
               if (VG_(clo_track_fds))
                  ML_(record_fd_open_named)(-1, fno);
         } else {
            VG_(message)(Vg_DebugMsg, 
               "Warning: invalid file name in /proc/self/fd: %s\n",
               d.d_name);
         }
      }

      VG_(lseek)(sr_Res(f), d.d_off, VKI_SEEK_SET);
   }

  out:
   VG_(close)(sr_Res(f));

#elif defined(VGO_darwin)
   init_preopened_fds_without_proc_self_fd();

#else
#  error Unknown OS
#endif
}

static
HChar *strdupcat ( const HChar* cc, const HChar *s1, const HChar *s2,
                   ArenaId aid )
{
   UInt len = VG_(strlen) ( s1 ) + VG_(strlen) ( s2 ) + 1;
   HChar *result = VG_(arena_malloc) ( aid, cc, len );
   VG_(strcpy) ( result, s1 );
   VG_(strcat) ( result, s2 );
   return result;
}

static 
void pre_mem_read_sendmsg ( ThreadId tid, Bool read,
                            const HChar *msg, Addr base, SizeT size )
{
   HChar *outmsg = strdupcat ( "di.syswrap.pmrs.1",
                               "sendmsg", msg, VG_AR_CORE );
   PRE_MEM_READ( outmsg, base, size );
   VG_(free) ( outmsg );
}

static 
void pre_mem_write_recvmsg ( ThreadId tid, Bool read,
                             const HChar *msg, Addr base, SizeT size )
{
   HChar *outmsg = strdupcat ( "di.syswrap.pmwr.1",
                               "recvmsg", msg, VG_AR_CORE );
   if ( read )
      PRE_MEM_READ( outmsg, base, size );
   else
      PRE_MEM_WRITE( outmsg, base, size );
   VG_(free) ( outmsg );
}

static
void post_mem_write_recvmsg ( ThreadId tid, Bool read,
                              const HChar *fieldName, Addr base, SizeT size )
{
   if ( !read )
      POST_MEM_WRITE( base, size );
}
 
static
void msghdr_foreachfield ( 
        ThreadId tid,
        const HChar *name,
        struct vki_msghdr *msg,
        UInt length,
        void (*foreach_func)( ThreadId, Bool, const HChar *, Addr, SizeT ),
        Bool rekv 
     )
{
   HChar *fieldName;

   if ( !msg )
      return;

   fieldName = VG_(malloc) ( "di.syswrap.mfef", VG_(strlen)(name) + 32 );

   VG_(sprintf) ( fieldName, "(%s)", name );

   foreach_func ( tid, True, fieldName, (Addr)&msg->msg_name, sizeof( msg->msg_name ) );
   foreach_func ( tid, True, fieldName, (Addr)&msg->msg_namelen, sizeof( msg->msg_namelen ) );
   foreach_func ( tid, True, fieldName, (Addr)&msg->msg_iov, sizeof( msg->msg_iov ) );
   foreach_func ( tid, True, fieldName, (Addr)&msg->msg_iovlen, sizeof( msg->msg_iovlen ) );
   foreach_func ( tid, True, fieldName, (Addr)&msg->msg_control, sizeof( msg->msg_control ) );
   foreach_func ( tid, True, fieldName, (Addr)&msg->msg_controllen, sizeof( msg->msg_controllen ) );

   if ( rekv )
      foreach_func ( tid, False, fieldName, (Addr)&msg->msg_flags, sizeof( msg->msg_flags ) );

   if ( ML_(safe_to_deref)(&msg->msg_name, sizeof (void *))
        && msg->msg_name ) {
      VG_(sprintf) ( fieldName, "(%s.msg_name)", name );
      foreach_func ( tid, False, fieldName, 
                     (Addr)msg->msg_name, msg->msg_namelen );
   }

   if ( ML_(safe_to_deref)(&msg->msg_iov, sizeof (void *))
        && msg->msg_iov ) {
      struct vki_iovec *iov = msg->msg_iov;
      UInt i;

      VG_(sprintf) ( fieldName, "(%s.msg_iov)", name );

      foreach_func ( tid, True, fieldName, 
                     (Addr)iov, msg->msg_iovlen * sizeof( struct vki_iovec ) );

      for ( i = 0; i < msg->msg_iovlen; ++i, ++iov ) {
         UInt iov_len = iov->iov_len <= length ? iov->iov_len : length;
         VG_(sprintf) ( fieldName, "(%s.msg_iov[%u])", name, i );
         foreach_func ( tid, False, fieldName, 
                        (Addr)iov->iov_base, iov_len );
         length = length - iov_len;
      }
   }

   if ( ML_(safe_to_deref) (&msg->msg_control, sizeof (void *))
        && msg->msg_control )
   {
      VG_(sprintf) ( fieldName, "(%s.msg_control)", name );
      foreach_func ( tid, False, fieldName, 
                     (Addr)msg->msg_control, msg->msg_controllen );
   }

   VG_(free) ( fieldName );
}

static void check_cmsg_for_fds(ThreadId tid, struct vki_msghdr *msg)
{
   struct vki_cmsghdr *cm = VKI_CMSG_FIRSTHDR(msg);

   while (cm) {
      if (cm->cmsg_level == VKI_SOL_SOCKET &&
          cm->cmsg_type == VKI_SCM_RIGHTS ) {
         Int *fds = (Int *) VKI_CMSG_DATA(cm);
         Int fdc = (cm->cmsg_len - VKI_CMSG_ALIGN(sizeof(struct vki_cmsghdr)))
                         / sizeof(int);
         Int i;

         for (i = 0; i < fdc; i++)
            if(VG_(clo_track_fds))
               
               
               ML_(record_fd_open_named)(tid, fds[i]);
      }

      cm = VKI_CMSG_NXTHDR(msg, cm);
   }
}

static
void pre_mem_read_sockaddr ( ThreadId tid,
                             const HChar *description,
                             struct vki_sockaddr *sa, UInt salen )
{
   HChar *outmsg;
   struct vki_sockaddr_un*  sun  = (struct vki_sockaddr_un *)sa;
   struct vki_sockaddr_in*  sin  = (struct vki_sockaddr_in *)sa;
   struct vki_sockaddr_in6* sin6 = (struct vki_sockaddr_in6 *)sa;
#  ifdef VKI_AF_BLUETOOTH
   struct vki_sockaddr_rc*  rc   = (struct vki_sockaddr_rc *)sa;
#  endif
#  ifdef VKI_AF_NETLINK
   struct vki_sockaddr_nl*  nl   = (struct vki_sockaddr_nl *)sa;
#  endif

   
   if ( sa == NULL || salen == 0 ) return;

   outmsg = VG_(malloc) ( "di.syswrap.pmr_sockaddr.1",
                          VG_(strlen)( description ) + 30 );

   VG_(sprintf) ( outmsg, description, "sa_family" );
   PRE_MEM_READ( outmsg, (Addr) &sa->sa_family, sizeof(vki_sa_family_t));

   switch (sa->sa_family) {
                  
      case VKI_AF_UNIX:
         VG_(sprintf) ( outmsg, description, "sun_path" );
         PRE_MEM_RASCIIZ( outmsg, (Addr) sun->sun_path );
         
         break;
                     
      case VKI_AF_INET:
         VG_(sprintf) ( outmsg, description, "sin_port" );
         PRE_MEM_READ( outmsg, (Addr) &sin->sin_port, sizeof (sin->sin_port) );
         VG_(sprintf) ( outmsg, description, "sin_addr" );
         PRE_MEM_READ( outmsg, (Addr) &sin->sin_addr, sizeof (sin->sin_addr) );
         break;
                           
      case VKI_AF_INET6:
         VG_(sprintf) ( outmsg, description, "sin6_port" );
         PRE_MEM_READ( outmsg,
            (Addr) &sin6->sin6_port, sizeof (sin6->sin6_port) );
         VG_(sprintf) ( outmsg, description, "sin6_flowinfo" );
         PRE_MEM_READ( outmsg,
            (Addr) &sin6->sin6_flowinfo, sizeof (sin6->sin6_flowinfo) );
         VG_(sprintf) ( outmsg, description, "sin6_addr" );
         PRE_MEM_READ( outmsg,
            (Addr) &sin6->sin6_addr, sizeof (sin6->sin6_addr) );
         VG_(sprintf) ( outmsg, description, "sin6_scope_id" );
         PRE_MEM_READ( outmsg,
            (Addr) &sin6->sin6_scope_id, sizeof (sin6->sin6_scope_id) );
         break;

#     ifdef VKI_AF_BLUETOOTH
      case VKI_AF_BLUETOOTH:
         VG_(sprintf) ( outmsg, description, "rc_bdaddr" );
         PRE_MEM_READ( outmsg, (Addr) &rc->rc_bdaddr, sizeof (rc->rc_bdaddr) );
         VG_(sprintf) ( outmsg, description, "rc_channel" );
         PRE_MEM_READ( outmsg, (Addr) &rc->rc_channel, sizeof (rc->rc_channel) );
         break;
#     endif

#     ifdef VKI_AF_NETLINK
      case VKI_AF_NETLINK:
         VG_(sprintf)(outmsg, description, "nl_pid");
         PRE_MEM_READ(outmsg, (Addr)&nl->nl_pid, sizeof(nl->nl_pid));
         VG_(sprintf)(outmsg, description, "nl_groups");
         PRE_MEM_READ(outmsg, (Addr)&nl->nl_groups, sizeof(nl->nl_groups));
         break;
#     endif

#     ifdef VKI_AF_UNSPEC
      case VKI_AF_UNSPEC:
         break;
#     endif

      default:
         VG_(sprintf) ( outmsg, description, "sa_data" );
         PRE_MEM_READ( outmsg, (Addr)&sa->sa_family + sizeof(sa->sa_family),
                       salen -  sizeof(sa->sa_family));
         break;
   }
   
   VG_(free) ( outmsg );
}

static UInt deref_UInt ( ThreadId tid, Addr a, const HChar* s )
{
   UInt* a_p = (UInt*)a;
   PRE_MEM_READ( s, (Addr)a_p, sizeof(UInt) );
   if (a_p == NULL)
      return 0;
   else
      return *a_p;
}

void ML_(buf_and_len_pre_check) ( ThreadId tid, Addr buf_p, Addr buflen_p,
                                  const HChar* buf_s, const HChar* buflen_s )
{
   if (VG_(tdict).track_pre_mem_write) {
      UInt buflen_in = deref_UInt( tid, buflen_p, buflen_s);
      if (buflen_in > 0) {
         VG_(tdict).track_pre_mem_write(
            Vg_CoreSysCall, tid, buf_s, buf_p, buflen_in );
      }
   }
}

void ML_(buf_and_len_post_check) ( ThreadId tid, SysRes res,
                                   Addr buf_p, Addr buflen_p, const HChar* s )
{
   if (!sr_isError(res) && VG_(tdict).track_post_mem_write) {
      UInt buflen_out = deref_UInt( tid, buflen_p, s);
      if (buflen_out > 0 && buf_p != (Addr)NULL) {
         VG_(tdict).track_post_mem_write( Vg_CoreSysCall, tid, buf_p, buflen_out );
      }
   }
}




static Addr do_brk ( Addr newbrk, ThreadId tid )
{
   NSegment const* aseg;
   Addr newbrkP;
   SizeT delta;
   Bool debug = False;

   if (debug)
      VG_(printf)("\ndo_brk: brk_base=%#lx brk_limit=%#lx newbrk=%#lx\n",
		  VG_(brk_base), VG_(brk_limit), newbrk);

   if (0) VG_(am_show_nsegments)(0, "in_brk");

   if (newbrk < VG_(brk_base))
      
      goto bad;

   if (newbrk < VG_(brk_limit)) {
      NSegment const * seg = VG_(am_find_nsegment)(newbrk);
      vg_assert(seg);

      if (seg->hasT)
         VG_(discard_translations)( newbrk, VG_(brk_limit) - newbrk, 
                                    "do_brk(shrink)" );
      NSegment const * seg2;

      seg2 = VG_(am_find_nsegment)( VG_(brk_limit) - 1 );
      vg_assert(seg2);

      if (seg == seg2 && seg->hasW)
         VG_(memset)( (void*)newbrk, 0, VG_(brk_limit) - newbrk );

      VG_(brk_limit) = newbrk;
      return newbrk;
   }

   
   if (VG_(brk_limit) > VG_(brk_base))
      aseg = VG_(am_find_nsegment)( VG_(brk_limit)-1 );
   else
      aseg = VG_(am_find_nsegment)( VG_(brk_limit) );

   
   vg_assert(aseg);
   vg_assert(aseg->kind == SkAnonC);

   if (newbrk <= aseg->end + 1) {
      
      VG_(brk_limit) = newbrk;
      return newbrk;
   }

   newbrkP = VG_PGROUNDUP(newbrk);
   delta = newbrkP - (aseg->end + 1);
   vg_assert(delta > 0);
   vg_assert(VG_IS_PAGE_ALIGNED(delta));
   
   Bool overflow;
   if (! VG_(am_extend_into_adjacent_reservation_client)( aseg->start, delta,
                                                          &overflow)) {
      if (overflow)
         VG_(umsg)("brk segment overflow in thread #%d: can't grow to %#lx\n",
                   tid, newbrkP);
      else
         VG_(umsg)("Cannot map memory to grow brk segment in thread #%d "
                   "to %#lx\n", tid, newbrkP);
      goto bad;
   }

   VG_(brk_limit) = newbrk;
   return newbrk;

  bad:
   return VG_(brk_limit);
}


/* 
> - what does the "Bool soft" parameter mean?

(Tom Hughes, 3 Oct 05):

Whether or not to consider a file descriptor invalid if it is above
the current soft limit.

Basically if we are testing whether a newly created file descriptor is
valid (in a post handler) then we set soft to true, and if we are
testing whether a file descriptor that is about to be used (in a pre
handler) is valid [viz, an already-existing fd] then we set it to false.

The point is that if the (virtual) soft limit is lowered then any
existing descriptors can still be read/written/closed etc (so long as
they are below the valgrind reserved descriptors) but no new
descriptors can be created above the new soft limit.

(jrs 4 Oct 05: in which case, I've renamed it "isNewFd")
*/

Bool ML_(fd_allowed)(Int fd, const HChar *syscallname, ThreadId tid,
                     Bool isNewFd)
{
   Bool allowed = True;

   
   if (fd < 0 || fd >= VG_(fd_hard_limit))
      allowed = False;

   
   if (fd == VG_(log_output_sink).fd || fd == VG_(xml_output_sink).fd)
      allowed = False;

   if (isNewFd && fd >= VG_(fd_soft_limit))
      allowed = False;

   

   
   if ((!allowed) && VG_(showing_core_errors)() ) {
      VG_(message)(Vg_UserMsg, 
         "Warning: invalid file descriptor %d in syscall %s()\n",
         fd, syscallname);
      if (fd == VG_(log_output_sink).fd && VG_(log_output_sink).fd >= 0)
	 VG_(message)(Vg_UserMsg, 
            "   Use --log-fd=<number> to select an alternative log fd.\n");
      if (fd == VG_(xml_output_sink).fd && VG_(xml_output_sink).fd >= 0)
	 VG_(message)(Vg_UserMsg, 
            "   Use --xml-fd=<number> to select an alternative XML "
            "output fd.\n");
      
      
      
      if (VG_(clo_verbosity) > 1) {
         VG_(get_and_pp_StackTrace)(tid, VG_(clo_backtrace_size));
      }
   }

   return allowed;
}




void 
ML_(generic_PRE_sys_socketpair) ( ThreadId tid,
                                  UWord arg0, UWord arg1, 
                                  UWord arg2, UWord arg3 )
{
   
   PRE_MEM_WRITE( "socketcall.socketpair(sv)", 
                  arg3, 2*sizeof(int) );
}

SysRes
ML_(generic_POST_sys_socketpair) ( ThreadId tid,
                                   SysRes res,
                                   UWord arg0, UWord arg1, 
                                   UWord arg2, UWord arg3 )
{
   SysRes r = res;
   Int fd1 = ((Int*)arg3)[0];
   Int fd2 = ((Int*)arg3)[1];
   vg_assert(!sr_isError(res)); 
   POST_MEM_WRITE( arg3, 2*sizeof(int) );
   if (!ML_(fd_allowed)(fd1, "socketcall.socketpair", tid, True) ||
       !ML_(fd_allowed)(fd2, "socketcall.socketpair", tid, True)) {
      VG_(close)(fd1);
      VG_(close)(fd2);
      r = VG_(mk_SysRes_Error)( VKI_EMFILE );
   } else {
      POST_MEM_WRITE( arg3, 2*sizeof(int) );
      if (VG_(clo_track_fds)) {
         ML_(record_fd_open_nameless)(tid, fd1);
         ML_(record_fd_open_nameless)(tid, fd2);
      }
   }
   return r;
}


SysRes 
ML_(generic_POST_sys_socket) ( ThreadId tid, SysRes res )
{
   SysRes r = res;
   vg_assert(!sr_isError(res)); 
   if (!ML_(fd_allowed)(sr_Res(res), "socket", tid, True)) {
      VG_(close)(sr_Res(res));
      r = VG_(mk_SysRes_Error)( VKI_EMFILE );
   } else {
      if (VG_(clo_track_fds))
         ML_(record_fd_open_nameless)(tid, sr_Res(res));
   }
   return r;
}


void 
ML_(generic_PRE_sys_bind) ( ThreadId tid,
                            UWord arg0, UWord arg1, UWord arg2 )
{
   pre_mem_read_sockaddr( 
      tid, "socketcall.bind(my_addr.%s)",
      (struct vki_sockaddr *) arg1, arg2 
   );
}


void 
ML_(generic_PRE_sys_accept) ( ThreadId tid,
                              UWord arg0, UWord arg1, UWord arg2 )
{
   
   Addr addr_p     = arg1;
   Addr addrlen_p  = arg2;
   if (addr_p != (Addr)NULL) 
      ML_(buf_and_len_pre_check) ( tid, addr_p, addrlen_p,
                                   "socketcall.accept(addr)",
                                   "socketcall.accept(addrlen_in)" );
}

SysRes 
ML_(generic_POST_sys_accept) ( ThreadId tid,
                               SysRes res,
                               UWord arg0, UWord arg1, UWord arg2 )
{
   SysRes r = res;
   vg_assert(!sr_isError(res)); 
   if (!ML_(fd_allowed)(sr_Res(res), "accept", tid, True)) {
      VG_(close)(sr_Res(res));
      r = VG_(mk_SysRes_Error)( VKI_EMFILE );
   } else {
      Addr addr_p     = arg1;
      Addr addrlen_p  = arg2;
      if (addr_p != (Addr)NULL) 
         ML_(buf_and_len_post_check) ( tid, res, addr_p, addrlen_p,
                                       "socketcall.accept(addrlen_out)" );
      if (VG_(clo_track_fds))
          ML_(record_fd_open_nameless)(tid, sr_Res(res));
   }
   return r;
}


void 
ML_(generic_PRE_sys_sendto) ( ThreadId tid, 
                              UWord arg0, UWord arg1, UWord arg2,
                              UWord arg3, UWord arg4, UWord arg5 )
{
   PRE_MEM_READ( "socketcall.sendto(msg)",
                 arg1, 
                 arg2   );
   pre_mem_read_sockaddr( 
      tid, "socketcall.sendto(to.%s)",
      (struct vki_sockaddr *) arg4, arg5
   );
}


void 
ML_(generic_PRE_sys_send) ( ThreadId tid,
                            UWord arg0, UWord arg1, UWord arg2 )
{
   
   PRE_MEM_READ( "socketcall.send(msg)",
                  arg1, 
                  arg2   );

}


void 
ML_(generic_PRE_sys_recvfrom) ( ThreadId tid, 
                                UWord arg0, UWord arg1, UWord arg2,
                                UWord arg3, UWord arg4, UWord arg5 )
{
   Addr buf_p      = arg1;
   Int  len        = arg2;
   Addr from_p     = arg4;
   Addr fromlen_p  = arg5;
   PRE_MEM_WRITE( "socketcall.recvfrom(buf)", buf_p, len );
   if (from_p != (Addr)NULL) 
      ML_(buf_and_len_pre_check) ( tid, from_p, fromlen_p, 
                                   "socketcall.recvfrom(from)",
                                   "socketcall.recvfrom(fromlen_in)" );
}

void 
ML_(generic_POST_sys_recvfrom) ( ThreadId tid,
                                 SysRes res,
                                 UWord arg0, UWord arg1, UWord arg2,
                                 UWord arg3, UWord arg4, UWord arg5 )
{
   Addr buf_p      = arg1;
   Int  len        = arg2;
   Addr from_p     = arg4;
   Addr fromlen_p  = arg5;

   vg_assert(!sr_isError(res)); 
   if (from_p != (Addr)NULL) 
      ML_(buf_and_len_post_check) ( tid, res, from_p, fromlen_p,
                                    "socketcall.recvfrom(fromlen_out)" );
   POST_MEM_WRITE( buf_p, len );
}


void 
ML_(generic_PRE_sys_recv) ( ThreadId tid,
                            UWord arg0, UWord arg1, UWord arg2 )
{
   
   PRE_MEM_WRITE( "socketcall.recv(buf)", 
                  arg1, 
                  arg2   );
}

void 
ML_(generic_POST_sys_recv) ( ThreadId tid, 
                             UWord res,
                             UWord arg0, UWord arg1, UWord arg2 )
{
   if (res >= 0 && arg1 != 0) {
      POST_MEM_WRITE( arg1, 
                      arg2   );
   }
}


void 
ML_(generic_PRE_sys_connect) ( ThreadId tid,
                               UWord arg0, UWord arg1, UWord arg2 )
{
   pre_mem_read_sockaddr( tid,
                          "socketcall.connect(serv_addr.%s)",
                          (struct vki_sockaddr *) arg1, arg2);
}


void 
ML_(generic_PRE_sys_setsockopt) ( ThreadId tid, 
                                  UWord arg0, UWord arg1, UWord arg2,
                                  UWord arg3, UWord arg4 )
{
   PRE_MEM_READ( "socketcall.setsockopt(optval)",
                 arg3, 
                 arg4   );
}


void 
ML_(generic_PRE_sys_getsockname) ( ThreadId tid,
                                   UWord arg0, UWord arg1, UWord arg2 )
{
   
   Addr name_p     = arg1;
   Addr namelen_p  = arg2;
   
   ML_(buf_and_len_pre_check) ( tid, name_p, namelen_p,
                                "socketcall.getsockname(name)",
                                "socketcall.getsockname(namelen_in)" );
}

void 
ML_(generic_POST_sys_getsockname) ( ThreadId tid,
                                    SysRes res,
                                    UWord arg0, UWord arg1, UWord arg2 )
{
   Addr name_p     = arg1;
   Addr namelen_p  = arg2;
   vg_assert(!sr_isError(res)); 
   ML_(buf_and_len_post_check) ( tid, res, name_p, namelen_p,
                                 "socketcall.getsockname(namelen_out)" );
}


void 
ML_(generic_PRE_sys_getpeername) ( ThreadId tid,
                                   UWord arg0, UWord arg1, UWord arg2 )
{
   
   Addr name_p     = arg1;
   Addr namelen_p  = arg2;
   
   ML_(buf_and_len_pre_check) ( tid, name_p, namelen_p,
                                "socketcall.getpeername(name)",
                                "socketcall.getpeername(namelen_in)" );
}

void 
ML_(generic_POST_sys_getpeername) ( ThreadId tid,
                                    SysRes res,
                                    UWord arg0, UWord arg1, UWord arg2 )
{
   Addr name_p     = arg1;
   Addr namelen_p  = arg2;
   vg_assert(!sr_isError(res)); 
   ML_(buf_and_len_post_check) ( tid, res, name_p, namelen_p,
                                 "socketcall.getpeername(namelen_out)" );
}


void 
ML_(generic_PRE_sys_sendmsg) ( ThreadId tid, const HChar *name,
                               struct vki_msghdr *msg )
{
   msghdr_foreachfield ( tid, name, msg, ~0, pre_mem_read_sendmsg, False );
}


void
ML_(generic_PRE_sys_recvmsg) ( ThreadId tid, const HChar *name,
                               struct vki_msghdr *msg )
{
   msghdr_foreachfield ( tid, name, msg, ~0, pre_mem_write_recvmsg, True );
}

void 
ML_(generic_POST_sys_recvmsg) ( ThreadId tid, const HChar *name,
                                struct vki_msghdr *msg, UInt length )
{
   msghdr_foreachfield( tid, name, msg, length, post_mem_write_recvmsg, True );
   check_cmsg_for_fds( tid, msg );
}




void
ML_(generic_PRE_sys_semop) ( ThreadId tid,
                             UWord arg0, UWord arg1, UWord arg2 )
{
   
   PRE_MEM_READ( "semop(sops)", arg1, arg2 * sizeof(struct vki_sembuf) );
}


void
ML_(generic_PRE_sys_semtimedop) ( ThreadId tid,
                                  UWord arg0, UWord arg1,
                                  UWord arg2, UWord arg3 )
{
   PRE_MEM_READ( "semtimedop(sops)", arg1, arg2 * sizeof(struct vki_sembuf) );
   if (arg3 != 0)
      PRE_MEM_READ( "semtimedop(timeout)", arg3, sizeof(struct vki_timespec) );
}


static
UInt get_sem_count( Int semid )
{
   struct vki_semid_ds buf;
   union vki_semun arg;
   SysRes res;

   buf.sem_nsems = 0;

   arg.buf = &buf;

#  ifdef __NR_semctl
   res = VG_(do_syscall4)(__NR_semctl, semid, 0, VKI_IPC_STAT, *(UWord *)&arg);
#  else
   res = VG_(do_syscall5)(__NR_ipc, 3 , semid, 0,
                          VKI_IPC_STAT, (UWord)&arg);
#  endif
   if (sr_isError(res))
      return 0;

   return buf.sem_nsems;
}

void
ML_(generic_PRE_sys_semctl) ( ThreadId tid,
                              UWord arg0, UWord arg1,
                              UWord arg2, UWord arg3 )
{
   
   union vki_semun arg = *(union vki_semun *)&arg3;
   UInt nsems;
   switch (arg2 ) {
#if defined(VKI_IPC_INFO)
   case VKI_IPC_INFO:
   case VKI_SEM_INFO:
   case VKI_IPC_INFO|VKI_IPC_64:
   case VKI_SEM_INFO|VKI_IPC_64:
      PRE_MEM_WRITE( "semctl(IPC_INFO, arg.buf)",
                     (Addr)arg.buf, sizeof(struct vki_seminfo) );
      break;
#endif

   case VKI_IPC_STAT:
#if defined(VKI_SEM_STAT)
   case VKI_SEM_STAT:
#endif
      PRE_MEM_WRITE( "semctl(IPC_STAT, arg.buf)",
                     (Addr)arg.buf, sizeof(struct vki_semid_ds) );
      break;

#if defined(VKI_IPC_64)
   case VKI_IPC_STAT|VKI_IPC_64:
#if defined(VKI_SEM_STAT)
   case VKI_SEM_STAT|VKI_IPC_64:
#endif
      PRE_MEM_WRITE( "semctl(IPC_STAT, arg.buf)",
                     (Addr)arg.buf, sizeof(struct vki_semid64_ds) );
      break;
#endif

   case VKI_IPC_SET:
      PRE_MEM_READ( "semctl(IPC_SET, arg.buf)",
                    (Addr)arg.buf, sizeof(struct vki_semid_ds) );
      break;

#if defined(VKI_IPC_64)
   case VKI_IPC_SET|VKI_IPC_64:
      PRE_MEM_READ( "semctl(IPC_SET, arg.buf)",
                    (Addr)arg.buf, sizeof(struct vki_semid64_ds) );
      break;
#endif

   case VKI_GETALL:
#if defined(VKI_IPC_64)
   case VKI_GETALL|VKI_IPC_64:
#endif
      nsems = get_sem_count( arg0 );
      PRE_MEM_WRITE( "semctl(IPC_GETALL, arg.array)",
                     (Addr)arg.array, sizeof(unsigned short) * nsems );
      break;

   case VKI_SETALL:
#if defined(VKI_IPC_64)
   case VKI_SETALL|VKI_IPC_64:
#endif
      nsems = get_sem_count( arg0 );
      PRE_MEM_READ( "semctl(IPC_SETALL, arg.array)",
                    (Addr)arg.array, sizeof(unsigned short) * nsems );
      break;
   }
}

void
ML_(generic_POST_sys_semctl) ( ThreadId tid,
                               UWord res,
                               UWord arg0, UWord arg1,
                               UWord arg2, UWord arg3 )
{
   union vki_semun arg = *(union vki_semun *)&arg3;
   UInt nsems;
   switch (arg2 ) {
#if defined(VKI_IPC_INFO)
   case VKI_IPC_INFO:
   case VKI_SEM_INFO:
   case VKI_IPC_INFO|VKI_IPC_64:
   case VKI_SEM_INFO|VKI_IPC_64:
      POST_MEM_WRITE( (Addr)arg.buf, sizeof(struct vki_seminfo) );
      break;
#endif

   case VKI_IPC_STAT:
#if defined(VKI_SEM_STAT)
   case VKI_SEM_STAT:
#endif
      POST_MEM_WRITE( (Addr)arg.buf, sizeof(struct vki_semid_ds) );
      break;

#if defined(VKI_IPC_64)
   case VKI_IPC_STAT|VKI_IPC_64:
   case VKI_SEM_STAT|VKI_IPC_64:
      POST_MEM_WRITE( (Addr)arg.buf, sizeof(struct vki_semid64_ds) );
      break;
#endif

   case VKI_GETALL:
#if defined(VKI_IPC_64)
   case VKI_GETALL|VKI_IPC_64:
#endif
      nsems = get_sem_count( arg0 );
      POST_MEM_WRITE( (Addr)arg.array, sizeof(unsigned short) * nsems );
      break;
   }
}



static
SizeT get_shm_size ( Int shmid )
{
#ifdef __NR_shmctl
#  ifdef VKI_IPC_64
   struct vki_shmid64_ds buf;
#    if defined(VGP_amd64_linux) || defined(VGP_arm64_linux)
     
     SysRes __res = VG_(do_syscall3)(__NR_shmctl, shmid, 
                                     VKI_IPC_STAT, (UWord)&buf);
#    else
     SysRes __res = VG_(do_syscall3)(__NR_shmctl, shmid,
                                     VKI_IPC_STAT|VKI_IPC_64, (UWord)&buf);
#    endif
#  else 
   struct vki_shmid_ds buf;
   SysRes __res = VG_(do_syscall3)(__NR_shmctl, shmid, VKI_IPC_STAT, (UWord)&buf);
#  endif 
#else
   struct vki_shmid_ds buf;
   SysRes __res = VG_(do_syscall5)(__NR_ipc, 24 , shmid,
                                 VKI_IPC_STAT, 0, (UWord)&buf);
#endif
   if (sr_isError(__res))
      return 0;
 
   return (SizeT) buf.shm_segsz;
}

UWord
ML_(generic_PRE_sys_shmat) ( ThreadId tid,
                             UWord arg0, UWord arg1, UWord arg2 )
{
   
   SizeT  segmentSize = get_shm_size ( arg0 );
   UWord tmp;
   Bool  ok;
   if (arg1 == 0) {
      vg_assert(VKI_SHMLBA >= VKI_PAGE_SIZE);
      if (VKI_SHMLBA > VKI_PAGE_SIZE) {
         segmentSize += VKI_SHMLBA - VKI_PAGE_SIZE;
      }
      tmp = VG_(am_get_advisory_client_simple)(0, segmentSize, &ok);
      if (ok) {
         if (VKI_SHMLBA > VKI_PAGE_SIZE) {
            arg1 = VG_ROUNDUP(tmp, VKI_SHMLBA);
         } else {
            arg1 = tmp;
         }
      }
   }
   else if (!ML_(valid_client_addr)(arg1, segmentSize, tid, "shmat"))
      arg1 = 0;
   return arg1;
}

void
ML_(generic_POST_sys_shmat) ( ThreadId tid,
                              UWord res,
                              UWord arg0, UWord arg1, UWord arg2 )
{
   SizeT segmentSize = VG_PGROUNDUP(get_shm_size(arg0));
   if ( segmentSize > 0 ) {
      UInt prot = VKI_PROT_READ|VKI_PROT_WRITE;
      Bool d;

      if (arg2 & VKI_SHM_RDONLY)
         prot &= ~VKI_PROT_WRITE;
      d = VG_(am_notify_client_shmat)( res, segmentSize, prot );

      VG_TRACK( new_mem_mmap, res, segmentSize, True, True, False,
                              0 );
      if (d)
         VG_(discard_translations)( (Addr)res, 
                                    (ULong)VG_PGROUNDUP(segmentSize),
                                    "ML_(generic_POST_sys_shmat)" );
   }
}


Bool
ML_(generic_PRE_sys_shmdt) ( ThreadId tid, UWord arg0 )
{
   
   return ML_(valid_client_addr)(arg0, 1, tid, "shmdt");
}

void
ML_(generic_POST_sys_shmdt) ( ThreadId tid, UWord res, UWord arg0 )
{
   NSegment const* s = VG_(am_find_nsegment)(arg0);

   if (s != NULL) {
      Addr  s_start = s->start;
      SizeT s_len   = s->end+1 - s->start;
      Bool  d;

      vg_assert(s->kind == SkShmC);
      vg_assert(s->start == arg0);

      d = VG_(am_notify_munmap)(s_start, s_len);
      s = NULL; 
      VG_TRACK( die_mem_munmap, s_start, s_len );
      if (d)
         VG_(discard_translations)( s_start,
                                    (ULong)s_len,
                                    "ML_(generic_POST_sys_shmdt)" );
   }
}

void
ML_(generic_PRE_sys_shmctl) ( ThreadId tid,
                              UWord arg0, UWord arg1, UWord arg2 )
{
   
   switch (arg1 ) {
#if defined(VKI_IPC_INFO)
   case VKI_IPC_INFO:
      PRE_MEM_WRITE( "shmctl(IPC_INFO, buf)",
                     arg2, sizeof(struct vki_shminfo) );
      break;
#if defined(VKI_IPC_64)
   case VKI_IPC_INFO|VKI_IPC_64:
      PRE_MEM_WRITE( "shmctl(IPC_INFO, buf)",
                     arg2, sizeof(struct vki_shminfo64) );
      break;
#endif
#endif

#if defined(VKI_SHM_INFO)
   case VKI_SHM_INFO:
#if defined(VKI_IPC_64)
   case VKI_SHM_INFO|VKI_IPC_64:
#endif
      PRE_MEM_WRITE( "shmctl(SHM_INFO, buf)",
                     arg2, sizeof(struct vki_shm_info) );
      break;
#endif

   case VKI_IPC_STAT:
#if defined(VKI_SHM_STAT)
   case VKI_SHM_STAT:
#endif
      PRE_MEM_WRITE( "shmctl(IPC_STAT, buf)",
                     arg2, sizeof(struct vki_shmid_ds) );
      break;

#if defined(VKI_IPC_64)
   case VKI_IPC_STAT|VKI_IPC_64:
   case VKI_SHM_STAT|VKI_IPC_64:
      PRE_MEM_WRITE( "shmctl(IPC_STAT, arg.buf)",
                     arg2, sizeof(struct vki_shmid64_ds) );
      break;
#endif

   case VKI_IPC_SET:
      PRE_MEM_READ( "shmctl(IPC_SET, arg.buf)",
                    arg2, sizeof(struct vki_shmid_ds) );
      break;

#if defined(VKI_IPC_64)
   case VKI_IPC_SET|VKI_IPC_64:
      PRE_MEM_READ( "shmctl(IPC_SET, arg.buf)",
                    arg2, sizeof(struct vki_shmid64_ds) );
      break;
#endif
   }
}

void
ML_(generic_POST_sys_shmctl) ( ThreadId tid,
                               UWord res,
                               UWord arg0, UWord arg1, UWord arg2 )
{
   switch (arg1 ) {
#if defined(VKI_IPC_INFO)
   case VKI_IPC_INFO:
      POST_MEM_WRITE( arg2, sizeof(struct vki_shminfo) );
      break;
   case VKI_IPC_INFO|VKI_IPC_64:
      POST_MEM_WRITE( arg2, sizeof(struct vki_shminfo64) );
      break;
#endif

#if defined(VKI_SHM_INFO)
   case VKI_SHM_INFO:
   case VKI_SHM_INFO|VKI_IPC_64:
      POST_MEM_WRITE( arg2, sizeof(struct vki_shm_info) );
      break;
#endif

   case VKI_IPC_STAT:
#if defined(VKI_SHM_STAT)
   case VKI_SHM_STAT:
#endif
      POST_MEM_WRITE( arg2, sizeof(struct vki_shmid_ds) );
      break;

#if defined(VKI_IPC_64)
   case VKI_IPC_STAT|VKI_IPC_64:
   case VKI_SHM_STAT|VKI_IPC_64:
      POST_MEM_WRITE( arg2, sizeof(struct vki_shmid64_ds) );
      break;
#endif


   }
}



SysRes
ML_(generic_PRE_sys_mmap) ( ThreadId tid,
                            UWord arg1, UWord arg2, UWord arg3,
                            UWord arg4, UWord arg5, Off64T arg6 )
{
   Addr       advised;
   SysRes     sres;
   MapRequest mreq;
   Bool       mreq_ok;

#  if defined(VGO_darwin)
   
   
   
   
   
   
   VG_(core_panic)("can't use ML_(generic_PRE_sys_mmap) on Darwin");
#  endif

   if (arg2 == 0) {
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }

   if (!VG_IS_PAGE_ALIGNED(arg1)) {
      
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }

   if (!VG_IS_PAGE_ALIGNED(arg6)) {
      
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }

#  if defined(VKI_MAP_32BIT)
   if (arg4 & VKI_MAP_32BIT) {
      return VG_(mk_SysRes_Error)( VKI_ENOMEM );
   }
#  endif

   mreq.start = arg1;
   mreq.len   = arg2;
   if (arg4 & VKI_MAP_FIXED) {
      mreq.rkind = MFixed;
   } else
   if (arg1 != 0) {
      mreq.rkind = MHint;
   } else {
      mreq.rkind = MAny;
   }

   
   advised = VG_(am_get_advisory)( &mreq, True, &mreq_ok );
   if (!mreq_ok) {
      
      return VG_(mk_SysRes_Error)( VKI_EINVAL );
   }

   sres = VG_(am_do_mmap_NO_NOTIFY)(advised, arg2, arg3,
                                    arg4 | VKI_MAP_FIXED,
                                    arg5, arg6);

   if (mreq.rkind == MHint && sr_isError(sres)) {
      mreq.start = 0;
      mreq.len   = arg2;
      mreq.rkind = MAny;
      advised = VG_(am_get_advisory)( &mreq, True, &mreq_ok );
      if (!mreq_ok) {
         
         return VG_(mk_SysRes_Error)( VKI_EINVAL );
      }
      
      sres = VG_(am_do_mmap_NO_NOTIFY)(advised, arg2, arg3,
                                       arg4 | VKI_MAP_FIXED,
                                       arg5, arg6);
   }

   if (sr_isError(sres) && !(arg4 & VKI_MAP_FIXED)) {
      advised = 0; 
      sres = VG_(am_do_mmap_NO_NOTIFY)(advised, arg2, arg3,
                                       arg4,
                                       arg5, arg6);
      if (!sr_isError(sres)) {
         vg_assert(VG_(am_covered_by_single_free_segment)((Addr)sr_Res(sres),
                                                           arg2));
      }
   }

   if (!sr_isError(sres)) {
      ULong di_handle;
      
      notify_core_of_mmap(
         (Addr)sr_Res(sres), 
         arg2, 
         arg3, 
         arg4, 
         arg5, 
         arg6  
      );
      
      di_handle = VG_(di_notify_mmap)( (Addr)sr_Res(sres), 
                                       False, (Int)arg5 );
      
      notify_tool_of_mmap(
         (Addr)sr_Res(sres), 
         arg2, 
         arg3, 
         di_handle 
      );
   }

   
   if (!sr_isError(sres) && (arg4 & VKI_MAP_FIXED))
      vg_assert(sr_Res(sres) == arg1);

   return sres;
}




#define PRE(name)      DEFN_PRE_TEMPLATE(generic, name)
#define POST(name)     DEFN_POST_TEMPLATE(generic, name)

#if defined(VG_LITTLEENDIAN)
#define MERGE64(lo,hi)   ( ((ULong)(lo)) | (((ULong)(hi)) << 32) )
#define MERGE64_FIRST(name) name##_low
#define MERGE64_SECOND(name) name##_high
#elif defined(VG_BIGENDIAN)
#define MERGE64(hi,lo)   ( ((ULong)(lo)) | (((ULong)(hi)) << 32) )
#define MERGE64_FIRST(name) name##_high
#define MERGE64_SECOND(name) name##_low
#else
#error Unknown endianness
#endif

PRE(sys_exit)
{
   ThreadState* tst;
   
   PRINT("exit( %ld )", ARG1);
   PRE_REG_READ1(void, "exit", int, status);
   tst = VG_(get_ThreadState)(tid);
   tst->exitreason = VgSrc_ExitThread;
   tst->os_state.exitcode = ARG1;
   SET_STATUS_Success(0);
}

PRE(sys_ni_syscall)
{
   PRINT("unimplemented (by the kernel) syscall: %s! (ni_syscall)\n",
      VG_SYSNUM_STRING(SYSNO));
   PRE_REG_READ0(long, "ni_syscall");
   SET_STATUS_Failure( VKI_ENOSYS );
}

PRE(sys_iopl)
{
   PRINT("sys_iopl ( %ld )", ARG1);
   PRE_REG_READ1(long, "iopl", unsigned long, level);
}

PRE(sys_fsync)
{
   *flags |= SfMayBlock;
   PRINT("sys_fsync ( %ld )", ARG1);
   PRE_REG_READ1(long, "fsync", unsigned int, fd);
}

PRE(sys_fdatasync)
{
   *flags |= SfMayBlock;
   PRINT("sys_fdatasync ( %ld )", ARG1);
   PRE_REG_READ1(long, "fdatasync", unsigned int, fd);
}

PRE(sys_msync)
{
   *flags |= SfMayBlock;
   PRINT("sys_msync ( %#lx, %llu, %ld )", ARG1,(ULong)ARG2,ARG3);
   PRE_REG_READ3(long, "msync",
                 unsigned long, start, vki_size_t, length, int, flags);
   PRE_MEM_READ( "msync(start)", ARG1, ARG2 );
}

struct vki_pmsg_strbuf {
   int     maxlen;         
   int     len;            
   vki_caddr_t buf;        
};
PRE(sys_getpmsg)
{
   
   struct vki_pmsg_strbuf *ctrl;
   struct vki_pmsg_strbuf *data;
   *flags |= SfMayBlock;
   PRINT("sys_getpmsg ( %ld, %#lx, %#lx, %#lx, %#lx )", ARG1,ARG2,ARG3,ARG4,ARG5);
   PRE_REG_READ5(int, "getpmsg",
                 int, fd, struct strbuf *, ctrl, struct strbuf *, data, 
                 int *, bandp, int *, flagsp);
   ctrl = (struct vki_pmsg_strbuf *)ARG2;
   data = (struct vki_pmsg_strbuf *)ARG3;
   if (ctrl && ctrl->maxlen > 0)
      PRE_MEM_WRITE( "getpmsg(ctrl)", (Addr)ctrl->buf, ctrl->maxlen);
   if (data && data->maxlen > 0)
      PRE_MEM_WRITE( "getpmsg(data)", (Addr)data->buf, data->maxlen);
   if (ARG4)
      PRE_MEM_WRITE( "getpmsg(bandp)", (Addr)ARG4, sizeof(int));
   if (ARG5)
      PRE_MEM_WRITE( "getpmsg(flagsp)", (Addr)ARG5, sizeof(int));
}
POST(sys_getpmsg)
{
   struct vki_pmsg_strbuf *ctrl;
   struct vki_pmsg_strbuf *data;
   vg_assert(SUCCESS);
   ctrl = (struct vki_pmsg_strbuf *)ARG2;
   data = (struct vki_pmsg_strbuf *)ARG3;
   if (RES == 0 && ctrl && ctrl->len > 0) {
      POST_MEM_WRITE( (Addr)ctrl->buf, ctrl->len);
   }
   if (RES == 0 && data && data->len > 0) {
      POST_MEM_WRITE( (Addr)data->buf, data->len);
   }
}

PRE(sys_putpmsg)
{
   
   struct vki_pmsg_strbuf *ctrl;
   struct vki_pmsg_strbuf *data;
   *flags |= SfMayBlock;
   PRINT("sys_putpmsg ( %ld, %#lx, %#lx, %ld, %ld )", ARG1,ARG2,ARG3,ARG4,ARG5);
   PRE_REG_READ5(int, "putpmsg",
                 int, fd, struct strbuf *, ctrl, struct strbuf *, data, 
                 int, band, int, flags);
   ctrl = (struct vki_pmsg_strbuf *)ARG2;
   data = (struct vki_pmsg_strbuf *)ARG3;
   if (ctrl && ctrl->len > 0)
      PRE_MEM_READ( "putpmsg(ctrl)", (Addr)ctrl->buf, ctrl->len);
   if (data && data->len > 0)
      PRE_MEM_READ( "putpmsg(data)", (Addr)data->buf, data->len);
}

PRE(sys_getitimer)
{
   struct vki_itimerval *value = (struct vki_itimerval*)ARG2;
   PRINT("sys_getitimer ( %ld, %#lx )", ARG1, ARG2);
   PRE_REG_READ2(long, "getitimer", int, which, struct itimerval *, value);

   PRE_timeval_WRITE( "getitimer(&value->it_interval)", &(value->it_interval));
   PRE_timeval_WRITE( "getitimer(&value->it_value)",    &(value->it_value));
}

POST(sys_getitimer)
{
   if (ARG2 != (Addr)NULL) {
      struct vki_itimerval *value = (struct vki_itimerval*)ARG2;
      POST_timeval_WRITE( &(value->it_interval) );
      POST_timeval_WRITE( &(value->it_value) );
   }
}

PRE(sys_setitimer)
{
   PRINT("sys_setitimer ( %ld, %#lx, %#lx )", ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "setitimer", 
                 int, which,
                 struct itimerval *, value, struct itimerval *, ovalue);
   if (ARG2 != (Addr)NULL) {
      struct vki_itimerval *value = (struct vki_itimerval*)ARG2;
      PRE_timeval_READ( "setitimer(&value->it_interval)",
                         &(value->it_interval));
      PRE_timeval_READ( "setitimer(&value->it_value)",
                         &(value->it_value));
   }
   if (ARG3 != (Addr)NULL) {
      struct vki_itimerval *ovalue = (struct vki_itimerval*)ARG3;
      PRE_timeval_WRITE( "setitimer(&ovalue->it_interval)",
                         &(ovalue->it_interval));
      PRE_timeval_WRITE( "setitimer(&ovalue->it_value)",
                         &(ovalue->it_value));
   }
}

POST(sys_setitimer)
{
   if (ARG3 != (Addr)NULL) {
      struct vki_itimerval *ovalue = (struct vki_itimerval*)ARG3;
      POST_timeval_WRITE( &(ovalue->it_interval) );
      POST_timeval_WRITE( &(ovalue->it_value) );
   }
}

PRE(sys_chroot)
{
   PRINT("sys_chroot ( %#lx )", ARG1);
   PRE_REG_READ1(long, "chroot", const char *, path);
   PRE_MEM_RASCIIZ( "chroot(path)", ARG1 );
}

PRE(sys_madvise)
{
   *flags |= SfMayBlock;
   PRINT("sys_madvise ( %#lx, %llu, %ld )", ARG1,(ULong)ARG2,ARG3);
   PRE_REG_READ3(long, "madvise",
                 unsigned long, start, vki_size_t, length, int, advice);
}

#if HAVE_MREMAP
PRE(sys_mremap)
{
   
   
   if (ARG4 & VKI_MREMAP_FIXED) {
      PRINT("sys_mremap ( %#lx, %llu, %ld, 0x%lx, %#lx )",
            ARG1, (ULong)ARG2, ARG3, ARG4, ARG5);
      PRE_REG_READ5(unsigned long, "mremap",
                    unsigned long, old_addr, unsigned long, old_size,
                    unsigned long, new_size, unsigned long, flags,
                    unsigned long, new_addr);
   } else {
      PRINT("sys_mremap ( %#lx, %llu, %ld, 0x%lx )",
            ARG1, (ULong)ARG2, ARG3, ARG4);
      PRE_REG_READ4(unsigned long, "mremap",
                    unsigned long, old_addr, unsigned long, old_size,
                    unsigned long, new_size, unsigned long, flags);
   }
   SET_STATUS_from_SysRes( 
      do_mremap((Addr)ARG1, ARG2, (Addr)ARG5, ARG3, ARG4, tid) 
   );
}
#endif 

PRE(sys_nice)
{
   PRINT("sys_nice ( %ld )", ARG1);
   PRE_REG_READ1(long, "nice", int, inc);
}

PRE(sys_mlock)
{
   *flags |= SfMayBlock;
   PRINT("sys_mlock ( %#lx, %llu )", ARG1, (ULong)ARG2);
   PRE_REG_READ2(long, "mlock", unsigned long, addr, vki_size_t, len);
}

PRE(sys_munlock)
{
   *flags |= SfMayBlock;
   PRINT("sys_munlock ( %#lx, %llu )", ARG1, (ULong)ARG2);
   PRE_REG_READ2(long, "munlock", unsigned long, addr, vki_size_t, len);
}

PRE(sys_mlockall)
{
   *flags |= SfMayBlock;
   PRINT("sys_mlockall ( %lx )", ARG1);
   PRE_REG_READ1(long, "mlockall", int, flags);
}

PRE(sys_setpriority)
{
   PRINT("sys_setpriority ( %ld, %ld, %ld )", ARG1, ARG2, ARG3);
   PRE_REG_READ3(long, "setpriority", int, which, int, who, int, prio);
}

PRE(sys_getpriority)
{
   PRINT("sys_getpriority ( %ld, %ld )", ARG1, ARG2);
   PRE_REG_READ2(long, "getpriority", int, which, int, who);
}

PRE(sys_pwrite64)
{
   *flags |= SfMayBlock;
#if VG_WORDSIZE == 4
   PRINT("sys_pwrite64 ( %ld, %#lx, %llu, %lld )",
         ARG1, ARG2, (ULong)ARG3, MERGE64(ARG4,ARG5));
   PRE_REG_READ5(ssize_t, "pwrite64",
                 unsigned int, fd, const char *, buf, vki_size_t, count,
                 vki_u32, MERGE64_FIRST(offset), vki_u32, MERGE64_SECOND(offset));
#elif VG_WORDSIZE == 8
   PRINT("sys_pwrite64 ( %ld, %#lx, %llu, %lld )",
         ARG1, ARG2, (ULong)ARG3, (Long)ARG4);
   PRE_REG_READ4(ssize_t, "pwrite64",
                 unsigned int, fd, const char *, buf, vki_size_t, count,
                 Word, offset);
#else
#  error Unexpected word size
#endif
   PRE_MEM_READ( "pwrite64(buf)", ARG2, ARG3 );
}

PRE(sys_sync)
{
   *flags |= SfMayBlock;
   PRINT("sys_sync ( )");
   PRE_REG_READ0(long, "sync");
}

PRE(sys_fstatfs)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_fstatfs ( %ld, %#lx )",ARG1,ARG2);
   PRE_REG_READ2(long, "fstatfs",
                 unsigned int, fd, struct statfs *, buf);
   PRE_MEM_WRITE( "fstatfs(buf)", ARG2, sizeof(struct vki_statfs) );
}

POST(sys_fstatfs)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_statfs) );
}

PRE(sys_fstatfs64)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_fstatfs64 ( %ld, %llu, %#lx )",ARG1,(ULong)ARG2,ARG3);
   PRE_REG_READ3(long, "fstatfs64",
                 unsigned int, fd, vki_size_t, size, struct statfs64 *, buf);
   PRE_MEM_WRITE( "fstatfs64(buf)", ARG3, ARG2 );
}
POST(sys_fstatfs64)
{
   POST_MEM_WRITE( ARG3, ARG2 );
}

PRE(sys_getsid)
{
   PRINT("sys_getsid ( %ld )", ARG1);
   PRE_REG_READ1(long, "getsid", vki_pid_t, pid);
}

PRE(sys_pread64)
{
   *flags |= SfMayBlock;
#if VG_WORDSIZE == 4
   PRINT("sys_pread64 ( %ld, %#lx, %llu, %lld )",
         ARG1, ARG2, (ULong)ARG3, MERGE64(ARG4,ARG5));
   PRE_REG_READ5(ssize_t, "pread64",
                 unsigned int, fd, char *, buf, vki_size_t, count,
                 vki_u32, MERGE64_FIRST(offset), vki_u32, MERGE64_SECOND(offset));
#elif VG_WORDSIZE == 8
   PRINT("sys_pread64 ( %ld, %#lx, %llu, %lld )",
         ARG1, ARG2, (ULong)ARG3, (Long)ARG4);
   PRE_REG_READ4(ssize_t, "pread64",
                 unsigned int, fd, char *, buf, vki_size_t, count,
                 Word, offset);
#else
#  error Unexpected word size
#endif
   PRE_MEM_WRITE( "pread64(buf)", ARG2, ARG3 );
}
POST(sys_pread64)
{
   vg_assert(SUCCESS);
   if (RES > 0) {
      POST_MEM_WRITE( ARG2, RES );
   }
}

PRE(sys_mknod)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_mknod ( %#lx(%s), 0x%lx, 0x%lx )", ARG1, (char*)ARG1, ARG2, ARG3 );
   PRE_REG_READ3(long, "mknod",
                 const char *, pathname, int, mode, unsigned, dev);
   PRE_MEM_RASCIIZ( "mknod(pathname)", ARG1 );
}

PRE(sys_flock)
{
   *flags |= SfMayBlock;
   PRINT("sys_flock ( %ld, %ld )", ARG1, ARG2 );
   PRE_REG_READ2(long, "flock", unsigned int, fd, unsigned int, operation);
}

static void pre_argv_envp(Addr a, ThreadId tid, const HChar* s1, const HChar* s2)
{
   while (True) {
      Addr a_deref;
      Addr* a_p = (Addr*)a;
      PRE_MEM_READ( s1, (Addr)a_p, sizeof(Addr) );
      a_deref = *a_p;
      if (0 == a_deref)
         break;
      PRE_MEM_RASCIIZ( s2, a_deref );
      a += sizeof(char*);
   }
}

static Bool i_am_the_only_thread ( void )
{
   Int c = VG_(count_living_threads)();
   vg_assert(c >= 1); 
   return c == 1;
}

void VG_(reap_threads)(ThreadId self)
{
   while (!i_am_the_only_thread()) {
      
      VG_(vg_yield)();
      VG_(poll_signals)(self);
   }
   vg_assert(i_am_the_only_thread());
}

PRE(sys_execve)
{
   HChar*       path = NULL;       
   HChar**      envp = NULL;
   HChar**      argv = NULL;
   HChar**      arg2copy;
   HChar*       launcher_basename = NULL;
   ThreadState* tst;
   Int          i, j, tot_args;
   SysRes       res;
   Bool         setuid_allowed, trace_this_child;

   PRINT("sys_execve ( %#lx(%s), %#lx, %#lx )", ARG1, (char*)ARG1, ARG2, ARG3);
   PRE_REG_READ3(vki_off_t, "execve",
                 char *, filename, char **, argv, char **, envp);
   PRE_MEM_RASCIIZ( "execve(filename)", ARG1 );
   if (ARG2 != 0)
      pre_argv_envp( ARG2, tid, "execve(argv)", "execve(argv[i])" );
   if (ARG3 != 0)
      pre_argv_envp( ARG3, tid, "execve(envp)", "execve(envp[i])" );

   vg_assert(VG_(is_valid_tid)(tid));
   tst = VG_(get_ThreadState)(tid);


   
   if (ARG1 == 0 
       || !VG_(am_is_valid_for_client)( ARG1, 1, VKI_PROT_READ )) {
      SET_STATUS_Failure( VKI_EFAULT );
      return;
   }

   
   if (0) {
      VG_(printf)("ARG1 = %p(%s)\n", (void*)ARG1, (HChar*)ARG1);
      if (ARG2) {
         VG_(printf)("ARG2 = ");
         Int q;
         HChar** vec = (HChar**)ARG2;
         for (q = 0; vec[q]; q++)
            VG_(printf)("%p(%s) ", vec[q], vec[q]);
         VG_(printf)("\n");
      } else {
         VG_(printf)("ARG2 = null\n");
      }
   }

   
   { 
     
     const HChar** child_argv = (const HChar**)ARG2;
     if (child_argv && child_argv[0] == NULL)
        child_argv = NULL;
     trace_this_child = VG_(should_we_trace_this_child)( (HChar*)ARG1, child_argv );
   }

   
   
   
   setuid_allowed = trace_this_child  ? False  : True;
   res = VG_(pre_exec_check)((const HChar *)ARG1, NULL, setuid_allowed);
   if (sr_isError(res)) {
      SET_STATUS_Failure( sr_Err(res) );
      return;
   }

   if (trace_this_child 
       && (VG_(name_of_launcher) == NULL
           || VG_(name_of_launcher)[0] != '/')) {
      SET_STATUS_Failure( VKI_ECHILD ); 
      return;
   }

   
   VG_(debugLog)(1, "syswrap", "Exec of %s\n", (HChar*)ARG1);

   
   
   if (VG_(clo_vgdb)  != Vg_VgdbNo) {
      
      
      
      
      VG_(gdbserver) (0);
   }

   VG_(nuke_all_threads_except)( tid, VgSrc_ExitThread );
   VG_(reap_threads)(tid);

   
   
   if (trace_this_child) {

      
      path = VG_(name_of_launcher);
      
      
      vg_assert(path);

      launcher_basename = VG_(strrchr)(path, '/');
      if (launcher_basename == NULL || launcher_basename[1] == 0) {
         launcher_basename = path;  
      } else {
         launcher_basename++;
      }

   } else {
      path = (HChar*)ARG1;
   }

   
   
   
   
   
   
   
   
   
   
   
   if (ARG3 == 0) {
      envp = NULL;
   } else {
      envp = VG_(env_clone)( (HChar**)ARG3 );
      if (envp == NULL) goto hosed;
      VG_(env_remove_valgrind_env_stuff)( envp );
   }

   if (trace_this_child) {
      
      VG_(env_setenv)( &envp, VALGRIND_LIB, VG_(libdir));
   }

   
   
   
   
   
   
   
   
   if (!trace_this_child) {
      argv = (HChar**)ARG2;
   } else {
      vg_assert( VG_(args_for_valgrind) );
      vg_assert( VG_(args_for_valgrind_noexecpass) >= 0 );
      vg_assert( VG_(args_for_valgrind_noexecpass) 
                   <= VG_(sizeXA)( VG_(args_for_valgrind) ) );
      
      
      tot_args = 1;
      
      tot_args += VG_(sizeXA)( VG_(args_for_valgrind) );
      tot_args -= VG_(args_for_valgrind_noexecpass);
      
      tot_args++;
      
      arg2copy = (HChar**)ARG2;
      if (arg2copy && arg2copy[0]) {
         for (i = 1; arg2copy[i]; i++)
            tot_args++;
      }
      
      argv = VG_(malloc)( "di.syswrap.pre_sys_execve.1",
                          (tot_args+1) * sizeof(HChar*) );
      
      j = 0;
      argv[j++] = launcher_basename;
      for (i = 0; i < VG_(sizeXA)( VG_(args_for_valgrind) ); i++) {
         if (i < VG_(args_for_valgrind_noexecpass))
            continue;
         argv[j++] = * (HChar**) VG_(indexXA)( VG_(args_for_valgrind), i );
      }
      argv[j++] = (HChar*)ARG1;
      if (arg2copy && arg2copy[0])
         for (i = 1; arg2copy[i]; i++)
            argv[j++] = arg2copy[i];
      argv[j++] = NULL;
      
      vg_assert(j == tot_args+1);
   }

   
   VG_(setrlimit)(VKI_RLIMIT_DATA, &VG_(client_rlimit_data));

   {
      vki_sigset_t allsigs;
      vki_siginfo_t info;

      for (i = 1; i < VG_(max_signal); i++) {
         vki_sigaction_fromK_t sa_f;
         vki_sigaction_toK_t   sa_t;
         VG_(do_sys_sigaction)(i, NULL, &sa_f);
         VG_(convert_sigaction_fromK_to_toK)(&sa_f, &sa_t);
         if (sa_t.ksa_handler == VKI_SIG_IGN)
            VG_(sigaction)(i, &sa_t, NULL);
         else {
            sa_t.ksa_handler = VKI_SIG_DFL;
            VG_(sigaction)(i, &sa_t, NULL);
         }
      }

      VG_(sigfillset)(&allsigs);
      while(VG_(sigtimedwait_zero)(&allsigs, &info) > 0)
         ;

      VG_(sigprocmask)(VKI_SIG_SETMASK, &tst->sig_mask, NULL);
   }

   if (0) {
      HChar **cpp;
      VG_(printf)("exec: %s\n", path);
      for (cpp = argv; cpp && *cpp; cpp++)
         VG_(printf)("argv: %s\n", *cpp);
      if (0)
         for (cpp = envp; cpp && *cpp; cpp++)
            VG_(printf)("env: %s\n", *cpp);
   }

   SET_STATUS_from_SysRes( 
      VG_(do_syscall3)(__NR_execve, (UWord)path, (UWord)argv, (UWord)envp) 
   );

  hosed:
   vg_assert(FAILURE);
   VG_(message)(Vg_UserMsg, "execve(%#lx(%s), %#lx, %#lx) failed, errno %ld\n",
                ARG1, (char*)ARG1, ARG2, ARG3, ERR);
   VG_(message)(Vg_UserMsg, "EXEC FAILED: I can't recover from "
                            "execve() failing, so I'm dying.\n");
   VG_(message)(Vg_UserMsg, "Add more stringent tests in PRE(sys_execve), "
                            "or work out how to recover.\n");
   VG_(exit)(101);
}

PRE(sys_access)
{
   PRINT("sys_access ( %#lx(%s), %ld )", ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "access", const char *, pathname, int, mode);
   PRE_MEM_RASCIIZ( "access(pathname)", ARG1 );
}

PRE(sys_alarm)
{
   PRINT("sys_alarm ( %ld )", ARG1);
   PRE_REG_READ1(unsigned long, "alarm", unsigned int, seconds);
}

PRE(sys_brk)
{
   Addr brk_limit = VG_(brk_limit);
   Addr brk_new; 

   PRINT("sys_brk ( %#lx )", ARG1);
   PRE_REG_READ1(unsigned long, "brk", unsigned long, end_data_segment);

   brk_new = do_brk(ARG1, tid);
   SET_STATUS_Success( brk_new );

   if (brk_new == ARG1) {
      
      if (brk_new < brk_limit) {
         
         VG_TRACK( die_mem_brk, (Addr)ARG1,
		   brk_limit-ARG1 );
      } else
      if (brk_new > brk_limit) {
         
         VG_TRACK( new_mem_brk, brk_limit,
                   ARG1-brk_limit, tid );
      }
   } else {
      
      vg_assert(brk_limit == brk_new);
   }
}

PRE(sys_chdir)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_chdir ( %#lx(%s) )", ARG1,(char*)ARG1);
   PRE_REG_READ1(long, "chdir", const char *, path);
   PRE_MEM_RASCIIZ( "chdir(path)", ARG1 );
}

PRE(sys_chmod)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_chmod ( %#lx(%s), %ld )", ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "chmod", const char *, path, vki_mode_t, mode);
   PRE_MEM_RASCIIZ( "chmod(path)", ARG1 );
}

PRE(sys_chown)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_chown ( %#lx(%s), 0x%lx, 0x%lx )", ARG1,(char*)ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "chown",
                 const char *, path, vki_uid_t, owner, vki_gid_t, group);
   PRE_MEM_RASCIIZ( "chown(path)", ARG1 );
}

PRE(sys_lchown)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_lchown ( %#lx(%s), 0x%lx, 0x%lx )", ARG1,(char*)ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "lchown",
                 const char *, path, vki_uid_t, owner, vki_gid_t, group);
   PRE_MEM_RASCIIZ( "lchown(path)", ARG1 );
}

PRE(sys_close)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_close ( %ld )", ARG1);
   PRE_REG_READ1(long, "close", unsigned int, fd);

   
   if ( (!ML_(fd_allowed)(ARG1, "close", tid, False))
        || (ARG1 == 2 && VG_(debugLog_getLevel)() > 0) )
      SET_STATUS_Failure( VKI_EBADF );
}

POST(sys_close)
{
   if (VG_(clo_track_fds)) record_fd_close(ARG1);
}

PRE(sys_dup)
{
   PRINT("sys_dup ( %ld )", ARG1);
   PRE_REG_READ1(long, "dup", unsigned int, oldfd);
}

POST(sys_dup)
{
   vg_assert(SUCCESS);
   if (!ML_(fd_allowed)(RES, "dup", tid, True)) {
      VG_(close)(RES);
      SET_STATUS_Failure( VKI_EMFILE );
   } else {
      if (VG_(clo_track_fds))
         ML_(record_fd_open_named)(tid, RES);
   }
}

PRE(sys_dup2)
{
   PRINT("sys_dup2 ( %ld, %ld )", ARG1,ARG2);
   PRE_REG_READ2(long, "dup2", unsigned int, oldfd, unsigned int, newfd);
   if (!ML_(fd_allowed)(ARG2, "dup2", tid, True))
      SET_STATUS_Failure( VKI_EBADF );
}

POST(sys_dup2)
{
   vg_assert(SUCCESS);
   if (VG_(clo_track_fds))
      ML_(record_fd_open_named)(tid, RES);
}

PRE(sys_fchdir)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_fchdir ( %ld )", ARG1);
   PRE_REG_READ1(long, "fchdir", unsigned int, fd);
}

PRE(sys_fchown)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_fchown ( %ld, %ld, %ld )", ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "fchown",
                 unsigned int, fd, vki_uid_t, owner, vki_gid_t, group);
}

PRE(sys_fchmod)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_fchmod ( %ld, %ld )", ARG1,ARG2);
   PRE_REG_READ2(long, "fchmod", unsigned int, fildes, vki_mode_t, mode);
}

PRE(sys_newfstat)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_newfstat ( %ld, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(long, "fstat", unsigned int, fd, struct stat *, buf);
   PRE_MEM_WRITE( "fstat(buf)", ARG2, sizeof(struct vki_stat) );
}

POST(sys_newfstat)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat) );
}

static vki_sigset_t fork_saved_mask;

PRE(sys_fork)
{
   Bool is_child;
   Int child_pid;
   vki_sigset_t mask;

   PRINT("sys_fork ( )");
   PRE_REG_READ0(long, "fork");

   VG_(sigfillset)(&mask);
   VG_(sigprocmask)(VKI_SIG_SETMASK, &mask, &fork_saved_mask);

   SET_STATUS_from_SysRes( VG_(do_syscall0)(__NR_fork) );

   if (!SUCCESS) return;

#if defined(VGO_linux)
   
   is_child = ( RES == 0 ? True : False );
   child_pid = ( is_child ? -1 : RES );
#elif defined(VGO_darwin)
   
   is_child = RESHI;
   child_pid = RES;
#else
#  error Unknown OS
#endif

   VG_(do_atfork_pre)(tid);

   if (is_child) {
      VG_(do_atfork_child)(tid);

      
      VG_(sigprocmask)(VKI_SIG_SETMASK, &fork_saved_mask, NULL);

      if (VG_(clo_child_silent_after_fork)) {
         if (!VG_(log_output_sink).is_socket)
            VG_(log_output_sink).fd = -1;
         if (!VG_(xml_output_sink).is_socket)
            VG_(xml_output_sink).fd = -1;
      }

   } else {
      VG_(do_atfork_parent)(tid);

      PRINT("   fork: process %d created child %d\n", VG_(getpid)(), child_pid);

      
      VG_(sigprocmask)(VKI_SIG_SETMASK, &fork_saved_mask, NULL);
   }
}

PRE(sys_ftruncate)
{
   *flags |= SfMayBlock;
   PRINT("sys_ftruncate ( %ld, %ld )", ARG1,ARG2);
   PRE_REG_READ2(long, "ftruncate", unsigned int, fd, unsigned long, length);
}

PRE(sys_truncate)
{
   *flags |= SfMayBlock;
   PRINT("sys_truncate ( %#lx(%s), %ld )", ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "truncate", 
                 const char *, path, unsigned long, length);
   PRE_MEM_RASCIIZ( "truncate(path)", ARG1 );
}

PRE(sys_ftruncate64)
{
   *flags |= SfMayBlock;
#if VG_WORDSIZE == 4
   PRINT("sys_ftruncate64 ( %ld, %lld )", ARG1, MERGE64(ARG2,ARG3));
   PRE_REG_READ3(long, "ftruncate64",
                 unsigned int, fd,
                 UWord, MERGE64_FIRST(length), UWord, MERGE64_SECOND(length));
#else
   PRINT("sys_ftruncate64 ( %ld, %lld )", ARG1, (Long)ARG2);
   PRE_REG_READ2(long, "ftruncate64",
                 unsigned int,fd, UWord,length);
#endif
}

PRE(sys_truncate64)
{
   *flags |= SfMayBlock;
#if VG_WORDSIZE == 4
   PRINT("sys_truncate64 ( %#lx, %lld )", ARG1, (Long)MERGE64(ARG2, ARG3));
   PRE_REG_READ3(long, "truncate64",
                 const char *, path,
                 UWord, MERGE64_FIRST(length), UWord, MERGE64_SECOND(length));
#else
   PRINT("sys_truncate64 ( %#lx, %lld )", ARG1, (Long)ARG2);
   PRE_REG_READ2(long, "truncate64",
                 const char *,path, UWord,length);
#endif
   PRE_MEM_RASCIIZ( "truncate64(path)", ARG1 );
}

PRE(sys_getdents)
{
   *flags |= SfMayBlock;
   PRINT("sys_getdents ( %ld, %#lx, %ld )", ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "getdents",
                 unsigned int, fd, struct vki_dirent *, dirp,
                 unsigned int, count);
   PRE_MEM_WRITE( "getdents(dirp)", ARG2, ARG3 );
}

POST(sys_getdents)
{
   vg_assert(SUCCESS);
   if (RES > 0)
      POST_MEM_WRITE( ARG2, RES );
}

PRE(sys_getdents64)
{
   *flags |= SfMayBlock;
   PRINT("sys_getdents64 ( %ld, %#lx, %ld )",ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "getdents64",
                 unsigned int, fd, struct vki_dirent64 *, dirp,
                 unsigned int, count);
   PRE_MEM_WRITE( "getdents64(dirp)", ARG2, ARG3 );
}

POST(sys_getdents64)
{
   vg_assert(SUCCESS);
   if (RES > 0)
      POST_MEM_WRITE( ARG2, RES );
}

PRE(sys_getgroups)
{
   PRINT("sys_getgroups ( %ld, %#lx )", ARG1, ARG2);
   PRE_REG_READ2(long, "getgroups", int, size, vki_gid_t *, list);
   if (ARG1 > 0)
      PRE_MEM_WRITE( "getgroups(list)", ARG2, ARG1 * sizeof(vki_gid_t) );
}

POST(sys_getgroups)
{
   vg_assert(SUCCESS);
   if (ARG1 > 0 && RES > 0)
      POST_MEM_WRITE( ARG2, RES * sizeof(vki_gid_t) );
}

PRE(sys_getcwd)
{
   
   
   
   
   
   
   PRINT("sys_getcwd ( %#lx, %llu )", ARG1,(ULong)ARG2);
   PRE_REG_READ2(long, "getcwd", char *, buf, unsigned long, size);
   PRE_MEM_WRITE( "getcwd(buf)", ARG1, ARG2 );
}

POST(sys_getcwd)
{
   vg_assert(SUCCESS);
   if (RES != (Addr)NULL)
      POST_MEM_WRITE( ARG1, RES );
}

PRE(sys_geteuid)
{
   PRINT("sys_geteuid ( )");
   PRE_REG_READ0(long, "geteuid");
}

PRE(sys_getegid)
{
   PRINT("sys_getegid ( )");
   PRE_REG_READ0(long, "getegid");
}

PRE(sys_getgid)
{
   PRINT("sys_getgid ( )");
   PRE_REG_READ0(long, "getgid");
}

PRE(sys_getpid)
{
   PRINT("sys_getpid ()");
   PRE_REG_READ0(long, "getpid");
}

PRE(sys_getpgid)
{
   PRINT("sys_getpgid ( %ld )", ARG1);
   PRE_REG_READ1(long, "getpgid", vki_pid_t, pid);
}

PRE(sys_getpgrp)
{
   PRINT("sys_getpgrp ()");
   PRE_REG_READ0(long, "getpgrp");
}

PRE(sys_getppid)
{
   PRINT("sys_getppid ()");
   PRE_REG_READ0(long, "getppid");
}

static void common_post_getrlimit(ThreadId tid, UWord a1, UWord a2)
{
   POST_MEM_WRITE( a2, sizeof(struct vki_rlimit) );

#ifdef _RLIMIT_POSIX_FLAG
   
   
   a1 &= ~_RLIMIT_POSIX_FLAG;
#endif

   switch (a1) {
   case VKI_RLIMIT_NOFILE:
      ((struct vki_rlimit *)a2)->rlim_cur = VG_(fd_soft_limit);
      ((struct vki_rlimit *)a2)->rlim_max = VG_(fd_hard_limit);
      break;

   case VKI_RLIMIT_DATA:
      *((struct vki_rlimit *)a2) = VG_(client_rlimit_data);
      break;

   case VKI_RLIMIT_STACK:
      *((struct vki_rlimit *)a2) = VG_(client_rlimit_stack);
      break;
   }
}

PRE(sys_old_getrlimit)
{
   PRINT("sys_old_getrlimit ( %ld, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(long, "old_getrlimit",
                 unsigned int, resource, struct rlimit *, rlim);
   PRE_MEM_WRITE( "old_getrlimit(rlim)", ARG2, sizeof(struct vki_rlimit) );
}

POST(sys_old_getrlimit)
{
   common_post_getrlimit(tid, ARG1, ARG2);
}

PRE(sys_getrlimit)
{
   PRINT("sys_getrlimit ( %ld, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(long, "getrlimit",
                 unsigned int, resource, struct rlimit *, rlim);
   PRE_MEM_WRITE( "getrlimit(rlim)", ARG2, sizeof(struct vki_rlimit) );
}

POST(sys_getrlimit)
{
   common_post_getrlimit(tid, ARG1, ARG2);
}

PRE(sys_getrusage)
{
   PRINT("sys_getrusage ( %ld, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(long, "getrusage", int, who, struct rusage *, usage);
   PRE_MEM_WRITE( "getrusage(usage)", ARG2, sizeof(struct vki_rusage) );
}

POST(sys_getrusage)
{
   vg_assert(SUCCESS);
   if (RES == 0)
      POST_MEM_WRITE( ARG2, sizeof(struct vki_rusage) );
}

PRE(sys_gettimeofday)
{
   PRINT("sys_gettimeofday ( %#lx, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(long, "gettimeofday",
                 struct timeval *, tv, struct timezone *, tz);
   
   if (ARG1 != 0)
      PRE_timeval_WRITE( "gettimeofday(tv)", ARG1 );
   if (ARG2 != 0)
      PRE_MEM_WRITE( "gettimeofday(tz)", ARG2, sizeof(struct vki_timezone) );
}

POST(sys_gettimeofday)
{
   vg_assert(SUCCESS);
   if (RES == 0) {
      if (ARG1 != 0)
         POST_timeval_WRITE( ARG1 );
      if (ARG2 != 0)
	 POST_MEM_WRITE( ARG2, sizeof(struct vki_timezone) );
   }
}

PRE(sys_settimeofday)
{
   PRINT("sys_settimeofday ( %#lx, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(long, "settimeofday",
                 struct timeval *, tv, struct timezone *, tz);
   if (ARG1 != 0)
      PRE_timeval_READ( "settimeofday(tv)", ARG1 );
   if (ARG2 != 0) {
      PRE_MEM_READ( "settimeofday(tz)", ARG2, sizeof(struct vki_timezone) );
      
   }
}

PRE(sys_getuid)
{
   PRINT("sys_getuid ( )");
   PRE_REG_READ0(long, "getuid");
}

void ML_(PRE_unknown_ioctl)(ThreadId tid, UWord request, UWord arg)
{         
   
   UInt dir  = _VKI_IOC_DIR(request);
   UInt size = _VKI_IOC_SIZE(request);
   if (SimHintiS(SimHint_lax_ioctls, VG_(clo_sim_hints))) {
   } else if ( dir == _VKI_IOC_NONE) {
      static UWord unknown_ioctl[10];
      static Int moans = sizeof(unknown_ioctl) / sizeof(unknown_ioctl[0]);

      if (moans > 0 && !VG_(clo_xml)) {
         
         UInt i;
         for (i = 0; i < sizeof(unknown_ioctl)/sizeof(unknown_ioctl[0]); i++) {
            if (unknown_ioctl[i] == request)
               break;
            if (unknown_ioctl[i] == 0) {
               unknown_ioctl[i] = request;
               moans--;
               VG_(umsg)("Warning: noted but unhandled ioctl 0x%lx"
                         " with no size/direction hints.\n", request); 
               VG_(umsg)("   This could cause spurious value errors to appear.\n");
               VG_(umsg)("   See README_MISSING_SYSCALL_OR_IOCTL for "
                         "guidance on writing a proper wrapper.\n" );
               
               return;
            }
         }
      }
   } else {
      
      
      if ((dir & _VKI_IOC_WRITE) && size > 0)
         PRE_MEM_READ( "ioctl(generic)", arg, size);
      if ((dir & _VKI_IOC_READ) && size > 0)
         PRE_MEM_WRITE( "ioctl(generic)", arg, size);
   }
}

void ML_(POST_unknown_ioctl)(ThreadId tid, UInt res, UWord request, UWord arg)
{
   
   UInt dir  = _VKI_IOC_DIR(request);
   UInt size = _VKI_IOC_SIZE(request);
   if (size > 0 && (dir & _VKI_IOC_READ)
       && res == 0 
       && arg != (Addr)NULL)
   {
      POST_MEM_WRITE(arg, size);
   }
}

Bool ML_(do_sigkill)(Int pid, Int tgid)
{
   ThreadState *tst;
   ThreadId tid;

   if (pid <= 0)
      return False;

   tid = VG_(lwpid_to_vgtid)(pid);
   if (tid == VG_INVALID_THREADID)
      return False;		

   tst = VG_(get_ThreadState)(tid);
   if (tst == NULL || tst->status == VgTs_Empty)
      return False;		

   if (tgid != -1 && tst->os_state.threadgroup != tgid)
      return False;		

   
   if (!VG_(is_exiting)(tid)) {
      if (VG_(clo_trace_signals))
	 VG_(message)(Vg_DebugMsg,
                      "Thread %d being killed with SIGKILL\n", 
                      tst->tid);
      
      tst->exitreason = VgSrc_FatalSig;
      tst->os_state.fatalsig = VKI_SIGKILL;
      
      if (!VG_(is_running_thread)(tid))
	 VG_(get_thread_out_of_syscall)(tid);
   }
   
   return True;
}

PRE(sys_kill)
{
   PRINT("sys_kill ( %ld, %ld )", ARG1,ARG2);
   PRE_REG_READ2(long, "kill", int, pid, int, sig);
   if (!ML_(client_signal_OK)(ARG2)) {
      SET_STATUS_Failure( VKI_EINVAL );
      return;
   }

   if (ARG2 == VKI_SIGKILL && ML_(do_sigkill)(ARG1, -1))
      SET_STATUS_Success(0);
   else
      SET_STATUS_from_SysRes( VG_(do_syscall3)(SYSNO, ARG1, ARG2, ARG3) );

   if (VG_(clo_trace_signals))
      VG_(message)(Vg_DebugMsg, "kill: sent signal %ld to pid %ld\n",
		   ARG2, ARG1);

   *flags |= SfPollAfter;
}

PRE(sys_link)
{
   *flags |= SfMayBlock;
   PRINT("sys_link ( %#lx(%s), %#lx(%s) )", ARG1,(char*)ARG1,ARG2,(char*)ARG2);
   PRE_REG_READ2(long, "link", const char *, oldpath, const char *, newpath);
   PRE_MEM_RASCIIZ( "link(oldpath)", ARG1);
   PRE_MEM_RASCIIZ( "link(newpath)", ARG2);
}

PRE(sys_newlstat)
{
   PRINT("sys_newlstat ( %#lx(%s), %#lx )", ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "lstat", char *, file_name, struct stat *, buf);
   PRE_MEM_RASCIIZ( "lstat(file_name)", ARG1 );
   PRE_MEM_WRITE( "lstat(buf)", ARG2, sizeof(struct vki_stat) );
}

POST(sys_newlstat)
{
   vg_assert(SUCCESS);
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat) );
}

PRE(sys_mkdir)
{
   *flags |= SfMayBlock;
   PRINT("sys_mkdir ( %#lx(%s), %ld )", ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "mkdir", const char *, pathname, int, mode);
   PRE_MEM_RASCIIZ( "mkdir(pathname)", ARG1 );
}

PRE(sys_mprotect)
{
   PRINT("sys_mprotect ( %#lx, %llu, %ld )", ARG1,(ULong)ARG2,ARG3);
   PRE_REG_READ3(long, "mprotect",
                 unsigned long, addr, vki_size_t, len, unsigned long, prot);

   if (!ML_(valid_client_addr)(ARG1, ARG2, tid, "mprotect")) {
      SET_STATUS_Failure( VKI_ENOMEM );
   } 
#if defined(VKI_PROT_GROWSDOWN)
   else 
   if (ARG3 & (VKI_PROT_GROWSDOWN|VKI_PROT_GROWSUP)) {
      UInt grows = ARG3 & (VKI_PROT_GROWSDOWN|VKI_PROT_GROWSUP);
      NSegment const *aseg = VG_(am_find_nsegment)(ARG1);
      NSegment const *rseg;

      vg_assert(aseg);

      if (grows == VKI_PROT_GROWSDOWN) {
         rseg = VG_(am_next_nsegment)( aseg, False );
         if (rseg &&
             rseg->kind == SkResvn &&
             rseg->smode == SmUpper &&
             rseg->end+1 == aseg->start) {
            Addr end = ARG1 + ARG2;
            ARG1 = aseg->start;
            ARG2 = end - aseg->start;
            ARG3 &= ~VKI_PROT_GROWSDOWN;
         } else {
            SET_STATUS_Failure( VKI_EINVAL );
         }
      } else if (grows == VKI_PROT_GROWSUP) {
         rseg = VG_(am_next_nsegment)( aseg, True );
         if (rseg &&
             rseg->kind == SkResvn &&
             rseg->smode == SmLower &&
             aseg->end+1 == rseg->start) {
            ARG2 = aseg->end - ARG1 + 1;
            ARG3 &= ~VKI_PROT_GROWSUP;
         } else {
            SET_STATUS_Failure( VKI_EINVAL );
         }
      } else {
         
         SET_STATUS_Failure( VKI_EINVAL );
      }
   }
#endif   
}

POST(sys_mprotect)
{
   Addr a    = ARG1;
   SizeT len = ARG2;
   Int  prot = ARG3;

   ML_(notify_core_and_tool_of_mprotect)(a, len, prot);
}

PRE(sys_munmap)
{
   if (0) VG_(printf)("  munmap( %#lx )\n", ARG1);
   PRINT("sys_munmap ( %#lx, %llu )", ARG1,(ULong)ARG2);
   PRE_REG_READ2(long, "munmap", unsigned long, start, vki_size_t, length);

   if (!ML_(valid_client_addr)(ARG1, ARG2, tid, "munmap"))
      SET_STATUS_Failure( VKI_EINVAL );
}

POST(sys_munmap)
{
   Addr  a   = ARG1;
   SizeT len = ARG2;

   ML_(notify_core_and_tool_of_munmap)( a, len );
}

PRE(sys_mincore)
{
   PRINT("sys_mincore ( %#lx, %llu, %#lx )", ARG1,(ULong)ARG2,ARG3);
   PRE_REG_READ3(long, "mincore",
                 unsigned long, start, vki_size_t, length,
                 unsigned char *, vec);
   PRE_MEM_WRITE( "mincore(vec)", ARG3, VG_PGROUNDUP(ARG2) / VKI_PAGE_SIZE );
}
POST(sys_mincore)
{
   POST_MEM_WRITE( ARG3, VG_PGROUNDUP(ARG2) / VKI_PAGE_SIZE );  
}

PRE(sys_nanosleep)
{
   *flags |= SfMayBlock|SfPostOnFail;
   PRINT("sys_nanosleep ( %#lx, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(long, "nanosleep", 
                 struct timespec *, req, struct timespec *, rem);
   PRE_MEM_READ( "nanosleep(req)", ARG1, sizeof(struct vki_timespec) );
   if (ARG2 != 0)
      PRE_MEM_WRITE( "nanosleep(rem)", ARG2, sizeof(struct vki_timespec) );
}

POST(sys_nanosleep)
{
   vg_assert(SUCCESS || FAILURE);
   if (ARG2 != 0 && FAILURE && ERR == VKI_EINTR)
      POST_MEM_WRITE( ARG2, sizeof(struct vki_timespec) );
}

PRE(sys_open)
{
   if (ARG2 & VKI_O_CREAT) {
      
      PRINT("sys_open ( %#lx(%s), %ld, %ld )",ARG1,(char*)ARG1,ARG2,ARG3);
      PRE_REG_READ3(long, "open",
                    const char *, filename, int, flags, int, mode);
   } else {
      
      PRINT("sys_open ( %#lx(%s), %ld )",ARG1,(char*)ARG1,ARG2);
      PRE_REG_READ2(long, "open",
                    const char *, filename, int, flags);
   }
   PRE_MEM_RASCIIZ( "open(filename)", ARG1 );

#if defined(VGO_linux)
   {
      HChar  name[30];   
      HChar* arg1s = (HChar*) ARG1;
      SysRes sres;

      VG_(sprintf)(name, "/proc/%d/cmdline", VG_(getpid)());
      if (ML_(safe_to_deref)( arg1s, 1 ) &&
          (VG_STREQ(arg1s, name) || VG_STREQ(arg1s, "/proc/self/cmdline"))
         )
      {
         sres = VG_(dup)( VG_(cl_cmdline_fd) );
         SET_STATUS_from_SysRes( sres );
         if (!sr_isError(sres)) {
            OffT off = VG_(lseek)( sr_Res(sres), 0, VKI_SEEK_SET );
            if (off < 0)
               SET_STATUS_Failure( VKI_EMFILE );
         }
         return;
      }
   }

   {
      HChar  name[30];   
      HChar* arg1s = (HChar*) ARG1;
      SysRes sres;

      VG_(sprintf)(name, "/proc/%d/auxv", VG_(getpid)());
      if (ML_(safe_to_deref)( arg1s, 1 ) &&
          (VG_STREQ(arg1s, name) || VG_STREQ(arg1s, "/proc/self/auxv"))
         )
      {
         sres = VG_(dup)( VG_(cl_auxv_fd) );
         SET_STATUS_from_SysRes( sres );
         if (!sr_isError(sres)) {
            OffT off = VG_(lseek)( sr_Res(sres), 0, VKI_SEEK_SET );
            if (off < 0)
               SET_STATUS_Failure( VKI_EMFILE );
         }
         return;
      }
   }
#endif 

   
   *flags |= SfMayBlock;
}

POST(sys_open)
{
   vg_assert(SUCCESS);
   if (!ML_(fd_allowed)(RES, "open", tid, True)) {
      VG_(close)(RES);
      SET_STATUS_Failure( VKI_EMFILE );
   } else {
      if (VG_(clo_track_fds))
         ML_(record_fd_open_with_given_name)(tid, RES, (HChar*)ARG1);
   }
}

PRE(sys_read)
{
   *flags |= SfMayBlock;
   PRINT("sys_read ( %ld, %#lx, %llu )", ARG1, ARG2, (ULong)ARG3);
   PRE_REG_READ3(ssize_t, "read",
                 unsigned int, fd, char *, buf, vki_size_t, count);

   if (!ML_(fd_allowed)(ARG1, "read", tid, False))
      SET_STATUS_Failure( VKI_EBADF );
   else
      PRE_MEM_WRITE( "read(buf)", ARG2, ARG3 );
}

POST(sys_read)
{
   vg_assert(SUCCESS);
   POST_MEM_WRITE( ARG2, RES );
}

PRE(sys_write)
{
   Bool ok;
   *flags |= SfMayBlock;
   PRINT("sys_write ( %ld, %#lx, %llu )", ARG1, ARG2, (ULong)ARG3);
   PRE_REG_READ3(ssize_t, "write",
                 unsigned int, fd, const char *, buf, vki_size_t, count);
   ok = ML_(fd_allowed)(ARG1, "write", tid, False);
   if (!ok && ARG1 == 2 
           && SimHintiS(SimHint_enable_outer, VG_(clo_sim_hints)))
      ok = True;
   if (!ok)
      SET_STATUS_Failure( VKI_EBADF );
   else
      PRE_MEM_READ( "write(buf)", ARG2, ARG3 );
}

PRE(sys_creat)
{
   *flags |= SfMayBlock;
   PRINT("sys_creat ( %#lx(%s), %ld )", ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "creat", const char *, pathname, int, mode);
   PRE_MEM_RASCIIZ( "creat(pathname)", ARG1 );
}

POST(sys_creat)
{
   vg_assert(SUCCESS);
   if (!ML_(fd_allowed)(RES, "creat", tid, True)) {
      VG_(close)(RES);
      SET_STATUS_Failure( VKI_EMFILE );
   } else {
      if (VG_(clo_track_fds))
         ML_(record_fd_open_with_given_name)(tid, RES, (HChar*)ARG1);
   }
}

PRE(sys_poll)
{
   UInt i;
   struct vki_pollfd* ufds = (struct vki_pollfd *)ARG1;
   *flags |= SfMayBlock;
   PRINT("sys_poll ( %#lx, %ld, %ld )\n", ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "poll",
                 struct vki_pollfd *, ufds, unsigned int, nfds, long, timeout);

   for (i = 0; i < ARG2; i++) {
      PRE_MEM_READ( "poll(ufds.fd)",
                    (Addr)(&ufds[i].fd), sizeof(ufds[i].fd) );
      PRE_MEM_READ( "poll(ufds.events)",
                    (Addr)(&ufds[i].events), sizeof(ufds[i].events) );
      PRE_MEM_WRITE( "poll(ufds.revents)",
                     (Addr)(&ufds[i].revents), sizeof(ufds[i].revents) );
   }
}

POST(sys_poll)
{
   if (RES >= 0) {
      UInt i;
      struct vki_pollfd* ufds = (struct vki_pollfd *)ARG1;
      for (i = 0; i < ARG2; i++)
	 POST_MEM_WRITE( (Addr)(&ufds[i].revents), sizeof(ufds[i].revents) );
   }
}

PRE(sys_readlink)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   Word saved = SYSNO;

   PRINT("sys_readlink ( %#lx(%s), %#lx, %llu )", ARG1,(char*)ARG1,ARG2,(ULong)ARG3);
   PRE_REG_READ3(long, "readlink",
                 const char *, path, char *, buf, int, bufsiz);
   PRE_MEM_RASCIIZ( "readlink(path)", ARG1 );
   PRE_MEM_WRITE( "readlink(buf)", ARG2,ARG3 );

   {
#if defined(VGO_linux)
      HChar  name[30];   
      HChar* arg1s = (HChar*) ARG1;
      VG_(sprintf)(name, "/proc/%d/exe", VG_(getpid)());
      if (ML_(safe_to_deref)(arg1s, 1) &&
          (VG_STREQ(arg1s, name) || VG_STREQ(arg1s, "/proc/self/exe"))
         )
      {
         VG_(sprintf)(name, "/proc/self/fd/%d", VG_(cl_exec_fd));
         SET_STATUS_from_SysRes( VG_(do_syscall3)(saved, (UWord)name, 
                                                         ARG2, ARG3));
      } else
#endif 
      {
         
         SET_STATUS_from_SysRes( VG_(do_syscall3)(saved, ARG1, ARG2, ARG3));
      }
   }

   if (SUCCESS && RES > 0)
      POST_MEM_WRITE( ARG2, RES );
}

PRE(sys_readv)
{
   Int i;
   struct vki_iovec * vec;
   *flags |= SfMayBlock;
   PRINT("sys_readv ( %ld, %#lx, %llu )",ARG1,ARG2,(ULong)ARG3);
   PRE_REG_READ3(ssize_t, "readv",
                 unsigned long, fd, const struct iovec *, vector,
                 unsigned long, count);
   if (!ML_(fd_allowed)(ARG1, "readv", tid, False)) {
      SET_STATUS_Failure( VKI_EBADF );
   } else {
      if ((Int)ARG3 >= 0)
         PRE_MEM_READ( "readv(vector)", ARG2, ARG3 * sizeof(struct vki_iovec) );

      if (ARG2 != 0) {
         
         vec = (struct vki_iovec *)ARG2;
         for (i = 0; i < (Int)ARG3; i++)
            PRE_MEM_WRITE( "readv(vector[...])",
                           (Addr)vec[i].iov_base, vec[i].iov_len );
      }
   }
}

POST(sys_readv)
{
   vg_assert(SUCCESS);
   if (RES > 0) {
      Int i;
      struct vki_iovec * vec = (struct vki_iovec *)ARG2;
      Int remains = RES;

      
      for (i = 0; i < (Int)ARG3; i++) {
	 Int nReadThisBuf = vec[i].iov_len;
	 if (nReadThisBuf > remains) nReadThisBuf = remains;
	 POST_MEM_WRITE( (Addr)vec[i].iov_base, nReadThisBuf );
	 remains -= nReadThisBuf;
	 if (remains < 0) VG_(core_panic)("readv: remains < 0");
      }
   }
}

PRE(sys_rename)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_rename ( %#lx(%s), %#lx(%s) )", ARG1,(char*)ARG1,ARG2,(char*)ARG2);
   PRE_REG_READ2(long, "rename", const char *, oldpath, const char *, newpath);
   PRE_MEM_RASCIIZ( "rename(oldpath)", ARG1 );
   PRE_MEM_RASCIIZ( "rename(newpath)", ARG2 );
}

PRE(sys_rmdir)
{
   *flags |= SfMayBlock;
   PRINT("sys_rmdir ( %#lx(%s) )", ARG1,(char*)ARG1);
   PRE_REG_READ1(long, "rmdir", const char *, pathname);
   PRE_MEM_RASCIIZ( "rmdir(pathname)", ARG1 );
}

PRE(sys_select)
{
   *flags |= SfMayBlock;
   PRINT("sys_select ( %ld, %#lx, %#lx, %#lx, %#lx )", ARG1,ARG2,ARG3,ARG4,ARG5);
   PRE_REG_READ5(long, "select",
                 int, n, vki_fd_set *, readfds, vki_fd_set *, writefds,
                 vki_fd_set *, exceptfds, struct vki_timeval *, timeout);
   
   if (ARG2 != 0)
      PRE_MEM_READ( "select(readfds)",   
		     ARG2, ARG1/8  );
   if (ARG3 != 0)
      PRE_MEM_READ( "select(writefds)",  
		     ARG3, ARG1/8  );
   if (ARG4 != 0)
      PRE_MEM_READ( "select(exceptfds)", 
		     ARG4, ARG1/8  );
   if (ARG5 != 0)
      PRE_timeval_READ( "select(timeout)", ARG5 );
}

PRE(sys_setgid)
{
   PRINT("sys_setgid ( %ld )", ARG1);
   PRE_REG_READ1(long, "setgid", vki_gid_t, gid);
}

PRE(sys_setsid)
{
   PRINT("sys_setsid ( )");
   PRE_REG_READ0(long, "setsid");
}

PRE(sys_setgroups)
{
   PRINT("setgroups ( %llu, %#lx )", (ULong)ARG1, ARG2);
   PRE_REG_READ2(long, "setgroups", int, size, vki_gid_t *, list);
   if (ARG1 > 0)
      PRE_MEM_READ( "setgroups(list)", ARG2, ARG1 * sizeof(vki_gid_t) );
}

PRE(sys_setpgid)
{
   PRINT("setpgid ( %ld, %ld )", ARG1, ARG2);
   PRE_REG_READ2(long, "setpgid", vki_pid_t, pid, vki_pid_t, pgid);
}

PRE(sys_setregid)
{
   PRINT("sys_setregid ( %ld, %ld )", ARG1, ARG2);
   PRE_REG_READ2(long, "setregid", vki_gid_t, rgid, vki_gid_t, egid);
}

PRE(sys_setreuid)
{
   PRINT("sys_setreuid ( 0x%lx, 0x%lx )", ARG1, ARG2);
   PRE_REG_READ2(long, "setreuid", vki_uid_t, ruid, vki_uid_t, euid);
}

PRE(sys_setrlimit)
{
   UWord arg1 = ARG1;
   PRINT("sys_setrlimit ( %ld, %#lx )", ARG1,ARG2);
   PRE_REG_READ2(long, "setrlimit",
                 unsigned int, resource, struct rlimit *, rlim);
   PRE_MEM_READ( "setrlimit(rlim)", ARG2, sizeof(struct vki_rlimit) );

#ifdef _RLIMIT_POSIX_FLAG
   
   
   arg1 &= ~_RLIMIT_POSIX_FLAG;
#endif

   if (!VG_(am_is_valid_for_client)(ARG2, sizeof(struct vki_rlimit), 
                                    VKI_PROT_READ)) {
      SET_STATUS_Failure( VKI_EFAULT );
   }
   else if (((struct vki_rlimit *)ARG2)->rlim_cur 
            > ((struct vki_rlimit *)ARG2)->rlim_max) {
      SET_STATUS_Failure( VKI_EINVAL );
   }
   else if (arg1 == VKI_RLIMIT_NOFILE) {
      if (((struct vki_rlimit *)ARG2)->rlim_cur > VG_(fd_hard_limit) ||
          ((struct vki_rlimit *)ARG2)->rlim_max != VG_(fd_hard_limit)) {
         SET_STATUS_Failure( VKI_EPERM );
      }
      else {
         VG_(fd_soft_limit) = ((struct vki_rlimit *)ARG2)->rlim_cur;
         SET_STATUS_Success( 0 );
      }
   }
   else if (arg1 == VKI_RLIMIT_DATA) {
      if (((struct vki_rlimit *)ARG2)->rlim_cur > VG_(client_rlimit_data).rlim_max ||
          ((struct vki_rlimit *)ARG2)->rlim_max > VG_(client_rlimit_data).rlim_max) {
         SET_STATUS_Failure( VKI_EPERM );
      }
      else {
         VG_(client_rlimit_data) = *(struct vki_rlimit *)ARG2;
         SET_STATUS_Success( 0 );
      }
   }
   else if (arg1 == VKI_RLIMIT_STACK && tid == 1) {
      if (((struct vki_rlimit *)ARG2)->rlim_cur > VG_(client_rlimit_stack).rlim_max ||
          ((struct vki_rlimit *)ARG2)->rlim_max > VG_(client_rlimit_stack).rlim_max) {
         SET_STATUS_Failure( VKI_EPERM );
      }
      else {
         VG_(threads)[tid].client_stack_szB = ((struct vki_rlimit *)ARG2)->rlim_cur;
         VG_(client_rlimit_stack) = *(struct vki_rlimit *)ARG2;
         SET_STATUS_Success( 0 );
      }
   }
}

PRE(sys_setuid)
{
   PRINT("sys_setuid ( %ld )", ARG1);
   PRE_REG_READ1(long, "setuid", vki_uid_t, uid);
}

PRE(sys_newstat)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_newstat ( %#lx(%s), %#lx )", ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "stat", char *, file_name, struct stat *, buf);
   PRE_MEM_RASCIIZ( "stat(file_name)", ARG1 );
   PRE_MEM_WRITE( "stat(buf)", ARG2, sizeof(struct vki_stat) );
}

POST(sys_newstat)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_stat) );
}

PRE(sys_statfs)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_statfs ( %#lx(%s), %#lx )",ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "statfs", const char *, path, struct statfs *, buf);
   PRE_MEM_RASCIIZ( "statfs(path)", ARG1 );
   PRE_MEM_WRITE( "statfs(buf)", ARG2, sizeof(struct vki_statfs) );
}
POST(sys_statfs)
{
   POST_MEM_WRITE( ARG2, sizeof(struct vki_statfs) );
}

PRE(sys_statfs64)
{
   PRINT("sys_statfs64 ( %#lx(%s), %llu, %#lx )",ARG1,(char*)ARG1,(ULong)ARG2,ARG3);
   PRE_REG_READ3(long, "statfs64",
                 const char *, path, vki_size_t, size, struct statfs64 *, buf);
   PRE_MEM_RASCIIZ( "statfs64(path)", ARG1 );
   PRE_MEM_WRITE( "statfs64(buf)", ARG3, ARG2 );
}
POST(sys_statfs64)
{
   POST_MEM_WRITE( ARG3, ARG2 );
}

PRE(sys_symlink)
{
   *flags |= SfMayBlock;
   PRINT("sys_symlink ( %#lx(%s), %#lx(%s) )",ARG1,(char*)ARG1,ARG2,(char*)ARG2);
   PRE_REG_READ2(long, "symlink", const char *, oldpath, const char *, newpath);
   PRE_MEM_RASCIIZ( "symlink(oldpath)", ARG1 );
   PRE_MEM_RASCIIZ( "symlink(newpath)", ARG2 );
}

PRE(sys_time)
{
   
   PRINT("sys_time ( %#lx )",ARG1);
   PRE_REG_READ1(long, "time", int *, t);
   if (ARG1 != 0) {
      PRE_MEM_WRITE( "time(t)", ARG1, sizeof(vki_time_t) );
   }
}

POST(sys_time)
{
   if (ARG1 != 0) {
      POST_MEM_WRITE( ARG1, sizeof(vki_time_t) );
   }
}

PRE(sys_times)
{
   PRINT("sys_times ( %#lx )", ARG1);
   PRE_REG_READ1(long, "times", struct tms *, buf);
   if (ARG1 != 0) {
      PRE_MEM_WRITE( "times(buf)", ARG1, sizeof(struct vki_tms) );
   }
}

POST(sys_times)
{
   if (ARG1 != 0) {
      POST_MEM_WRITE( ARG1, sizeof(struct vki_tms) );
   }
}

PRE(sys_umask)
{
   PRINT("sys_umask ( %ld )", ARG1);
   PRE_REG_READ1(long, "umask", int, mask);
}

PRE(sys_unlink)
{
   *flags |= SfMayBlock;
   PRINT("sys_unlink ( %#lx(%s) )", ARG1,(char*)ARG1);
   PRE_REG_READ1(long, "unlink", const char *, pathname);
   PRE_MEM_RASCIIZ( "unlink(pathname)", ARG1 );
}

PRE(sys_newuname)
{
   PRINT("sys_newuname ( %#lx )", ARG1);
   PRE_REG_READ1(long, "uname", struct new_utsname *, buf);
   PRE_MEM_WRITE( "uname(buf)", ARG1, sizeof(struct vki_new_utsname) );
}

POST(sys_newuname)
{
   if (ARG1 != 0) {
      POST_MEM_WRITE( ARG1, sizeof(struct vki_new_utsname) );
   }
}

PRE(sys_waitpid)
{
   *flags |= SfMayBlock;
   PRINT("sys_waitpid ( %ld, %#lx, %ld )", ARG1,ARG2,ARG3);
   PRE_REG_READ3(long, "waitpid", 
                 vki_pid_t, pid, unsigned int *, status, int, options);

   if (ARG2 != (Addr)NULL)
      PRE_MEM_WRITE( "waitpid(status)", ARG2, sizeof(int) );
}

POST(sys_waitpid)
{
   if (ARG2 != (Addr)NULL)
      POST_MEM_WRITE( ARG2, sizeof(int) );
}

PRE(sys_wait4)
{
   *flags |= SfMayBlock;
   PRINT("sys_wait4 ( %ld, %#lx, %ld, %#lx )", ARG1,ARG2,ARG3,ARG4);

   PRE_REG_READ4(long, "wait4", 
                 vki_pid_t, pid, unsigned int *, status, int, options,
                 struct rusage *, rusage);
   if (ARG2 != (Addr)NULL)
      PRE_MEM_WRITE( "wait4(status)", ARG2, sizeof(int) );
   if (ARG4 != (Addr)NULL)
      PRE_MEM_WRITE( "wait4(rusage)", ARG4, sizeof(struct vki_rusage) );
}

POST(sys_wait4)
{
   if (ARG2 != (Addr)NULL)
      POST_MEM_WRITE( ARG2, sizeof(int) );
   if (ARG4 != (Addr)NULL)
      POST_MEM_WRITE( ARG4, sizeof(struct vki_rusage) );
}

PRE(sys_writev)
{
   Int i;
   struct vki_iovec * vec;
   *flags |= SfMayBlock;
   PRINT("sys_writev ( %ld, %#lx, %llu )",ARG1,ARG2,(ULong)ARG3);
   PRE_REG_READ3(ssize_t, "writev",
                 unsigned long, fd, const struct iovec *, vector,
                 unsigned long, count);
   if (!ML_(fd_allowed)(ARG1, "writev", tid, False)) {
      SET_STATUS_Failure( VKI_EBADF );
   } else {
      if ((Int)ARG3 >= 0)
         PRE_MEM_READ( "writev(vector)", 
                       ARG2, ARG3 * sizeof(struct vki_iovec) );
      if (ARG2 != 0) {
         
         vec = (struct vki_iovec *)ARG2;
         for (i = 0; i < (Int)ARG3; i++)
            PRE_MEM_READ( "writev(vector[...])",
                           (Addr)vec[i].iov_base, vec[i].iov_len );
      }
   }
}

PRE(sys_utimes)
{
   FUSE_COMPATIBLE_MAY_BLOCK();
   PRINT("sys_utimes ( %#lx(%s), %#lx )", ARG1,(char*)ARG1,ARG2);
   PRE_REG_READ2(long, "utimes", char *, filename, struct timeval *, tvp);
   PRE_MEM_RASCIIZ( "utimes(filename)", ARG1 );
   if (ARG2 != 0) {
      PRE_timeval_READ( "utimes(tvp[0])", ARG2 );
      PRE_timeval_READ( "utimes(tvp[1])", ARG2+sizeof(struct vki_timeval) );
   }
}

PRE(sys_acct)
{
   PRINT("sys_acct ( %#lx(%s) )", ARG1,(char*)ARG1);
   PRE_REG_READ1(long, "acct", const char *, filename);
   PRE_MEM_RASCIIZ( "acct(filename)", ARG1 );
}

PRE(sys_pause)
{
   *flags |= SfMayBlock;
   PRINT("sys_pause ( )");
   PRE_REG_READ0(long, "pause");
}

PRE(sys_sigaltstack)
{
   PRINT("sigaltstack ( %#lx, %#lx )",ARG1,ARG2);
   PRE_REG_READ2(int, "sigaltstack",
                 const vki_stack_t *, ss, vki_stack_t *, oss);
   if (ARG1 != 0) {
      const vki_stack_t *ss = (vki_stack_t *)ARG1;
      PRE_MEM_READ( "sigaltstack(ss)", (Addr)&ss->ss_sp, sizeof(ss->ss_sp) );
      PRE_MEM_READ( "sigaltstack(ss)", (Addr)&ss->ss_flags, sizeof(ss->ss_flags) );
      PRE_MEM_READ( "sigaltstack(ss)", (Addr)&ss->ss_size, sizeof(ss->ss_size) );
   }
   if (ARG2 != 0) {
      PRE_MEM_WRITE( "sigaltstack(oss)", ARG2, sizeof(vki_stack_t) );
   }

   SET_STATUS_from_SysRes( 
      VG_(do_sys_sigaltstack) (tid, (vki_stack_t*)ARG1, 
                              (vki_stack_t*)ARG2)
   );
}
POST(sys_sigaltstack)
{
   vg_assert(SUCCESS);
   if (RES == 0 && ARG2 != 0)
      POST_MEM_WRITE( ARG2, sizeof(vki_stack_t));
}

PRE(sys_sethostname)
{
   PRINT("sys_sethostname ( %#lx, %ld )", ARG1,ARG2);
   PRE_REG_READ2(long, "sethostname", char *, name, int, len);
   PRE_MEM_READ( "sethostname(name)", ARG1, ARG2 );
}

#undef PRE
#undef POST

#endif 


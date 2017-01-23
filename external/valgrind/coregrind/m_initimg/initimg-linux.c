

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

#if defined(VGO_linux)

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_debuglog.h"
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"
#include "pub_core_libcproc.h"
#include "pub_core_libcprint.h"
#include "pub_core_xarray.h"
#include "pub_core_clientstate.h"
#include "pub_core_aspacemgr.h"
#include "pub_core_mallocfree.h"
#include "pub_core_machine.h"
#include "pub_core_ume.h"
#include "pub_core_options.h"
#include "pub_core_syscall.h"
#include "pub_core_tooliface.h"       
#include "pub_core_threadstate.h"     
#include "priv_initimg_pathscan.h"
#include "pub_core_initimg.h"         

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <elf.h>




static void load_client ( ExeInfo* info, 
                          Addr*    client_ip,
			  Addr*    client_toc)
{
   const HChar* exe_name;
   Int    ret;
   SysRes res;

   vg_assert( VG_(args_the_exename) != NULL);
   exe_name = ML_(find_executable)( VG_(args_the_exename) );

   if (!exe_name) {
      VG_(printf)("valgrind: %s: command not found\n", VG_(args_the_exename));
      VG_(exit)(127);      
   }

   VG_(memset)(info, 0, sizeof(*info));
   ret = VG_(do_exec)(exe_name, info);
   if (ret < 0) {
      VG_(printf)("valgrind: could not execute '%s'\n", exe_name);
      VG_(exit)(1);
   }

   

   res = VG_(open)(exe_name, VKI_O_RDONLY, VKI_S_IRUSR);
   if (!sr_isError(res))
      VG_(cl_exec_fd) = sr_Res(res);

   
   *client_ip  = info->init_ip;
   *client_toc = info->init_toc;
   VG_(brk_base) = VG_(brk_limit) = VG_PGROUNDUP(info->brkbase);
}



static HChar** setup_client_env ( HChar** origenv, const HChar* toolname)
{
   vg_assert(origenv);
   vg_assert(toolname);

   const HChar* preload_core    = "vgpreload_core";
   const HChar* ld_preload      = "LD_PRELOAD=";
   const HChar* v_launcher      = VALGRIND_LAUNCHER "=";
   Int    ld_preload_len  = VG_(strlen)( ld_preload );
   Int    v_launcher_len  = VG_(strlen)( v_launcher );
   Bool   ld_preload_done = False;
   Int    vglib_len       = VG_(strlen)(VG_(libdir));
   Bool   debug           = False;

   HChar** cpp;
   HChar** ret;
   HChar*  preload_tool_path;
   Int     envc, i;

   Int preload_core_path_len = vglib_len + sizeof(preload_core) 
                                         + sizeof(VG_PLATFORM) + 16;
   Int preload_tool_path_len = vglib_len + VG_(strlen)(toolname) 
                                         + sizeof(VG_PLATFORM) + 16;
   Int preload_string_len    = preload_core_path_len + preload_tool_path_len;
   HChar* preload_string     = VG_(malloc)("initimg-linux.sce.1",
                                           preload_string_len);
   preload_tool_path = VG_(malloc)("initimg-linux.sce.2", preload_tool_path_len);
   VG_(snprintf)(preload_tool_path, preload_tool_path_len,
                 "%s/vgpreload_%s-%s.so", VG_(libdir), toolname, VG_PLATFORM);
   if (VG_(access)(preload_tool_path, True, False, False) == 0) {
      VG_(snprintf)(preload_string, preload_string_len, "%s/%s-%s.so:%s", 
                    VG_(libdir), preload_core, VG_PLATFORM, preload_tool_path);
   } else {
      VG_(snprintf)(preload_string, preload_string_len, "%s/%s-%s.so", 
                    VG_(libdir), preload_core, VG_PLATFORM);
   }
   VG_(free)(preload_tool_path);

   VG_(debugLog)(2, "initimg", "preload_string:\n");
   VG_(debugLog)(2, "initimg", "  \"%s\"\n", preload_string);

   
   if (debug) VG_(printf)("\n\n");
   envc = 0;
   for (cpp = origenv; cpp && *cpp; cpp++) {
      envc++;
      if (debug) VG_(printf)("XXXXXXXXX: BEFORE %s\n", *cpp);
   }

   
   ret = VG_(malloc) ("initimg-linux.sce.3",
                      sizeof(HChar *) * (envc+1+1)); 

   
   for (cpp = ret; *origenv; ) {
      if (debug) VG_(printf)("XXXXXXXXX: COPY   %s\n", *origenv);
      *cpp++ = *origenv++;
   }
   *cpp = NULL;
   
   vg_assert(envc == (cpp - ret));

   
   for (cpp = ret; cpp && *cpp; cpp++) {
      if (VG_(memcmp)(*cpp, ld_preload, ld_preload_len) == 0) {
         Int len = VG_(strlen)(*cpp) + preload_string_len;
         HChar *cp = VG_(malloc)("initimg-linux.sce.4", len);

         VG_(snprintf)(cp, len, "%s%s:%s",
                       ld_preload, preload_string, (*cpp)+ld_preload_len);

         *cpp = cp;

         ld_preload_done = True;
      }
      if (debug) VG_(printf)("XXXXXXXXX: MASH   %s\n", *cpp);
   }

   
   if (!ld_preload_done) {
      Int len = ld_preload_len + preload_string_len;
      HChar *cp = VG_(malloc) ("initimg-linux.sce.5", len);

      VG_(snprintf)(cp, len, "%s%s", ld_preload, preload_string);

      ret[envc++] = cp;
      if (debug) VG_(printf)("XXXXXXXXX: ADD    %s\n", cp);
   }

   
   
   for (i = 0; i < envc; i++)
      if (0 == VG_(memcmp(ret[i], v_launcher, v_launcher_len)))
         break;

   if (i < envc) {
      for (; i < envc-1; i++)
         ret[i] = ret[i+1];
      envc--;
   }

   VG_(free)(preload_string);
   ret[envc] = NULL;

   for (i = 0; i < envc; i++) {
      if (debug) VG_(printf)("XXXXXXXXX: FINAL  %s\n", ret[i]);
   }

   return ret;
}



#ifndef AT_DCACHEBSIZE
#define AT_DCACHEBSIZE		19
#endif 

#ifndef AT_ICACHEBSIZE
#define AT_ICACHEBSIZE		20
#endif 

#ifndef AT_UCACHEBSIZE
#define AT_UCACHEBSIZE		21
#endif 

#ifndef AT_BASE_PLATFORM
#define AT_BASE_PLATFORM	24
#endif 

#ifndef AT_RANDOM
#define AT_RANDOM		25
#endif 

#ifndef AT_HWCAP2
#define AT_HWCAP2		26
#endif 

#ifndef AT_EXECFN
#define AT_EXECFN		31
#endif 

#ifndef AT_SYSINFO
#define AT_SYSINFO		32
#endif 

#ifndef AT_SYSINFO_EHDR
#define AT_SYSINFO_EHDR		33
#endif 

#ifndef AT_SECURE
#define AT_SECURE 23   
#endif	

static HChar *copy_str(HChar **tab, const HChar *str)
{
   HChar *cp = *tab;
   HChar *orig = cp;

   while(*str)
      *cp++ = *str++;
   *cp++ = '\0';

   if (0)
      VG_(printf)("copied %p \"%s\" len %lld\n", orig, orig, (Long)(cp-orig));

   *tab = cp;

   return orig;
}



struct auxv
{
   Word a_type;
   union {
      void *a_ptr;
      Word a_val;
   } u;
};

static
struct auxv *find_auxv(UWord* sp)
{
   sp++;                

   while (*sp != 0)     
      sp++;
   sp++;

   while (*sp != 0)     
      sp++;
   sp++;
   
#if defined(VGA_ppc32) || defined(VGA_ppc64be) || defined(VGA_ppc64le)
# if defined AT_IGNOREPPC
   while (*sp == AT_IGNOREPPC)        
      sp += 2;
# endif
#endif

   return (struct auxv *)sp;
}

static 
Addr setup_client_stack( void*  init_sp,
                         HChar** orig_envp, 
                         const ExeInfo* info,
                         UInt** client_auxv,
                         Addr   clstack_end,
                         SizeT  clstack_max_size,
                         const VexArchInfo* vex_archinfo )
{

   SysRes res;
   HChar **cpp;
   HChar *strtab;		
   HChar *stringbase;
   Addr *ptr;
   struct auxv *auxv;
   const struct auxv *orig_auxv;
   const struct auxv *cauxv;
   unsigned stringsize;		
   unsigned auxsize;		
   Int argc;			
   Int envc;			
   unsigned stacksize;		
   Addr client_SP;	        
   Addr clstack_start;
   Int i;

   vg_assert(VG_IS_PAGE_ALIGNED(clstack_end+1));
   vg_assert( VG_(args_for_client) );

   
   orig_auxv = find_auxv(init_sp);

   

   
   stringsize   = 0;

   argc = 0;
   if (info->interp_name != NULL) {
      argc++;
      stringsize += VG_(strlen)(info->interp_name) + 1;
   }
   if (info->interp_args != NULL) {
      argc++;
      stringsize += VG_(strlen)(info->interp_args) + 1;
   }

   
   stringsize += VG_(strlen)( VG_(args_the_exename) ) + 1;

   for (i = 0; i < VG_(sizeXA)( VG_(args_for_client) ); i++) {
      argc++;
      stringsize += VG_(strlen)( * (HChar**) 
                                   VG_(indexXA)( VG_(args_for_client), i ))
                    + 1;
   }

   
   envc = 0;
   for (cpp = orig_envp; cpp && *cpp; cpp++) {
      envc++;
      stringsize += VG_(strlen)(*cpp) + 1;
   }

   
   auxsize = sizeof(*auxv);	
   for (cauxv = orig_auxv; cauxv->a_type != AT_NULL; cauxv++) {
      if (cauxv->a_type == AT_PLATFORM ||
          cauxv->a_type == AT_BASE_PLATFORM)
	 stringsize += VG_(strlen)(cauxv->u.a_ptr) + 1;
      else if (cauxv->a_type == AT_RANDOM)
	 stringsize += 16;
      else if (cauxv->a_type == AT_EXECFN)
	 stringsize += VG_(strlen)(VG_(args_the_exename)) + 1;
      auxsize += sizeof(*cauxv);
   }

#  if defined(VGP_ppc32_linux) || defined(VGP_ppc64be_linux) \
      || defined(VGP_ppc64le_linux)
   auxsize += 2 * sizeof(*cauxv);
#  endif

   
   stacksize =
      sizeof(Word) +                          
      sizeof(HChar **) +                      
      sizeof(HChar **)*argc +                 
      sizeof(HChar **) +                      
      sizeof(HChar **)*envc +                 
      sizeof(HChar **) +                      
      auxsize +                               
      VG_ROUNDUP(stringsize, sizeof(Word));   

   if (0) VG_(printf)("stacksize = %d\n", stacksize);

   
   client_SP = clstack_end - stacksize;
   client_SP = VG_ROUNDDN(client_SP, 16); 

   
   stringbase = strtab = (HChar *)clstack_end 
                         - VG_ROUNDUP(stringsize, sizeof(int));

   clstack_start = VG_PGROUNDDN(client_SP);

   
   clstack_max_size = VG_PGROUNDUP(clstack_max_size);

   if (0)
      VG_(printf)("stringsize=%d auxsize=%d stacksize=%d maxsize=0x%x\n"
                  "clstack_start %p\n"
                  "clstack_end   %p\n",
	          stringsize, auxsize, stacksize, (Int)clstack_max_size,
                  (void*)clstack_start, (void*)clstack_end);

   

   { SizeT anon_size   = clstack_end - clstack_start + 1;
     SizeT resvn_size  = clstack_max_size - anon_size;
     Addr  anon_start  = clstack_start;
     Addr  resvn_start = anon_start - resvn_size;
     SizeT inner_HACK  = 0;
     Bool  ok;

     vg_assert(VG_STACK_REDZONE_SZB >= 0);
     vg_assert(VG_STACK_REDZONE_SZB < VKI_PAGE_SIZE);
     if (VG_STACK_REDZONE_SZB > 0) {
        vg_assert(resvn_size > VKI_PAGE_SIZE);
        resvn_size -= VKI_PAGE_SIZE;
        anon_start -= VKI_PAGE_SIZE;
        anon_size += VKI_PAGE_SIZE;
     }

     vg_assert(VG_IS_PAGE_ALIGNED(anon_size));
     vg_assert(VG_IS_PAGE_ALIGNED(resvn_size));
     vg_assert(VG_IS_PAGE_ALIGNED(anon_start));
     vg_assert(VG_IS_PAGE_ALIGNED(resvn_start));
     vg_assert(resvn_start == clstack_end + 1 - clstack_max_size);

#    ifdef ENABLE_INNER
     inner_HACK = 1024*1024; 
#    endif

     if (0)
        VG_(printf)("%#lx 0x%lx  %#lx 0x%lx\n",
                    resvn_start, resvn_size, anon_start, anon_size);

     res = VG_(mk_SysRes_Error)(0);
     ok = VG_(am_create_reservation)(
             resvn_start,
             resvn_size -inner_HACK,
             SmUpper, 
             anon_size +inner_HACK
          );
     if (ok) {
        
        res = VG_(am_mmap_anon_fixed_client)(
                 anon_start -inner_HACK,
                 anon_size +inner_HACK,
	         info->stack_prot
	      );
     }
     if ((!ok) || sr_isError(res)) {
        
        VG_(printf)("valgrind: "
                    "I failed to allocate space for the application's stack.\n");
        VG_(printf)("valgrind: "
                    "This may be the result of a very large --main-stacksize=\n");
        VG_(printf)("valgrind: setting.  Cannot continue.  Sorry.\n\n");
        VG_(exit)(1);
     }

     vg_assert(ok);
     vg_assert(!sr_isError(res)); 

     
     VG_(clstk_start_base) = anon_start -inner_HACK;
     VG_(clstk_end)  = VG_(clstk_start_base) + anon_size +inner_HACK -1;

   }

   

   ptr = (Addr*)client_SP;

   
   *ptr++ = argc + 1;

   
   if (info->interp_name) {
      *ptr++ = (Addr)copy_str(&strtab, info->interp_name);
      VG_(free)(info->interp_name);
   }
   if (info->interp_args) {
      *ptr++ = (Addr)copy_str(&strtab, info->interp_args);
      VG_(free)(info->interp_args);
   }

   *ptr++ = (Addr)copy_str(&strtab, VG_(args_the_exename));

   for (i = 0; i < VG_(sizeXA)( VG_(args_for_client) ); i++) {
      *ptr++ = (Addr)copy_str(
                       &strtab, 
                       * (HChar**) VG_(indexXA)( VG_(args_for_client), i )
                     );
   }
   *ptr++ = 0;

   
   VG_(client_envp) = (HChar **)ptr;
   for (cpp = orig_envp; cpp && *cpp; ptr++, cpp++)
      *ptr = (Addr)copy_str(&strtab, *cpp);
   *ptr++ = 0;

   
   auxv = (struct auxv *)ptr;
   *client_auxv = (UInt *)auxv;
   VG_(client_auxv) = (UWord *)*client_auxv;
   
   
   
   

#  if defined(VGP_ppc32_linux) || defined(VGP_ppc64be_linux) \
      || defined(VGP_ppc64le_linux)
   auxv[0].a_type  = AT_IGNOREPPC;
   auxv[0].u.a_val = AT_IGNOREPPC;
   auxv[1].a_type  = AT_IGNOREPPC;
   auxv[1].u.a_val = AT_IGNOREPPC;
   auxv += 2;
#  endif

   for (; orig_auxv->a_type != AT_NULL; auxv++, orig_auxv++) {

      
      *auxv = *orig_auxv;

      
      switch(auxv->a_type) {

         case AT_IGNORE:
         case AT_PHENT:
         case AT_PAGESZ:
         case AT_FLAGS:
         case AT_NOTELF:
         case AT_UID:
         case AT_EUID:
         case AT_GID:
         case AT_EGID:
         case AT_CLKTCK:
#        if !defined(__ANDROID__)
         case AT_FPUCW: 
#        endif
            break;

         case AT_PHDR:
            if (info->phdr == 0)
               auxv->a_type = AT_IGNORE;
            else
               auxv->u.a_val = info->phdr;
            break;

         case AT_PHNUM:
            if (info->phdr == 0)
               auxv->a_type = AT_IGNORE;
            else
               auxv->u.a_val = info->phnum;
            break;

         case AT_BASE:
            auxv->u.a_val = info->interp_offset;
            break;

         case AT_PLATFORM:
         case AT_BASE_PLATFORM:
            
            auxv->u.a_ptr = copy_str(&strtab, orig_auxv->u.a_ptr);
            break;

         case AT_ENTRY:
            auxv->u.a_val = info->entry;
            break;

         case AT_HWCAP:
#           if defined(VGP_arm_linux)
            { Bool has_neon = (auxv->u.a_val & VKI_HWCAP_NEON) > 0;
              VG_(debugLog)(2, "initimg",
                               "ARM has-neon from-auxv: %s\n",
                               has_neon ? "YES" : "NO");
              VG_(machine_arm_set_has_NEON)( has_neon );
              #define VKI_HWCAP_TLS 32768
              Bool has_tls = (auxv->u.a_val & VKI_HWCAP_TLS) > 0;
              VG_(debugLog)(2, "initimg",
                               "ARM has-tls from-auxv: %s\n",
                               has_tls ? "YES" : "NO");
            }
#           endif
            break;
#        if defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
         case AT_HWCAP2:  {
            Bool auxv_2_07, hw_caps_2_07;
            auxv_2_07 = (auxv->u.a_val & 0x80000000ULL) == 0x80000000ULL;
            hw_caps_2_07 = (vex_archinfo->hwcaps & VEX_HWCAPS_PPC64_ISA2_07)
               == VEX_HWCAPS_PPC64_ISA2_07;

            vg_assert(auxv_2_07 == hw_caps_2_07);
            }

            break;
#           endif

         case AT_ICACHEBSIZE:
         case AT_DCACHEBSIZE:
         case AT_UCACHEBSIZE:
#           if defined(VGP_ppc32_linux)
            
            if (auxv->u.a_val > 0) {
               VG_(machine_ppc32_set_clszB)( auxv->u.a_val );
               VG_(debugLog)(2, "initimg", 
                                "PPC32 icache line size %u (type %u)\n", 
                                (UInt)auxv->u.a_val, (UInt)auxv->a_type );
            }
#           elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
            
            if (auxv->u.a_val > 0) {
               VG_(machine_ppc64_set_clszB)( auxv->u.a_val );
               VG_(debugLog)(2, "initimg", 
                                "PPC64 icache line size %u (type %u)\n", 
                                (UInt)auxv->u.a_val, (UInt)auxv->a_type );
            }
#           endif
            break;

#        if defined(VGP_ppc32_linux) || defined(VGP_ppc64be_linux) \
            || defined(VGP_ppc64le_linux)
         case AT_IGNOREPPC:
            break;
#        endif

         case AT_SECURE:
            auxv->u.a_val = 0;
            break;

         case AT_SYSINFO:
            
            auxv->a_type = AT_IGNORE;
            break;

#        if !defined(VGP_ppc32_linux) && !defined(VGP_ppc64be_linux) \
            && !defined(VGP_ppc64le_linux)
         case AT_SYSINFO_EHDR: {
            
            const NSegment* ehdrseg = VG_(am_find_nsegment)((Addr)auxv->u.a_ptr);
            vg_assert(ehdrseg);
            VG_(am_munmap_valgrind)(ehdrseg->start, ehdrseg->end - ehdrseg->start);
            auxv->a_type = AT_IGNORE;
            break;
         }
#        endif

         case AT_RANDOM:
            auxv->u.a_ptr = strtab;
            VG_(memcpy)(strtab, orig_auxv->u.a_ptr, 16);
            strtab += 16;
            break;

         case AT_EXECFN:
            
            auxv->u.a_ptr = copy_str(&strtab, VG_(args_the_exename));
            break;

         default:
            
            VG_(debugLog)(2, "initimg",
                             "stomping auxv entry %lld\n", 
                             (ULong)auxv->a_type);
            auxv->a_type = AT_IGNORE;
            break;
      }
   }
   *auxv = *orig_auxv;
   vg_assert(auxv->a_type == AT_NULL);

   vg_assert((strtab-stringbase) == stringsize);

   

   if (0) VG_(printf)("startup SP = %#lx\n", client_SP);
   return client_SP;
}



static void setup_client_dataseg ( SizeT max_size )
{
   Bool   ok;
   SysRes sres;
   Addr   anon_start  = VG_(brk_base);
   SizeT  anon_size   = VKI_PAGE_SIZE;
   Addr   resvn_start = anon_start + anon_size;
   SizeT  resvn_size  = max_size - anon_size;

   vg_assert(VG_IS_PAGE_ALIGNED(anon_size));
   vg_assert(VG_IS_PAGE_ALIGNED(resvn_size));
   vg_assert(VG_IS_PAGE_ALIGNED(anon_start));
   vg_assert(VG_IS_PAGE_ALIGNED(resvn_start));

   
   vg_assert(VG_(brk_base) == VG_(brk_limit));

   ok = VG_(am_create_reservation)( 
           resvn_start, 
           resvn_size, 
           SmLower, 
           anon_size
        );

   if (!ok) {
      anon_start = VG_(am_get_advisory_client_simple)
                      ( 0, anon_size+resvn_size, &ok );
      if (ok) {
         resvn_start = anon_start + anon_size;
         ok = VG_(am_create_reservation)( 
                 resvn_start, 
                 resvn_size, 
                 SmLower, 
                 anon_size
              );
         if (ok)
            VG_(brk_base) = VG_(brk_limit) = anon_start;
      }
   }
   vg_assert(ok);

   sres = VG_(am_mmap_anon_fixed_client)( 
             anon_start, 
             anon_size, 
             VKI_PROT_READ|VKI_PROT_WRITE|VKI_PROT_EXEC
          );
   vg_assert(!sr_isError(sres));
   vg_assert(sr_Res(sres) == anon_start);
}



IIFinaliseImageInfo VG_(ii_create_image)( IICreateImageInfo iicii,
                                          const VexArchInfo* vex_archinfo )
{
   ExeInfo info;
   HChar** env = NULL;

   IIFinaliseImageInfo iifii;
   VG_(memset)( &iifii, 0, sizeof(iifii) );

   
   
   
   
   
   VG_(debugLog)(1, "initimg", "Loading client\n");

   if (VG_(args_the_exename) == NULL)
      VG_(err_missing_prog)();

   load_client(&info, &iifii.initial_client_IP, &iifii.initial_client_TOC);

   
   
   
   
   
   VG_(debugLog)(1, "initimg", "Setup client env\n");
   env = setup_client_env(iicii.envp, iicii.toolname);

   
   
   
   
   
   {
      void* init_sp = iicii.argv - 1;
      SizeT m1  = 1024 * 1024;
      SizeT m16 = 16 * m1;
      SizeT szB = (SizeT)VG_(client_rlimit_stack).rlim_cur;
      if (szB < m1) szB = m1;
      if (szB > m16) szB = m16;
      if (VG_(clo_main_stacksize) > 0) szB = VG_(clo_main_stacksize);
      if (szB < m1) szB = m1;
      szB = VG_PGROUNDUP(szB);
      VG_(debugLog)(1, "initimg",
                       "Setup client stack: size will be %ld\n", szB);

      iifii.clstack_max_size = szB;

      iifii.initial_client_SP
         = setup_client_stack( init_sp, env, 
                               &info, &iifii.client_auxv, 
                               iicii.clstack_end, iifii.clstack_max_size,
                               vex_archinfo );

      VG_(free)(env);

      VG_(debugLog)(2, "initimg",
                       "Client info: "
                       "initial_IP=%p initial_TOC=%p brk_base=%p\n",
                       (void*)(iifii.initial_client_IP), 
                       (void*)(iifii.initial_client_TOC),
                       (void*)VG_(brk_base) );
      VG_(debugLog)(2, "initimg",
                       "Client info: "
                       "initial_SP=%p max_stack_size=%ld\n",
                       (void*)(iifii.initial_client_SP),
                       (SizeT)iifii.clstack_max_size );
   }

   
   
   
   
   
   { 
      SizeT m1 = 1024 * 1024;
      SizeT m8 = 8 * m1;
      SizeT dseg_max_size = (SizeT)VG_(client_rlimit_data).rlim_cur;
      VG_(debugLog)(1, "initimg", "Setup client data (brk) segment\n");
      if (dseg_max_size < m1) dseg_max_size = m1;
      if (dseg_max_size > m8) dseg_max_size = m8;
      dseg_max_size = VG_PGROUNDUP(dseg_max_size);

      setup_client_dataseg( dseg_max_size );
   }

   return iifii;
}



void VG_(ii_finalise_image)( IIFinaliseImageInfo iifii )
{
   ThreadArchState* arch = &VG_(threads)[1].arch;


#  if defined(VGP_x86_linux)
   vg_assert(0 == sizeof(VexGuestX86State) % LibVEX_GUEST_STATE_ALIGN);

   LibVEX_GuestX86_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestX86State));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestX86State));

   
   arch->vex.guest_ESP = iifii.initial_client_SP;
   arch->vex.guest_EIP = iifii.initial_client_IP;

   asm volatile("movw %%cs, %0" : : "m" (arch->vex.guest_CS));
   asm volatile("movw %%ds, %0" : : "m" (arch->vex.guest_DS));
   asm volatile("movw %%ss, %0" : : "m" (arch->vex.guest_SS));
   asm volatile("movw %%es, %0" : : "m" (arch->vex.guest_ES));

#  elif defined(VGP_amd64_linux)
   vg_assert(0 == sizeof(VexGuestAMD64State) % LibVEX_GUEST_STATE_ALIGN);

   LibVEX_GuestAMD64_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestAMD64State));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestAMD64State));

   
   arch->vex.guest_RSP = iifii.initial_client_SP;
   arch->vex.guest_RIP = iifii.initial_client_IP;

#  elif defined(VGP_ppc32_linux)
   vg_assert(0 == sizeof(VexGuestPPC32State) % LibVEX_GUEST_STATE_ALIGN);

   LibVEX_GuestPPC32_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestPPC32State));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestPPC32State));

   
   arch->vex.guest_GPR1 = iifii.initial_client_SP;
   arch->vex.guest_CIA  = iifii.initial_client_IP;

#  elif defined(VGP_ppc64be_linux) || defined(VGP_ppc64le_linux)
   vg_assert(0 == sizeof(VexGuestPPC64State) % LibVEX_GUEST_STATE_ALIGN);

   LibVEX_GuestPPC64_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestPPC64State));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestPPC64State));

   
   arch->vex.guest_GPR1 = iifii.initial_client_SP;
   arch->vex.guest_GPR2 = iifii.initial_client_TOC;
   arch->vex.guest_CIA  = iifii.initial_client_IP;
#if defined(VGP_ppc64le_linux)
   arch->vex.guest_GPR12 = iifii.initial_client_IP;
#endif

#  elif defined(VGP_arm_linux)
   LibVEX_GuestARM_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestARMState));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestARMState));

   arch->vex.guest_R13  = iifii.initial_client_SP;
   arch->vex.guest_R15T = iifii.initial_client_IP;

   
   
   arch->vex.guest_R1 =  iifii.initial_client_SP;

#  elif defined(VGP_arm64_linux)
   
   LibVEX_GuestARM64_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestARM64State));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestARM64State));

   arch->vex.guest_XSP = iifii.initial_client_SP;
   arch->vex.guest_PC  = iifii.initial_client_IP;

#  elif defined(VGP_s390x_linux)
   vg_assert(0 == sizeof(VexGuestS390XState) % LibVEX_GUEST_STATE_ALIGN);

   LibVEX_GuestS390X_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0xFF, sizeof(VexGuestS390XState));
   VG_(memset)(&arch->vex_shadow2, 0x00, sizeof(VexGuestS390XState));
   
   arch->vex_shadow1.guest_SP = 0;
   arch->vex_shadow1.guest_fpc = 0;
   arch->vex_shadow1.guest_IA = 0;

   
   arch->vex.guest_SP = iifii.initial_client_SP;
   arch->vex.guest_IA = iifii.initial_client_IP;
   
   arch->vex.guest_fpc = 0;

   
   VG_TRACK(post_reg_write, Vg_CoreStartup, 1, VG_O_STACK_PTR, 8);
   VG_TRACK(post_reg_write, Vg_CoreStartup, 1, VG_O_FPC_REG,   4);
   VG_TRACK(post_reg_write, Vg_CoreStartup, 1, VG_O_INSTR_PTR, 8);

#define PRECISE_GUEST_REG_DEFINEDNESS_AT_STARTUP 1

#  elif defined(VGP_mips32_linux)
   vg_assert(0 == sizeof(VexGuestMIPS32State) % LibVEX_GUEST_STATE_ALIGN);
   LibVEX_GuestMIPS32_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestMIPS32State));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestMIPS32State));

   arch->vex.guest_r29 = iifii.initial_client_SP;
   arch->vex.guest_PC = iifii.initial_client_IP;
   arch->vex.guest_r31 = iifii.initial_client_SP;

#   elif defined(VGP_mips64_linux)
   vg_assert(0 == sizeof(VexGuestMIPS64State) % LibVEX_GUEST_STATE_ALIGN);
   LibVEX_GuestMIPS64_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestMIPS64State));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestMIPS64State));

   arch->vex.guest_r29 = iifii.initial_client_SP;
   arch->vex.guest_PC = iifii.initial_client_IP;
   arch->vex.guest_r31 = iifii.initial_client_SP;

#  elif defined(VGP_tilegx_linux)
   vg_assert(0 == sizeof(VexGuestTILEGXState) % LibVEX_GUEST_STATE_ALIGN);

   
   LibVEX_GuestTILEGX_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestTILEGXState));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestTILEGXState));

   
   arch->vex.guest_r54 = iifii.initial_client_SP;
   arch->vex.guest_pc  = iifii.initial_client_IP;

#  else
#    error Unknown platform
#  endif

#  if !defined(PRECISE_GUEST_REG_DEFINEDNESS_AT_STARTUP)
   
   VG_TRACK( post_reg_write, Vg_CoreStartup, 1, 0,
             sizeof(VexGuestArchState));
#  endif

   const NSegment *seg = VG_(am_find_nsegment)(VG_(brk_base));
   vg_assert(seg);
   vg_assert(seg->kind == SkAnonC);
   VG_TRACK(new_mem_brk, VG_(brk_base), seg->end + 1 - VG_(brk_base),
            1);
   VG_TRACK(die_mem_brk, VG_(brk_base), seg->end + 1 - VG_(brk_base));
}

#endif 




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

#if defined(VGO_darwin)

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
#include "pub_core_tooliface.h"       
#include "pub_core_threadstate.h"     
#include "priv_initimg_pathscan.h"
#include "pub_core_initimg.h"         




static void load_client ( ExeInfo* info, 
                          Addr*    client_ip)
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

   

   res = VG_(open)(exe_name, VKI_O_RDONLY, VKI_S_IRUSR);
   if (!sr_isError(res))
      VG_(cl_exec_fd) = sr_Res(res);

   
   *client_ip  = info->init_ip;
}



static HChar** setup_client_env ( HChar** origenv, const HChar* toolname)
{
   const HChar* preload_core    = "vgpreload_core";
   const HChar* ld_preload      = "DYLD_INSERT_LIBRARIES=";
   const HChar* dyld_cache      = "DYLD_SHARED_REGION=";
   const HChar* dyld_cache_value= "avoid";
   const HChar* v_launcher      = VALGRIND_LAUNCHER "=";
   Int    ld_preload_len  = VG_(strlen)( ld_preload );
   Int    dyld_cache_len  = VG_(strlen)( dyld_cache );
   Int    v_launcher_len  = VG_(strlen)( v_launcher );
   Bool   ld_preload_done = False;
   Bool   dyld_cache_done = False;
   Int    vglib_len       = VG_(strlen)(VG_(libdir));

   HChar** cpp;
   HChar** ret;
   HChar*  preload_tool_path;
   Int     envc, i;

   Int preload_core_path_len = vglib_len + sizeof(preload_core) 
                                         + sizeof(VG_PLATFORM) + 16;
   Int preload_tool_path_len = vglib_len + VG_(strlen)(toolname) 
                                         + sizeof(VG_PLATFORM) + 16;
   Int preload_string_len    = preload_core_path_len + preload_tool_path_len;
   HChar* preload_string     = VG_(malloc)("initimg-darwin.sce.1", preload_string_len);

   preload_tool_path = VG_(malloc)("initimg-darwin.sce.2", preload_tool_path_len);
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

   
   envc = 0;
   for (cpp = origenv; cpp && *cpp; cpp++)
      envc++;

   
   ret = VG_(malloc) ("initimg-darwin.sce.3", 
                      sizeof(HChar *) * (envc+2+1)); 

   
   for (cpp = ret; *origenv; )
      *cpp++ = *origenv++;
   *cpp = NULL;
   
   vg_assert(envc == (cpp - ret));

   
   for (cpp = ret; cpp && *cpp; cpp++) {
      if (VG_(memcmp)(*cpp, ld_preload, ld_preload_len) == 0) {
         Int len = VG_(strlen)(*cpp) + preload_string_len;
         HChar *cp = VG_(malloc)("initimg-darwin.sce.4", len);

         VG_(snprintf)(cp, len, "%s%s:%s",
                       ld_preload, preload_string, (*cpp)+ld_preload_len);

         *cpp = cp;

         ld_preload_done = True;
      }
      if (VG_(memcmp)(*cpp, dyld_cache, dyld_cache_len) == 0) {
         Int len = dyld_cache_len + VG_(strlen)(dyld_cache_value) + 1;
         HChar *cp = VG_(malloc)("initimg-darwin.sce.4.2", len);

         VG_(snprintf)(cp, len, "%s%s", dyld_cache, dyld_cache_value);

         *cpp = cp;

         ld_preload_done = True;
      }
   }

   
   if (!ld_preload_done) {
      Int len = ld_preload_len + preload_string_len;
      HChar *cp = VG_(malloc) ("initimg-darwin.sce.5", len);

      VG_(snprintf)(cp, len, "%s%s", ld_preload, preload_string);

      ret[envc++] = cp;
   }
   if (!dyld_cache_done) {
      Int len = dyld_cache_len + VG_(strlen)(dyld_cache_value) + 1;
      HChar *cp = VG_(malloc) ("initimg-darwin.sce.5.2", len);

      VG_(snprintf)(cp, len, "%s%s", dyld_cache, dyld_cache_value);

      ret[envc++] = cp;
   }
   

   
   
   for (i = 0; i < envc; i++)
      if (0 == VG_(memcmp)(ret[i], v_launcher, v_launcher_len))
         break;

   if (i < envc) {
      for (; i < envc-1; i++)
         ret[i] = ret[i+1];
      envc--;
   }

   
   for (i = 0; i < envc; i++) {
      if (0 == VG_(strncmp)(ret[i], "VYLD_", 5)) {
         ret[i][0] = 'D';
      }
   }


   VG_(free)(preload_string);
   ret[envc] = NULL;
   return ret;
}



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



static 
Addr setup_client_stack( void*  init_sp,
                         HChar** orig_envp, 
                         const ExeInfo* info,
                         Addr   clstack_end,
                         SizeT  clstack_max_size,
                         const VexArchInfo* vex_archinfo )
{
   HChar **cpp;
   HChar *strtab;		
   HChar *stringbase;
   Addr *ptr;
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

   

   
   stringsize   = 0;
   auxsize = 0;

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

   
   auxsize += 2 * sizeof(Word);
   if (info->executable_path) {
       stringsize += 1 + VG_(strlen)(info->executable_path);
   }

   
   if (info->dynamic) auxsize += sizeof(Word);

   
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

   
   client_SP = clstack_end + 1 - stacksize;
   client_SP = VG_ROUNDDN(client_SP, 32); 

   
   stringbase = strtab = (HChar *)clstack_end 
                         - VG_ROUNDUP(stringsize, sizeof(int));

   
   clstack_max_size = VG_PGROUNDUP(clstack_max_size);

   
   clstack_start = clstack_end + 1 - clstack_max_size;

   
   
   VG_(clstk_start_base) = clstack_start;
   VG_(clstk_end)  = clstack_end;

   if (0)
      VG_(printf)("stringsize=%d auxsize=%d stacksize=%d maxsize=0x%x\n"
                  "clstack_start %p\n"
                  "clstack_end   %p\n",
	          stringsize, auxsize, stacksize, (Int)clstack_max_size,
                  (void*)clstack_start, (void*)clstack_end);

   

   

   

   ptr = (Addr*)client_SP;

   
   if (info->dynamic) *ptr++ = info->text;

   
   *ptr++ = (Addr)(argc + 1);

   
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

   
   if (info->executable_path) 
       *ptr++ = (Addr)copy_str(&strtab, info->executable_path);
   else 
       *ptr++ = 0;
   *ptr++ = 0;

   vg_assert((strtab-stringbase) == stringsize);

   

   if (0) VG_(printf)("startup SP = %#lx\n", client_SP);
   return client_SP;
}



static void record_system_memory(void)
{
   return;
   

   
#if defined(VGA_amd64)
   
   
   VG_(am_notify_client_mmap)(0x7fffffe00000, 0x7ffffffff000-0x7fffffe00000,
                              VKI_PROT_READ|VKI_PROT_EXEC, 0, -1, 0);

#elif defined(VGA_x86)
   
   
   VG_(am_notify_client_mmap)(0xfffec000, 0xfffff000-0xfffec000,
                              VKI_PROT_READ|VKI_PROT_EXEC, 0, -1, 0);

#else
#  error unknown architecture
#endif  
}



IIFinaliseImageInfo VG_(ii_create_image)( IICreateImageInfo iicii,
                                          const VexArchInfo* vex_archinfo )
{
   ExeInfo info;
   VG_(memset)( &info, 0, sizeof(info) );

   HChar** env = NULL;

   IIFinaliseImageInfo iifii;
   VG_(memset)( &iifii, 0, sizeof(iifii) );

   
   
   
   
   
   VG_(debugLog)(1, "initimg", "Loading client\n");

   if (VG_(args_the_exename) == NULL)
      VG_(err_missing_prog)();

   load_client(&info, &iifii.initial_client_IP);

   
   
   
   
   
   VG_(debugLog)(1, "initimg", "Setup client env\n");
   env = setup_client_env(iicii.envp, iicii.toolname);

   
   
   
   
   
   iicii.clstack_end = info.stack_end;
   iifii.clstack_max_size = info.stack_end - info.stack_start + 1;
   
   iifii.initial_client_SP = 
       setup_client_stack( iicii.argv - 1, env, &info, 
                           iicii.clstack_end, iifii.clstack_max_size,
                           vex_archinfo );

   VG_(free)(env);

   VG_(debugLog)(2, "initimg",
                 "Client info: "
                 "initial_IP=%p initial_SP=%p stack=[%p..%p]\n", 
                 (void*)(iifii.initial_client_IP),
                 (void*)(iifii.initial_client_SP),
                 (void*)(info.stack_start), 
                 (void*)(info.stack_end));


   
   record_system_memory();

   return iifii;
}



void VG_(ii_finalise_image)( IIFinaliseImageInfo iifii )
{
   ThreadArchState* arch = &VG_(threads)[1].arch;

   

#  if defined(VGP_x86_darwin)
   vg_assert(0 == sizeof(VexGuestX86State) % 16);

   LibVEX_GuestX86_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestX86State));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestX86State));

   
   arch->vex.guest_ESP = iifii.initial_client_SP;
   arch->vex.guest_EIP = iifii.initial_client_IP;

#  elif defined(VGP_amd64_darwin)
   vg_assert(0 == sizeof(VexGuestAMD64State) % 16);

   LibVEX_GuestAMD64_initialise(&arch->vex);

   
   VG_(memset)(&arch->vex_shadow1, 0, sizeof(VexGuestAMD64State));
   VG_(memset)(&arch->vex_shadow2, 0, sizeof(VexGuestAMD64State));

   
   arch->vex.guest_RSP = iifii.initial_client_SP;
   arch->vex.guest_RIP = iifii.initial_client_IP;

#  else
#    error Unknown platform
#  endif

   
   VG_TRACK( post_reg_write, Vg_CoreStartup, 1, 0,
             sizeof(VexGuestArchState));
}

#endif 


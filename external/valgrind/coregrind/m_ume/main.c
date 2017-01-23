

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


#include "pub_core_basics.h"
#include "pub_core_vki.h"

#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"    
#include "pub_core_libcfile.h"      
#include "pub_core_libcprint.h"     
#include "pub_core_mallocfree.h"    
#include "pub_core_syscall.h"       
#include "pub_core_options.h"       
#include "pub_core_ume.h"           

#include "priv_ume.h"


typedef struct {
   Bool (*match_fn)(const void *hdr, SizeT len);
   Int  (*load_fn)(Int fd, const HChar *name, ExeInfo *info);
} ExeHandler;

static ExeHandler exe_handlers[] = {
#  if defined(VGO_linux)
   { VG_(match_ELF),    VG_(load_ELF) },
#  elif defined(VGO_darwin)
   { VG_(match_macho),  VG_(load_macho) },
#  else
#    error "unknown OS"
#  endif
   { VG_(match_script), VG_(load_script) },
};
#define EXE_HANDLER_COUNT (sizeof(exe_handlers)/sizeof(exe_handlers[0]))


SysRes 
VG_(pre_exec_check)(const HChar* exe_name, Int* out_fd, Bool allow_setuid)
{
   Int fd, ret, i;
   SysRes res;
   Char  buf[4096];
   SizeT bufsz = sizeof buf, fsz;
   Bool is_setuid = False;

   
   res = VG_(open)(exe_name, VKI_O_RDONLY, 0);
   if (sr_isError(res)) {
      return res;
   }
   fd = sr_Res(res);

   
   ret = VG_(check_executable)(&is_setuid, exe_name, allow_setuid);
   if (0 != ret) {
      VG_(close)(fd);
      if (is_setuid && !VG_(clo_xml)) {
         VG_(message)(Vg_UserMsg, "\n");
         VG_(message)(Vg_UserMsg,
                      "Warning: Can't execute setuid/setgid/setcap executable: %s\n",
                      exe_name);
         VG_(message)(Vg_UserMsg, "Possible workaround: remove "
                      "--trace-children=yes, if in effect\n");
         VG_(message)(Vg_UserMsg, "\n");
      }
      return VG_(mk_SysRes_Error)(ret);
   }

   fsz = (SizeT)VG_(fsize)(fd);
   if (fsz < bufsz)
      bufsz = fsz;

   res = VG_(pread)(fd, buf, bufsz, 0);
   if (sr_isError(res) || sr_Res(res) != bufsz) {
      VG_(close)(fd);
      return VG_(mk_SysRes_Error)(VKI_EACCES);
   }
   bufsz = sr_Res(res);

   
   for (i = 0; i < EXE_HANDLER_COUNT; i++) {
      if ((*exe_handlers[i].match_fn)(buf, bufsz)) {
         res = VG_(mk_SysRes_Success)(i);
         break;
      }
   }
   if (i == EXE_HANDLER_COUNT) {
      
      res = VG_(mk_SysRes_Error)(VKI_ENOEXEC);
   }

   
   if (!sr_isError(res) && out_fd) {
      *out_fd = fd; 
   } else { 
      VG_(close)(fd);
   }

   return res;
}

Int VG_(do_exec_inner)(const HChar* exe, ExeInfo* info)
{
   SysRes res;
   Int fd;
   Int ret;

   res = VG_(pre_exec_check)(exe, &fd, False);
   if (sr_isError(res))
      return sr_Err(res);

   vg_assert2(sr_Res(res) >= 0 && sr_Res(res) < EXE_HANDLER_COUNT, 
              "invalid VG_(pre_exec_check) result");

   ret = (*exe_handlers[sr_Res(res)].load_fn)(fd, exe, info);

   VG_(close)(fd);

   return ret;
}


static Bool is_hash_bang_file(const HChar* f)
{
   SysRes res = VG_(open)(f, VKI_O_RDONLY, 0);
   if (!sr_isError(res)) {
      HChar buf[3] = {0,0,0};
      Int fd = sr_Res(res);
      Int n  = VG_(read)(fd, buf, 2); 
      if (n == 2 && VG_STREQ("#!", buf))
         return True;
   }
   return False;
}

static Bool is_binary_file(const HChar* f)
{
   SysRes res = VG_(open)(f, VKI_O_RDONLY, 0);
   if (!sr_isError(res)) {
      UChar buf[80];
      Int fd = sr_Res(res);
      Int n  = VG_(read)(fd, buf, 80); 
      Int i;
      for (i = 0; i < n; i++) {
         if (buf[i] > 127)
            return True;      
      }
      return False;
   } else {
      
      
      
      VG_(fmsg)("%s: unknown error\n", f);
      VG_(exit)(126);      
   }
}

static Int do_exec_shell_followup(Int ret, const HChar* exe_name, ExeInfo* info)
{
#  if defined(VGPV_arm_linux_android) \
      || defined(VGPV_x86_linux_android) \
      || defined(VGPV_mips32_linux_android) \
      || defined(VGPV_arm64_linux_android)
   const HChar*  default_interp_name = "/system/bin/sh";
#  else
   const HChar*  default_interp_name = "/bin/sh";
#  endif

   SysRes res;
   struct vg_stat st;

   if (VKI_ENOEXEC == ret) {
      
      

      
      if (is_binary_file(exe_name)) {
         VG_(fmsg)("%s: cannot execute binary file\n", exe_name);
         VG_(exit)(126);      
      }

      
      

      info->interp_name = VG_(strdup)("ume.desf.1", default_interp_name);
      info->interp_args = NULL;
      if (info->argv && info->argv[0] != NULL)
         info->argv[0] = exe_name;

      ret = VG_(do_exec_inner)(info->interp_name, info);

      if (0 != ret) {
         
         VG_(fmsg)("%s: bad interpreter (%s): %s\n",
                     exe_name, info->interp_name, VG_(strerror)(ret));
         VG_(exit)(126);      
      }

   } else if (0 != ret) {
      
      
      Int exit_code = 126;    

      res = VG_(stat)(exe_name, &st);

      
      if (sr_isError(res) && sr_Err(res) == VKI_ENOENT) {
         VG_(fmsg)("%s: %s\n", exe_name, VG_(strerror)(ret));
         exit_code = 127;     

      
      } else if (!sr_isError(res) && VKI_S_ISDIR(st.mode)) {
         VG_(fmsg)("%s: is a directory\n", exe_name);
      
      
      } else if (0 != VG_(check_executable)(NULL, exe_name, 
                                            False)) {
         VG_(fmsg)("%s: %s\n", exe_name, VG_(strerror)(ret));

      
      } else if (is_hash_bang_file(exe_name)) {
         VG_(fmsg)("%s: bad interpreter: %s\n", exe_name, VG_(strerror)(ret));

      
      } else {
         VG_(fmsg)("%s: %s\n", exe_name, VG_(strerror)(ret));
      }
      VG_(exit)(exit_code);
   }
   return ret;
}


Int VG_(do_exec)(const HChar* exe_name, ExeInfo* info)
{
   Int ret;
   
   info->interp_name = NULL;
   info->interp_args = NULL;

   ret = VG_(do_exec_inner)(exe_name, info);

   if (0 != ret) {
      ret = do_exec_shell_followup(ret, exe_name, info);
   }
   return ret;
}


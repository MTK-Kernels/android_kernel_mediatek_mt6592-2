

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



#include "pub_core_debuglog.h"
#include "pub_core_vki.h"       
                                
#include "pub_core_libcproc.h"  
#include "pub_core_ume.h"

#include <assert.h>
#include <ctype.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef EM_X86_64
#define EM_X86_64 62    
#endif

#ifndef EM_AARCH64
#define EM_AARCH64 183  
#endif

#ifndef EM_PPC64
#define EM_PPC64 21  
#endif

#ifndef EM_TILEGX
#define EM_TILEGX 191
#endif

__attribute__((noreturn))
static void barf ( const char *format, ... )
{
   va_list vargs;

   va_start(vargs, format);
   fprintf(stderr, "valgrind: Cannot continue: ");
   vfprintf(stderr, format, vargs);
   fprintf(stderr, "\n");
   va_end(vargs);

   exit(1);
   
   assert(0);
}

static const char *find_client(const char *clientname)
{
   char *fullname;
   const char *path = getenv("PATH");
   const char *colon;

   assert(clientname != NULL);

   if (path == NULL) return clientname;

   
   unsigned need = strlen(path) + strlen("/") + strlen(clientname) + 1;

   fullname = malloc(need);
   if (fullname == NULL)
      barf("malloc of fullname failed.");

   while (path)
   {
      if ((colon = strchr(path, ':')) == NULL)
      {
         strcpy(fullname, path);
         path = NULL;
      }
      else
      {
         strncpy(fullname, path, colon - path);
         fullname[colon - path] = '\0';
         path = colon + 1;
      }

      strcat(fullname, "/");
      strcat(fullname, clientname);

      if (access(fullname, R_OK|X_OK) == 0)
         return fullname;
   }
   free(fullname);

   return clientname;
}

static const char *select_platform(const char *clientname)
{
   int fd;
   char header[4096];
   ssize_t n_bytes;
   const char *platform = NULL;

   VG_(debugLog)(2, "launcher", "selecting platform for '%s'\n", clientname);

   if (strchr(clientname, '/') == NULL)
      clientname = find_client(clientname);

   VG_(debugLog)(2, "launcher", "selecting platform for '%s'\n", clientname);

   if ((fd = open(clientname, O_RDONLY)) < 0)
      return NULL;
   

   VG_(debugLog)(2, "launcher", "opened '%s'\n", clientname);

   n_bytes = read(fd, header, sizeof(header));
   close(fd);
   if (n_bytes < 2) {
      return NULL;
   }

   VG_(debugLog)(2, "launcher", "read %ld bytes from '%s'\n",
                    (long int)n_bytes, clientname);

   if (header[0] == '#' && header[1] == '!') {
      int i = 2;

      STATIC_ASSERT(VKI_BINPRM_BUF_SIZE < sizeof header);
      if (n_bytes > VKI_BINPRM_BUF_SIZE)
         n_bytes = VKI_BINPRM_BUF_SIZE - 1;
      header[n_bytes] = '\0';
      char *eol = strchr(header, '\n');
      if (eol != NULL)
         *eol = '\0';
 
      
      while  (header[i] == ' '|| header[i] == '\t')
         i++;

      
      const char *interp = header + i;

      if (header[i] == '\0') {
         
#  if defined(VGPV_arm_linux_android) \
      || defined(VGPV_x86_linux_android) \
      || defined(VGPV_mips32_linux_android) \
      || defined(VGPV_arm64_linux_android)
         interp = "/system/bin/sh";
#  else
         interp = "/bin/sh";
#  endif
      } else {
         while (header[i]) {
            if (header[i] == ' ' || header[i] == '\t') break;
            i++;
         }
         header[i] = '\0';
      }

      platform = select_platform(interp);

   } else if (n_bytes >= SELFMAG && memcmp(header, ELFMAG, SELFMAG) == 0) {

      if (n_bytes >= sizeof(Elf32_Ehdr) && header[EI_CLASS] == ELFCLASS32) {
         const Elf32_Ehdr *ehdr = (Elf32_Ehdr *)header;

         if (header[EI_DATA] == ELFDATA2LSB) {
            if (ehdr->e_machine == EM_386 &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "x86-linux";
            }
            else 
            if (ehdr->e_machine == EM_ARM &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "arm-linux";
            }
            else
            if (ehdr->e_machine == EM_MIPS &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "mips32-linux";
            }
         }
         else if (header[EI_DATA] == ELFDATA2MSB) {
            if (ehdr->e_machine == EM_PPC &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "ppc32-linux";
            }
            else 
            if (ehdr->e_machine == EM_MIPS &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "mips32-linux";
            }
         }

      } else if (n_bytes >= sizeof(Elf64_Ehdr) && header[EI_CLASS] == ELFCLASS64) {
         const Elf64_Ehdr *ehdr = (Elf64_Ehdr *)header;

         if (header[EI_DATA] == ELFDATA2LSB) {
            if (ehdr->e_machine == EM_X86_64 &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "amd64-linux";
            } else if (ehdr->e_machine == EM_MIPS &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "mips64-linux";
            } else if (ehdr->e_machine == EM_TILEGX &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "tilegx-linux";
            } else if (ehdr->e_machine == EM_AARCH64 &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "arm64-linux";
            } else if (ehdr->e_machine == EM_PPC64 &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "ppc64le-linux";
            }
         } else if (header[EI_DATA] == ELFDATA2MSB) {
#           if !defined(VGPV_arm_linux_android) \
               && !defined(VGPV_x86_linux_android) \
               && !defined(VGPV_mips32_linux_android) \
               && !defined(VGPV_arm64_linux_android)
            if (ehdr->e_machine == EM_PPC64 &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "ppc64be-linux";
            } 
            else 
            if (ehdr->e_machine == EM_S390 &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "s390x-linux";
            } else if (ehdr->e_machine == EM_MIPS &&
                (ehdr->e_ident[EI_OSABI] == ELFOSABI_SYSV ||
                 ehdr->e_ident[EI_OSABI] == ELFOSABI_LINUX)) {
               platform = "mips64-linux";
            }
#           endif
         }
      }
   }

   VG_(debugLog)(2, "launcher", "selected platform '%s'\n",
                 platform ? platform : "unknown");

   return platform;
}

static const char *valgrind_lib = VG_LIBDIR;

int main(int argc, char** argv, char** envp)
{
   int i, j, loglevel, r;
   const char *toolname = NULL;
   const char *clientname = NULL;
   const char *platform;
   const char *default_platform;
   const char *cp;
   char *toolfile;
   const char *launcher_name;
   char* new_line;
   char** new_env;

   loglevel = 0;
   for (i = 1; i < argc; i++) {
      if (argv[i][0] != '-') {
         clientname = argv[i];
         break;
      }
      if (0 == strcmp(argv[i], "--")) {
         if (i+1 < argc)
            clientname = argv[i+1];
         break;
      }
      if (0 == strcmp(argv[i], "-d")) 
         loglevel++;
      if (0 == strncmp(argv[i], "--tool=", 7)) 
         toolname = argv[i] + 7;
   }

   VG_(debugLog_startup)(loglevel, "Stage 1");

   
   if (toolname) {
      VG_(debugLog)(1, "launcher", "tool '%s' requested\n", toolname);
   } else {
      VG_(debugLog)(1, "launcher", 
                       "no tool requested, defaulting to 'memcheck'\n");
      toolname = "memcheck";
   }

   if ((0==strcmp(VG_PLATFORM,"x86-linux"))    ||
       (0==strcmp(VG_PLATFORM,"amd64-linux"))  ||
       (0==strcmp(VG_PLATFORM,"ppc32-linux"))  ||
       (0==strcmp(VG_PLATFORM,"ppc64be-linux"))  ||
       (0==strcmp(VG_PLATFORM,"ppc64le-linux"))  ||
       (0==strcmp(VG_PLATFORM,"arm-linux"))    ||
       (0==strcmp(VG_PLATFORM,"arm64-linux"))  ||
       (0==strcmp(VG_PLATFORM,"s390x-linux"))  ||
       (0==strcmp(VG_PLATFORM,"tilegx-linux")) ||
       (0==strcmp(VG_PLATFORM,"mips32-linux")) ||
       (0==strcmp(VG_PLATFORM,"mips64-linux")))
      default_platform = VG_PLATFORM;
   else
      barf("Unknown VG_PLATFORM '%s'", VG_PLATFORM);

   if (clientname == NULL) {
      VG_(debugLog)(1, "launcher", 
                       "no client specified, defaulting platform to '%s'\n",
                        default_platform);
      platform = default_platform;
   } else if ((platform = select_platform(clientname)) != NULL) {
      VG_(debugLog)(1, "launcher", "selected platform '%s'\n", platform);
   } else {
      VG_(debugLog)(1, "launcher", 
                       "no platform detected, defaulting platform to '%s'\n",
                       default_platform);
      platform = default_platform;
   }
   
   unsigned bufsiz = 0;
   char *buf = NULL;

   while (42) {
      bufsiz += 500;
      buf = realloc(buf, bufsiz);
      if (buf == NULL)
         barf("realloc of buf failed.");
      r = readlink("/proc/self/exe", buf, bufsiz);
      if (r == -1) {
        fprintf(stderr, "valgrind: warning (non-fatal): "
                "readlink(\"/proc/self/exe\") failed.\n");
        fprintf(stderr, "valgrind: continuing, however --trace-children=yes "
                "will not work.\n");
        launcher_name = "";
        break;
      }
      if (r == bufsiz) continue;   

      assert(r < bufsiz);   

      buf[r] = '\0';
      launcher_name = buf;
      break;
   }

   
   new_line = malloc(strlen(VALGRIND_LAUNCHER) + 1 
                     + strlen(launcher_name) + 1);
   if (new_line == NULL)
      barf("malloc of new_line failed.");
   strcpy(new_line, VALGRIND_LAUNCHER);
   strcat(new_line, "=");
   strcat(new_line, launcher_name);

   for (j = 0; envp[j]; j++)
      ;
   new_env = malloc((j+2) * sizeof(char*));
   if (new_env == NULL)
      barf("malloc of new_env failed.");
   for (i = 0; i < j; i++)
      new_env[i] = envp[i];
   new_env[i++] = new_line;
   new_env[i++] = NULL;
   assert(i == j+2);

   
   cp = getenv(VALGRIND_LIB);

   if (cp != NULL)
      valgrind_lib = cp;

   
   toolfile = malloc(strlen(valgrind_lib) + strlen(toolname) + strlen(platform) + 3);
   if (toolfile == NULL)
      barf("malloc of toolfile failed.");
   sprintf(toolfile, "%s/%s-%s", valgrind_lib, toolname, platform);

   VG_(debugLog)(1, "launcher", "launching %s\n", toolfile);

   execve(toolfile, argv, new_env);

   fprintf(stderr, "valgrind: failed to start tool '%s' for platform '%s': %s\n",
                   toolname, platform, strerror(errno));

   exit(1);
}

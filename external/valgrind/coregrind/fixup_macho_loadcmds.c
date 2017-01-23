
/* Derived from Valgrind sources, coregrind/m_debuginfo/readmacho.c.
   GPL 2+ therefore.

   Can be compiled as either a 32- or 64-bit program (doesn't matter).
*/


#define DEBUGPRINTING 0

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#undef PLAT_x86_darwin
#undef PLAT_amd64_darwin

#if defined(__APPLE__) && defined(__i386__)
#  define PLAT_x86_darwin 1
#elif defined(__APPLE__) && defined(__x86_64__)
#  define PLAT_amd64_darwin 1
#else
#  error "Can't be compiled on this platform"
#endif

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/fat.h>
#include <mach/i386/thread_status.h>

#include "config.h"
#if DARWIN_VERS != DARWIN_10_5 && DARWIN_VERS != DARWIN_10_6 \
    && DARWIN_VERS != DARWIN_10_7 && DARWIN_VERS != DARWIN_10_8 \
    && DARWIN_VERS != DARWIN_10_9 && DARWIN_VERS != DARWIN_10_10
#  error "Unknown DARWIN_VERS value.  This file only compiles on Darwin."
#endif


typedef  unsigned char   UChar;
typedef    signed char   Char;
typedef           char   HChar; 

typedef  unsigned int    UInt;
typedef    signed int    Int;

typedef  unsigned char   Bool;
#define  True   ((Bool)1)
#define  False  ((Bool)0)

typedef  unsigned long   UWord;

typedef  UWord           SizeT;
typedef  UWord           Addr;

typedef  unsigned long long int   ULong;
typedef    signed long long int   Long;



__attribute__((noreturn))
void fail ( HChar* msg )
{
   fprintf(stderr, "fixup_macho_loadcmds: fail: %s\n", msg);
   exit(1);
}



typedef
   struct {
      UChar* img;
      SizeT  img_szB;
      UChar* macho_img;
      SizeT  macho_img_szB;
   }
   ImageInfo;


Bool is_macho_object_file( const void* buf, SizeT szB )
{


   const struct fat_header*  fh_be = buf;
   const struct mach_header_64* mh    = buf;

   assert(buf);
   if (szB < sizeof(struct fat_header))
      return False;
   if (ntohl(fh_be->magic) == FAT_MAGIC)
      return True;

   if (szB < sizeof(struct mach_header_64))
      return False;
   if (mh->magic == MH_MAGIC_64)
      return True;

   return False;
}


static void unmap_image ( ImageInfo* ii )
{
   Int r;
   assert(ii->img);
   assert(ii->img_szB > 0);
   r = munmap( ii->img, ii->img_szB );
   assert( !r );
   memset(ii, 0, sizeof(*ii));
}


static Int map_image_aboard ( ImageInfo* ii, HChar* filename )
{
   memset(ii, 0, sizeof(*ii));

   
   { SizeT  size;
     Int r, fd;
     struct stat stat_buf;

     r = stat(filename, &stat_buf);
     if (r)
        fail("Can't stat image (to determine its size)?!");
     size = stat_buf.st_size;

     fd = open(filename, O_RDWR, 0);
     if (fd == -1)
        fail("Can't open image for possible modification!");
     if (DEBUGPRINTING)
        printf("size %lu fd %d\n", size, fd);
     void* v = mmap ( NULL, size, PROT_READ|PROT_WRITE,
                                  MAP_FILE|MAP_SHARED, fd, 0 );
     if (v == MAP_FAILED) {
        perror("mmap failed");
        fail("Can't mmap image for possible modification!");
     }

     close(fd);

     ii->img     = (UChar*)v;
     ii->img_szB = size;
   }

   { struct fat_header*  fh_be;
     struct fat_header   fh;
     struct mach_header_64* mh;
     
     
     
     ii->macho_img     = ii->img;
     ii->macho_img_szB = ii->img_szB;

     
     if (ii->img_szB < sizeof(struct fat_header))
        fail("Invalid Mach-O file (0 too small).");

     
     fh_be = (struct fat_header *)ii->img;
     fh.magic = ntohl(fh_be->magic);
     fh.nfat_arch = ntohl(fh_be->nfat_arch);
     if (fh.magic == FAT_MAGIC) {
        
        struct fat_arch *arch_be;
        struct fat_arch arch;
        Int f;
        if (ii->img_szB < sizeof(struct fat_header)
                          + fh.nfat_arch * sizeof(struct fat_arch))
           fail("Invalid Mach-O file (1 too small).");

        for (f = 0, arch_be = (struct fat_arch *)(fh_be+1); 
             f < fh.nfat_arch;
             f++, arch_be++) {
           Int cputype;
#          if defined(PLAT_x86_darwin)
           cputype = CPU_TYPE_X86;
#          elif defined(PLAT_amd64_darwin)
           cputype = CPU_TYPE_X86_64;
#          else
#            error "unknown architecture"
#          endif
           arch.cputype    = ntohl(arch_be->cputype);
           arch.cpusubtype = ntohl(arch_be->cpusubtype);
           arch.offset     = ntohl(arch_be->offset);
           arch.size       = ntohl(arch_be->size);
           if (arch.cputype == cputype) {
              if (ii->img_szB < arch.offset + arch.size)
                 fail("Invalid Mach-O file (2 too small).");
              ii->macho_img     = ii->img + arch.offset;
              ii->macho_img_szB = arch.size;
              break;
           }
        }
        if (f == fh.nfat_arch)
           fail("No acceptable architecture found in fat file.");
     }

     

     
     assert(ii->img_szB >= sizeof(struct fat_header));

     if (ii->macho_img_szB < sizeof(struct mach_header_64))
        fail("Invalid Mach-O file (3 too small).");

     if (ii->macho_img_szB > ii->img_szB)
        fail("Invalid Mach-O file (thin bigger than fat).");

     if (ii->macho_img >= ii->img
         && ii->macho_img + ii->macho_img_szB <= ii->img + ii->img_szB) {
        
     } else {
        fail("Invalid Mach-O file (thin not inside fat).");
     }

     mh = (struct mach_header_64 *)ii->macho_img;
     if (mh->magic == MH_MAGIC) {
        assert(ii->img);
        assert(ii->macho_img);
        assert(ii->img_szB > 0);
        assert(ii->macho_img_szB > 0);
        assert(ii->macho_img >= ii->img);
        assert(ii->macho_img + ii->macho_img_szB <= ii->img + ii->img_szB);
        return 32;
     }
     if (mh->magic != MH_MAGIC_64)
        fail("Invalid Mach-O file (bad magic).");

     if (ii->macho_img_szB < sizeof(struct mach_header_64) + mh->sizeofcmds)
        fail("Invalid Mach-O file (4 too small).");
   }

   assert(ii->img);
   assert(ii->macho_img);
   assert(ii->img_szB > 0);
   assert(ii->macho_img_szB > 0);
   assert(ii->macho_img >= ii->img);
   assert(ii->macho_img + ii->macho_img_szB <= ii->img + ii->img_szB);
   return 64;
}



void modify_macho_loadcmds ( HChar* filename,
                             ULong  expected_stack_start,
                             ULong  expected_stack_size )
{
   ImageInfo ii;
   memset(&ii, 0, sizeof(ii));

   Int size = map_image_aboard( &ii, filename );
   if (size == 32) {
      fprintf(stderr, "fixup_macho_loadcmds:   Is 32-bit MachO file;"
              " no modifications needed.\n");
      goto out;
   }

   assert(size == 64);

   assert(ii.macho_img != NULL && ii.macho_img_szB > 0);


   
   ULong init_rsp = 0;
   Bool  have_rsp = False;
   struct segment_command_64* seg__unixstack = NULL;
   struct segment_command_64* seg__linkedit  = NULL;
   struct segment_command_64* seg__pagezero  = NULL;

   

   { struct mach_header_64 *mh = (struct mach_header_64 *)ii.macho_img;
      struct load_command *cmd;
      Int c;

      for (c = 0, cmd = (struct load_command *)(mh+1);
           c < mh->ncmds;
           c++, cmd = (struct load_command *)(cmd->cmdsize
                                              + (unsigned long)cmd)) {
         if (DEBUGPRINTING)
            printf("load cmd: offset %4lu   size %3d   kind %2d = ",
                   (unsigned long)((UChar*)cmd - (UChar*)ii.macho_img),
                   cmd->cmdsize, cmd->cmd);

         switch (cmd->cmd) {
            case LC_SEGMENT_64:
               if (DEBUGPRINTING)
                  printf("LC_SEGMENT_64");
               break;
            case LC_SYMTAB:
               if (DEBUGPRINTING)
                  printf("LC_SYMTAB");
               break;
            case LC_DYSYMTAB:
               if (DEBUGPRINTING)
                  printf("LC_DYSYMTAB");
               break;
            case LC_UUID:
               if (DEBUGPRINTING)
                  printf("LC_UUID");
               break;
            case LC_UNIXTHREAD:
               if (DEBUGPRINTING)
                  printf("LC_UNIXTHREAD");
               break;
            default:
               if (DEBUGPRINTING)
                  printf("???");
               fail("unexpected load command in Mach header");
            break;
         }
         if (DEBUGPRINTING)
            printf("\n");

         if (cmd->cmd == LC_UNIXTHREAD) {
            struct thread_command* tcmd = (struct thread_command*)cmd;
            UInt* w32s = (UInt*)( (UChar*)tcmd + sizeof(*tcmd) );
            if (DEBUGPRINTING)
               printf("UnixThread: flavor %u = ", w32s[0]);
            if (w32s[0] == x86_THREAD_STATE64 && !have_rsp) {
               if (DEBUGPRINTING)
                  printf("x86_THREAD_STATE64\n");
               x86_thread_state64_t* state64
                  = (x86_thread_state64_t*)(&w32s[2]);
               have_rsp = True;
               init_rsp = state64->__rsp;
               if (DEBUGPRINTING)
                  printf("rsp = 0x%llx\n", init_rsp);
            } else {
               if (DEBUGPRINTING)
                  printf("???");
            }
            if (DEBUGPRINTING)
               printf("\n");
         }

         if (cmd->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *seg = (struct segment_command_64 *)cmd;
            if (0 == strcmp(seg->segname, "__LINKEDIT"))
               seg__linkedit = seg;
            if (0 == strcmp(seg->segname, "__UNIXSTACK"))
               seg__unixstack = seg;
            if (0 == strcmp(seg->segname, "__PAGEZERO"))
               seg__pagezero = seg;
         }

      }
   }

   if (!have_rsp)
      fail("Can't find / check initial RSP setting");
   if (init_rsp != expected_stack_start + expected_stack_size)
      fail("Initial RSP value not as expected");

   fprintf(stderr, "fixup_macho_loadcmds:   "
                   "initial RSP is as expected (0x%llx)\n",
                   expected_stack_start + expected_stack_size );

   if (seg__unixstack) {
      struct segment_command_64 *seg = seg__unixstack;
      if (seg->vmaddr != expected_stack_start)
         fail("has __UNIXSTACK, but wrong ::vmaddr");
      if (seg->vmsize != expected_stack_size)
         fail("has __UNIXSTACK, but wrong ::vmsize");
      if (seg->maxprot != 7)
         fail("has __UNIXSTACK, but wrong ::maxprot (should be 7)");
      if (seg->initprot != 3)
         fail("has __UNIXSTACK, but wrong ::initprot (should be 3)");
      if (seg->nsects != 0)
         fail("has __UNIXSTACK, but wrong ::nsects (should be 0)");
      if (seg->flags != 0)
         fail("has __UNIXSTACK, but wrong ::flags (should be 0)");
      
      fprintf(stderr, "fixup_macho_loadcmds:   "
              "acceptable __UNIXSTACK present; no modifications.\n" );
      goto maybe_mash_pagezero;
   }

   if (seg__linkedit) {
      struct segment_command_64 *seg = seg__linkedit;
      if (seg->nsects != 0)
         fail("has __LINKEDIT, but wrong ::nsects (should be 0)");
      if (seg->flags != 0)
         fail("has __LINKEDIT, but wrong ::flags (should be 0)");
      fprintf(stderr, "fixup_macho_loadcmds:   "
              "no __UNIXSTACK present.\n" );
      fprintf(stderr, "fixup_macho_loadcmds:   "
              "converting __LINKEDIT to __UNIXSTACK.\n" );
      strcpy(seg->segname, "__UNIXSTACK");
      seg->vmaddr   = expected_stack_start;
      seg->vmsize   = expected_stack_size;
      seg->fileoff  = 0;
      seg->filesize = 0;
      seg->maxprot  = 7;
      seg->initprot = 3;
      
      goto maybe_mash_pagezero;
   }

   
   fail("no __UNIXSTACK found and no usable __LINKEDIT found; "
        "out of options.");
   

  maybe_mash_pagezero:
   
#  if DARWIN_VERS == DARWIN_10_10
   assert(size == 64);
   if (!seg__pagezero) {
      fail("Can't find __PAGEZERO to modify; can't continue.");
   }
   fprintf(stderr, "fixup_macho_loadcmds:   "
           "changing __PAGEZERO.vmaddr from %p to 0x0.\n",
           (void*)seg__pagezero->vmaddr);
   seg__pagezero->vmaddr = 0;
#  endif

  out:   
   if (ii.img)
      unmap_image(&ii);
}


static Bool is_plausible_tool_exe_name ( HChar* nm )
{
   HChar* p;
   if (!nm)
      return False;

   
   p = strstr(nm, "-x86-darwin");
   if (p && 0 == strcmp(p, "-x86-darwin"))
      return True;

   p = strstr(nm, "-amd64-darwin");
   if (p && 0 == strcmp(p, "-amd64-darwin"))
      return True;

   return False;
}


int main ( int argc, char** argv )
{
   Int   r;
   ULong req_stack_addr = 0;
   ULong req_stack_size = 0;

   if (argc != 4)
      fail("args: -stack_addr-arg -stack_size-arg "
           "name-of-tool-executable-to-modify"); 

   r= sscanf(argv[1], "0x%llx", &req_stack_addr);
   if (r != 1) fail("invalid stack_addr arg");

   r= sscanf(argv[2], "0x%llx", &req_stack_size);
   if (r != 1) fail("invalid stack_size arg");

   fprintf(stderr, "fixup_macho_loadcmds: "
           "requested stack_addr (top) 0x%llx, "
           "stack_size 0x%llx\n", req_stack_addr, req_stack_size );

   if (!is_plausible_tool_exe_name(argv[3]))
      fail("implausible tool exe name -- not of the form *-{x86,amd64}-darwin");

   fprintf(stderr, "fixup_macho_loadcmds: examining tool exe: %s\n", 
           argv[3] );
   modify_macho_loadcmds( argv[3], req_stack_addr - req_stack_size,
                          req_stack_size );

   return 0;
}



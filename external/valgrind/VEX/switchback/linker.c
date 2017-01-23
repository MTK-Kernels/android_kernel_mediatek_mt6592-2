

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <elf.h>
#include <fcntl.h>
#include <string.h>

#include "linker.h"

#include "../pub/libvex_basictypes.h"

#if 0
#define IF_DEBUG(x,y) 
static int debug_linker = 0;
#endif


#if defined(__x86_64__)
#   define x86_64_TARGET_ARCH
#elif defined(__i386__)
#   define i386_TARGET_ARCH
#elif defined (__powerpc__)
#   define ppc32_TARGET_ARCH
#elif defined(__aarch64__)
#   define arm64_TARGET_ARCH
#else
#   error "Unknown arch"
#endif


#if 0
#define CALLOC_MAX 10000000
static HChar calloc_area[CALLOC_MAX];
static UInt calloc_used = 0;
static void* calloc_below2G ( Int n, Int m )
{
   void* p;
   int i;
   while ((calloc_used % 16) > 0) calloc_used++;
   assert(calloc_used + n*m < CALLOC_MAX);
   p = &calloc_area[calloc_used];
   for (i = 0; i < n*m; i++)
     calloc_area[calloc_used+i] = 0;
   calloc_used += n*m;
   return p;
}
#endif

#define MYMALLOC_MAX 50*1000*1000
static HChar mymalloc_area[MYMALLOC_MAX];
static UInt  mymalloc_used = 0;
void* mymalloc ( Int n )
{
   void* p;
#if defined(__powerpc64__) || defined(__aarch64__)
   while ((ULong)(mymalloc_area+mymalloc_used) & 0xFFF)
#else
   while ((UInt)(mymalloc_area+mymalloc_used) & 0xFFF)
#endif
      mymalloc_used++;
   assert(mymalloc_used+n < MYMALLOC_MAX);
   p = (void*)(&mymalloc_area[mymalloc_used]);
   mymalloc_used += n;
   
   return p;
}

void myfree ( void* p )
{
}







#if 0

#define FALSE 0
#define TRUE  1

typedef enum { OBJECT_LOADED, OBJECT_RESOLVED } OStatus;


#define N_FIXUP_PAGES 1


typedef 
   enum { SECTIONKIND_CODE_OR_RODATA,
          SECTIONKIND_RWDATA,
          SECTIONKIND_OTHER,
          SECTIONKIND_NOINFOAVAIL } 
   SectionKind;

typedef 
   struct _Section { 
      void* start; 
      void* end; 
      SectionKind kind;
      struct _Section* next;
   } 
   Section;

typedef 
   struct _ProddableBlock {
      void* start;
      int   size;
      struct _ProddableBlock* next;
   }
   ProddableBlock;

typedef struct _ObjectCode {
    OStatus    status;
    char*      fileName;
    int        fileSize;
    char*      formatName;            

    char**     symbols;
    int        n_symbols;

    
    void*      image;

    
    char*      fixup;
    int        fixup_used;
    int        fixup_size;

    Section* sections;

    
     void* lochash;
    
    
    struct _ObjectCode * next;

    ProddableBlock* proddables;

} ObjectCode;


#if VEX_HOST_WORDSIZE == 8
#define ELFCLASS    ELFCLASS64
#define Elf_Addr    Elf64_Addr
#define Elf_Word    Elf64_Word
#define Elf_Sword   Elf64_Sword
#define Elf_Ehdr    Elf64_Ehdr
#define Elf_Phdr    Elf64_Phdr
#define Elf_Shdr    Elf64_Shdr
#define Elf_Sym     Elf64_Sym
#define Elf_Rel     Elf64_Rel
#define Elf_Rela    Elf64_Rela
#define ELF_ST_TYPE ELF64_ST_TYPE
#define ELF_ST_BIND ELF64_ST_BIND
#define ELF_R_TYPE  ELF64_R_TYPE
#define ELF_R_SYM   ELF64_R_SYM
#else
#define ELFCLASS    ELFCLASS32
#define Elf_Addr    Elf32_Addr
#define Elf_Word    Elf32_Word
#define Elf_Sword   Elf32_Sword
#define Elf_Ehdr    Elf32_Ehdr
#define Elf_Phdr    Elf32_Phdr
#define Elf_Shdr    Elf32_Shdr
#define Elf_Sym     Elf32_Sym
#define Elf_Rel     Elf32_Rel
#define Elf_Rela    Elf32_Rela
#ifndef ELF_ST_TYPE
#define ELF_ST_TYPE ELF32_ST_TYPE
#endif
#ifndef ELF_ST_BIND
#define ELF_ST_BIND ELF32_ST_BIND
#endif
#ifndef ELF_R_TYPE
#define ELF_R_TYPE  ELF32_R_TYPE
#endif
#ifndef ELF_R_SYM
#define ELF_R_SYM   ELF32_R_SYM
#endif
#endif





static void addProddableBlock ( ObjectCode* oc, void* start, int size )
{
   ProddableBlock* pb
      = mymalloc(sizeof(ProddableBlock));
   if (debug_linker)
      fprintf(stderr, "aPB oc=%p %p %d   (%p .. %p)\n", oc, start, size,
	      start, ((char*)start)+size-1 );
   assert(size > 0);
   pb->start      = start;
   pb->size       = size;
   pb->next       = oc->proddables;
   oc->proddables = pb;
}

static void checkProddableBlock ( ObjectCode* oc, void* addr )
{
   ProddableBlock* pb;
   for (pb = oc->proddables; pb != NULL; pb = pb->next) {
      char* s = (char*)(pb->start);
      char* e = s + pb->size - 1;
      char* a = (char*)addr;
      if (a >= s && (a+3) <= e) return;
   }
   fprintf(stderr,
           "checkProddableBlock: invalid fixup %p in runtime linker\n",
           addr);
   exit(1);
}




typedef 
   struct { char* mp_name; void* mp_addr; } 
   Maplet;

typedef
   struct {
      int sm_size;
      int sm_used;
      Maplet* maplets;
   }
   StringMap;

static StringMap* new_StringMap ( void )
{
   StringMap* sm = mymalloc(sizeof(StringMap));
   sm->sm_size = 10;
   sm->sm_used = 0;
   sm->maplets = mymalloc(10 * sizeof(Maplet));
   return sm;
}

static void delete_StringMap ( StringMap* sm )
{
   assert(sm->maplets != NULL);
   myfree(sm->maplets);
   sm->maplets = NULL;
   myfree(sm);
}

static void ensure_StringMap ( StringMap* sm )
{
   int i;
   Maplet* mp2;
   assert(sm->maplets != NULL);
   if (sm->sm_used < sm->sm_size)
     return;
   sm->sm_size *= 2;
   mp2 = mymalloc(sm->sm_size * sizeof(Maplet));
   for (i = 0; i < sm->sm_used; i++)
      mp2[i] = sm->maplets[i];
   myfree(sm->maplets);
   sm->maplets = mp2;
}

static void* search_StringMap ( StringMap* sm, char* name )
{
   int i;
   for (i = 0; i < sm->sm_used; i++)
      if (0 == strcmp(name, sm->maplets[i].mp_name))
         return sm->maplets[i].mp_addr;
   return NULL;
}

static void addto_StringMap ( StringMap* sm, char* name, void* addr )
{
   ensure_StringMap(sm);
   sm->maplets[sm->sm_used].mp_name = name;
   sm->maplets[sm->sm_used].mp_addr = addr;
   sm->sm_used++;
}

static void paranoid_addto_StringMap ( StringMap* sm, char* name, void* addr )
{
   if (0)
       fprintf(stderr, "paranoid_addto_StringMap(%s,%p)\n", name, addr);
   if (search_StringMap(sm,name) != NULL) {
      fprintf(stderr, "duplicate: paranoid_addto_StringMap(%s,%p)\n", name, addr);
      exit(1);
   }
   addto_StringMap(sm,name,addr);
}



StringMap*  global_symbol_table = NULL;
ObjectCode* global_object_list = NULL;

static void initLinker ( void )
{
   if (global_symbol_table != NULL)
      return;
   global_symbol_table = new_StringMap();
}




static 
void * lookupSymbol( char *lbl )
{
   void *val;
   initLinker() ;
   assert(global_symbol_table != NULL);
   val = search_StringMap(global_symbol_table, lbl);
   return val;
}




static char *
findElfSection ( void* objImage, Elf_Word sh_type )
{
   char* ehdrC = (char*)objImage;
   Elf_Ehdr* ehdr = (Elf_Ehdr*)ehdrC;
   Elf_Shdr* shdr = (Elf_Shdr*)(ehdrC + ehdr->e_shoff);
   char* sh_strtab = ehdrC + shdr[ehdr->e_shstrndx].sh_offset;
   char* ptr = NULL;
   int i;

   for (i = 0; i < ehdr->e_shnum; i++) {
      if (shdr[i].sh_type == sh_type
          
          && i != ehdr->e_shstrndx
          && 0 != memcmp(".stabstr", sh_strtab + shdr[i].sh_name, 8)
         ) {
         ptr = ehdrC + shdr[i].sh_offset;
         break;
      }
   }
   return ptr;
}

#ifdef arm_TARGET_ARCH
static
char* alloc_fixup_bytes ( ObjectCode* oc, int nbytes )
{
   char* res;
   assert(nbytes % 4 == 0);
   assert(nbytes > 0);
   res = &(oc->fixup[oc->fixup_used]);
   oc->fixup_used += nbytes;
   if (oc->fixup_used >= oc->fixup_size) {
     fprintf(stderr, "fixup area too small for %s\n", oc->fileName);
     exit(1);
   }
   return res;
}
#endif



static
void* lookup_magic_hacks ( char* sym )
{
   if (0==strcmp(sym, "printf")) return (void*)(&printf);
   return NULL;
}

#ifdef arm_TARGET_ARCH
static
void arm_notify_new_code ( char* start, int length )
{
  __asm __volatile ("mov r1, %0\n\t"
                    "mov r2, %1\n\t"
                    "mov r3, %2\n\t"
                    "swi 0x9f0002\n\t"
                    : 
                    : "ir" (start), "ir" (length), "ir" (0) );
}


static
void gen_armle_goto ( char* fixup, char* dstP )
{
  Elf_Word w = (Elf_Word)dstP;
  fprintf(stderr,"at %p generating jump to %p\n", fixup, dstP );
  fixup[0] = 0x04; fixup[1] = 0xF0; fixup[2] = 0x1F; fixup[3] = 0xE5;
  fixup[4] = w & 0xFF; w >>= 8;
  fixup[5] = w & 0xFF; w >>= 8;
  fixup[6] = w & 0xFF; w >>= 8;
  fixup[7] = w & 0xFF; w >>= 8;
  arm_notify_new_code(fixup, 8);
}
#endif 


#ifdef ppc32_TARGET_ARCH
static void invalidate_icache(void *ptr, int nbytes)
{
   unsigned long startaddr = (unsigned long) ptr;
   unsigned long endaddr = startaddr + nbytes;
   unsigned long addr;
   unsigned long cls = 16; 

   startaddr &= ~(cls - 1);
   for (addr = startaddr; addr < endaddr; addr += cls)
      asm volatile("dcbst 0,%0" : : "r" (addr));
   asm volatile("sync");
   for (addr = startaddr; addr < endaddr; addr += cls)
      asm volatile("icbi 0,%0" : : "r" (addr));
   asm volatile("sync; isync");
}

static UInt compute_ppc_HA ( UInt x ) {
   return 0xFFFF & ( (x >> 16) + ((x & 0x8000) ? 1 : 0) );
}
static UInt compute_ppc_LO ( UInt x ) {
   return 0xFFFF & x;
}
static UInt compute_ppc_HI ( UInt x ) {
   return 0xFFFF & (x >> 16);
}
#endif 


static int
do_Elf_Rel_relocations ( ObjectCode* oc, char* ehdrC,
                         Elf_Shdr* shdr, int shnum,
                         Elf_Sym*  stab, char* strtab )
{
   int j;
   char *symbol = NULL;
   Elf_Word* targ;
   Elf_Rel*  rtab = (Elf_Rel*) (ehdrC + shdr[shnum].sh_offset);
   int         nent = shdr[shnum].sh_size / sizeof(Elf_Rel);
   int target_shndx = shdr[shnum].sh_info;
   int symtab_shndx = shdr[shnum].sh_link;

   stab  = (Elf_Sym*) (ehdrC + shdr[ symtab_shndx ].sh_offset);
   targ  = (Elf_Word*)(ehdrC + shdr[ target_shndx ].sh_offset);
   IF_DEBUG(linker,belch( "relocations for section %d using symtab %d",
                          target_shndx, symtab_shndx ));

   for (j = 0; j < nent; j++) {
      Elf_Addr offset = rtab[j].r_offset;
      Elf_Addr info   = rtab[j].r_info;

      Elf_Addr  P  = ((Elf_Addr)targ) + offset;
      Elf_Word* pP = (Elf_Word*)P;
      Elf_Addr  A  = *pP;
      Elf_Addr  S;
      Elf_Addr  value;

      IF_DEBUG(linker,belch( "Rel entry %3d is raw(%6p %6p)",
                             j, (void*)offset, (void*)info ));
      if (!info) {
         IF_DEBUG(linker,belch( " ZERO" ));
         S = 0;
      } else {
         Elf_Sym sym = stab[ELF_R_SYM(info)];
	 
         if (ELF_ST_BIND(sym.st_info) == STB_LOCAL) {
            symbol = sym.st_name==0 ? "(noname)" : strtab+sym.st_name;
            S = (Elf_Addr)
                (ehdrC + shdr[ sym.st_shndx ].sh_offset
                       + stab[ELF_R_SYM(info)].st_value);

	 } else {
            
            symbol = strtab + sym.st_name;
            S = (Elf_Addr)lookupSymbol( symbol );
	 }
         if (!S) {
            S = (Elf_Addr)lookup_magic_hacks(symbol);
         }
         if (!S) {
            fprintf(stderr,"%s: unknown symbol `%s'\n", 
                           oc->fileName, symbol);
	    return 0;
         }
         if (debug_linker>1) 
            fprintf(stderr, "\n`%s' resolves to %p\n", symbol, (void*)S );
      }

      if (debug_linker>1)
         fprintf(stderr, "Reloc: P = %p   S = %p   A = %p\n",
			     (void*)P, (void*)S, (void*)A );
      checkProddableBlock ( oc, pP );

      value = S + A;

      switch (ELF_R_TYPE(info)) {
#        ifdef i386_TARGET_ARCH
         case R_386_32:   *pP = value;     break;
         case R_386_PC32: *pP = value - P; break;
#        endif
#        ifdef arm_TARGET_ARCH
         case R_ARM_PC24: {
	    Elf_Word w, delta, deltaTop8;
 	    char* fixup = alloc_fixup_bytes(oc, 8);
            
            Elf_Word real_dst = (A & 0x00FFFFFF) + 2;
	    
            if (real_dst & 0x00800000) 
               real_dst |= 0xFF000000;
            else
               real_dst &= 0x00FFFFFF;

            real_dst <<= 2;
	    real_dst += S;

	    gen_armle_goto(fixup, (char*)real_dst);

	    
            delta = (((Elf_Word)fixup) - ((Elf_Word)pP) - 8);
            deltaTop8 = (delta >> 24) & 0xFF;
            if (deltaTop8 != 0 && deltaTop8 != 0xFF) {
	      fprintf(stderr,"R_ARM_PC24: out of range delta 0x%x for %s\n",
		      delta, symbol);
	      exit(1);
	    }
            delta >>= 2;
	    w = *pP;
            w &= 0xFF000000;
            w |= (0x00FFFFFF & delta );
            *pP = w;
	    break;
         }
         case R_ARM_ABS32:
	    *pP = value;
	    break;
#        endif
         default:
            fprintf(stderr,
                    "%s: unhandled ELF relocation(Rel) type %d\n\n",
		    oc->fileName, (Int)ELF_R_TYPE(info));
            return 0;
      }

   }
   return 1;
}

static int
do_Elf_Rela_relocations ( ObjectCode* oc, char* ehdrC,
                          Elf_Shdr* shdr, int shnum,
                          Elf_Sym*  stab, char* strtab )
{
   int j;
   char *symbol;
   Elf_Addr targ;
   Elf_Rela* rtab = (Elf_Rela*) (ehdrC + shdr[shnum].sh_offset);
   int         nent = shdr[shnum].sh_size / sizeof(Elf_Rela);
   int target_shndx = shdr[shnum].sh_info;
   int symtab_shndx = shdr[shnum].sh_link;

   stab  = (Elf_Sym*) (ehdrC + shdr[ symtab_shndx ].sh_offset);
   targ  = (Elf_Addr) (ehdrC + shdr[ target_shndx ].sh_offset);
   IF_DEBUG(linker,belch( "relocations for section %d using symtab %d",
                          target_shndx, symtab_shndx ));

   for (j = 0; j < nent; j++) {
#if defined(DEBUG) || defined(sparc_TARGET_ARCH)  \
                   || defined(ia64_TARGET_ARCH)   \
                   || defined(x86_64_TARGET_ARCH) \
                   || defined(ppc32_TARGET_ARCH)
      
      Elf_Addr  offset = rtab[j].r_offset;
      Elf_Addr  P      = targ + offset;
#endif
      Elf_Addr  info   = rtab[j].r_info;
      Elf_Addr  A      = rtab[j].r_addend;
      Elf_Addr  S =0;
      Elf_Addr  value;
#     if defined(sparc_TARGET_ARCH)
      Elf_Word* pP = (Elf_Word*)P;
      Elf_Word  w1, w2;
#     endif
#     if defined(ia64_TARGET_ARCH)
      Elf64_Xword *pP = (Elf64_Xword *)P;
      Elf_Addr addr;
#     endif
#     if defined(x86_64_TARGET_ARCH)
      ULong* pP = (ULong*)P;
#     endif
#     if defined(ppc32_TARGET_ARCH)
      Int sI, sI2;
      Elf_Word* pP = (Elf_Word*)P;
#     endif

      IF_DEBUG(linker,belch( "Rel entry %3d is raw(%6p %6p %6p)   ",
                             j, (void*)offset, (void*)info,
                                (void*)A ));
      if (!info) {
         IF_DEBUG(linker,belch( " ZERO" ));
         S = 0;
      } else {
         Elf_Sym sym = stab[ELF_R_SYM(info)];
	 
         if (ELF_ST_BIND(sym.st_info) == STB_LOCAL) {
            symbol = sym.st_name==0 ? "(noname)" : strtab+sym.st_name;
            S = (Elf_Addr)
                (ehdrC + shdr[ sym.st_shndx ].sh_offset
                       + stab[ELF_R_SYM(info)].st_value);
#ifdef ELF_FUNCTION_DESC
	    
            if (S && ELF_ST_TYPE(sym.st_info) == STT_FUNC) {
               S = allocateFunctionDesc(S + A);
       	       A = 0;
            }
#endif
	 } else {
            
            symbol = strtab + sym.st_name;
            S = (Elf_Addr)lookupSymbol( symbol );

#ifdef ELF_FUNCTION_DESC
            if (S && (ELF_ST_TYPE(sym.st_info) == STT_FUNC) && (A != 0))
               belch("%s: function %s with addend %p", oc->fileName, symbol, (void *)A);
#endif
	 }
         if (!S) {
	   fprintf(stderr,"%s: unknown symbol `%s'\n", oc->fileName, symbol);
	   return 0;
         }
         if (0)
            fprintf(stderr, "`%s' resolves to %p\n", symbol, (void*)S );
      }

#if 0
         fprintf ( stderr, "Reloc: offset = %p   P = %p   S = %p   A = %p\n",
                           (void*)offset, (void*)P, (void*)S, (void*)A );
#endif

      

      value = S + A;

      switch (ELF_R_TYPE(info)) {
#        if defined(sparc_TARGET_ARCH)
         case R_SPARC_WDISP30:
            w1 = *pP & 0xC0000000;
            w2 = (Elf_Word)((value - P) >> 2);
            ASSERT((w2 & 0xC0000000) == 0);
            w1 |= w2;
            *pP = w1;
            break;
         case R_SPARC_HI22:
            w1 = *pP & 0xFFC00000;
            w2 = (Elf_Word)(value >> 10);
            ASSERT((w2 & 0xFFC00000) == 0);
            w1 |= w2;
            *pP = w1;
            break;
         case R_SPARC_LO10:
            w1 = *pP & ~0x3FF;
            w2 = (Elf_Word)(value & 0x3FF);
            ASSERT((w2 & ~0x3FF) == 0);
            w1 |= w2;
            *pP = w1;
            break;
         case R_SPARC_UA32:
         case R_SPARC_32:
            w2 = (Elf_Word)value;
            *pP = w2;
            break;
#        endif
#        if defined(ia64_TARGET_ARCH)
	 case R_IA64_DIR64LSB:
	 case R_IA64_FPTR64LSB:
	    *pP = value;
	    break;
	 case R_IA64_PCREL64LSB:
	    *pP = value - P;
	    break;
	 case R_IA64_SEGREL64LSB:
	    addr = findElfSegment(ehdrC, value);
	    *pP = value - addr;
	    break;
	 case R_IA64_GPREL22:
	    ia64_reloc_gprel22(P, value);
	    break;
	 case R_IA64_LTOFF22:
	 case R_IA64_LTOFF22X:
	 case R_IA64_LTOFF_FPTR22:
	    addr = allocateGOTEntry(value);
	    ia64_reloc_gprel22(P, addr);
	    break;
	 case R_IA64_PCREL21B:
	    ia64_reloc_pcrel21(P, S, oc);
	    break;
	 case R_IA64_LDXMOV:
	    break;
#        endif
#        if defined(x86_64_TARGET_ARCH)
         case R_X86_64_64: 
            *((ULong*)pP) = (ULong)(S + A);
            break;
         case R_X86_64_PC32: 
            *((UInt*)pP) = (UInt)(S + A - P);
            break;
         case R_X86_64_32: 
            *((UInt*)pP) = (UInt)(S + A);
            break;
         case R_X86_64_32S: 
            *((UInt*)pP) = (UInt)(S + A);
            break;
#        endif
#        if defined(ppc32_TARGET_ARCH)
         case R_PPC_ADDR32: 
            *((UInt*)pP) = S+A;
            invalidate_icache(pP,4);
            break;
         case R_PPC_ADDR16_LO: 
            *((UInt*)pP) &= 0x0000FFFF;
            *((UInt*)pP) |= 0xFFFF0000 & (compute_ppc_LO(S+A) << 16);
            invalidate_icache(pP,4);
            break;
         case R_PPC_ADDR16_HA: 
            *((UInt*)pP) &= 0x0000FFFF;
            *((UInt*)pP) |= 0xFFFF0000 & (compute_ppc_HA(S+A) << 16);
            invalidate_icache(pP,4);
            break;
         case R_PPC_REL24: 
            sI = S+A-P;
	    sI >>= 2;
            sI2 = sI >> 23; 
            if (sI2 != 0 && sI2 != 0xFFFFFFFF) {
               fprintf(stderr, "%s: R_PPC_REL24 relocation failed\n", oc->fileName );
	       return 0;
            }
            *((UInt*)pP) &= ~(0x00FFFFFF << 2);
            *((UInt*)pP) |= (0xFFFFFF & sI) << 2;
           invalidate_icache(pP,4);
            break;
         case R_PPC_REL32: 
            *((UInt*)pP) = S+A-P;
            invalidate_icache(pP,4);
            break;
#        endif
         default:
            fprintf(stderr,
                    "%s: unhandled ELF relocation(RelA) type %d\n",
		    oc->fileName, (Int)ELF_R_TYPE(info));
            return 0;
      }

   }
   return 1;
}


static int
ocResolve_ELF ( ObjectCode* oc )
{
   char *strtab;
   int   shnum, ok;
   Elf_Sym*  stab  = NULL;
   char*     ehdrC = (char*)(oc->image);
   Elf_Ehdr* ehdr  = (Elf_Ehdr*) ehdrC;
   Elf_Shdr* shdr  = (Elf_Shdr*) (ehdrC + ehdr->e_shoff);
   char* sh_strtab = ehdrC + shdr[ehdr->e_shstrndx].sh_offset;

   
   stab = (Elf_Sym*) findElfSection ( ehdrC, SHT_SYMTAB );

   
   strtab = findElfSection ( ehdrC, SHT_STRTAB );

   if (stab == NULL || strtab == NULL) {
      fprintf(stderr,"%s: can't find string or symbol table\n", oc->fileName);
      return 0;
   }

   
   for (shnum = 0; shnum < ehdr->e_shnum; shnum++) {

      if (0 == memcmp(".rel.stab", sh_strtab + shdr[shnum].sh_name, 9))
         continue;

      if (shdr[shnum].sh_type == SHT_REL ) {
         ok = do_Elf_Rel_relocations ( oc, ehdrC, shdr,
                                       shnum, stab, strtab );
         if (!ok) return ok;
      }
      else
      if (shdr[shnum].sh_type == SHT_RELA) {
         ok = do_Elf_Rela_relocations ( oc, ehdrC, shdr,
                                        shnum, stab, strtab );
         if (!ok) return ok;
      }
   }

   
   delete_StringMap(oc->lochash);
   oc->lochash = NULL;

   return 1;
}



static int
ocVerifyImage_ELF ( ObjectCode* oc )
{
   Elf_Shdr* shdr;
   Elf_Sym*  stab;
   int i, j, nent, nstrtab, nsymtabs;
   char* sh_strtab;
   char* strtab;

   char*     ehdrC = (char*)(oc->image);
   Elf_Ehdr* ehdr  = (Elf_Ehdr*)ehdrC;

   if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
       ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
       ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
       ehdr->e_ident[EI_MAG3] != ELFMAG3) {
      fprintf(stderr,"%s: not an ELF object\n", oc->fileName);
      return 0;
   }

   if (ehdr->e_ident[EI_CLASS] != ELFCLASS) {
      fprintf(stderr,"%s: unsupported ELF format\n", oc->fileName);
      return 0;
   }

   if (ehdr->e_ident[EI_DATA] == ELFDATA2LSB) {
      if (debug_linker)
         fprintf(stderr, "Is little-endian\n" );
   } else
   if (ehdr->e_ident[EI_DATA] == ELFDATA2MSB) {
       if (debug_linker)
          fprintf(stderr, "Is big-endian\n" );
   } else {
       fprintf(stderr,"%s: unknown endiannness\n", oc->fileName);
       return 0;
   }

   if (ehdr->e_type != ET_REL) {
      fprintf(stderr,"%s: not a relocatable object (.o) file\n", oc->fileName);
      return 0;
   }
   if (debug_linker)
      fprintf(stderr, "Is a relocatable object (.o) file\n" );

   if (debug_linker)
      fprintf(stderr, "Architecture is " );
   switch (ehdr->e_machine) {
      case EM_386:    if (debug_linker) fprintf(stderr, "x86\n" ); break;
      case EM_SPARC:  if (debug_linker) fprintf(stderr, "sparc\n" ); break;
      case EM_ARM:    if (debug_linker) fprintf(stderr, "arm\n" ); break;
#ifdef EM_IA_64
      case EM_IA_64:  if (debug_linker) fprintf(stderr, "ia64\n" ); break;
#endif
      case EM_X86_64: if (debug_linker) fprintf(stderr, "x86_64\n" ); break;
      case EM_PPC:    if (debug_linker) fprintf(stderr, "ppc\n" ); break;
      default:        if (debug_linker) fprintf(stderr, "unknown\n" );
                      fprintf(stderr,"%s: unknown architecture\n", oc->fileName);
                      return 0;
   }

   if (debug_linker>1) fprintf(stderr,
             "\nSection header table: start %lld, n_entries %d, ent_size %d\n",
             (Long)ehdr->e_shoff, 
             ehdr->e_shnum, ehdr->e_shentsize  );

   assert (ehdr->e_shentsize == sizeof(Elf_Shdr));

   shdr = (Elf_Shdr*) (ehdrC + ehdr->e_shoff);

   if (ehdr->e_shstrndx == SHN_UNDEF) {
      fprintf(stderr,"%s: no section header string table\n", oc->fileName);
      return 0;
   } else {
      if (debug_linker>1) 
         fprintf(stderr, "Section header string table is section %d\n",
                          ehdr->e_shstrndx);
      sh_strtab = ehdrC + shdr[ehdr->e_shstrndx].sh_offset;
   }

   for (i = 0; i < ehdr->e_shnum; i++) {
      if (debug_linker>1) fprintf(stderr, "%2d:  ", i );
      if (debug_linker>1) fprintf(stderr, "type=%2d  ", (int)shdr[i].sh_type );
      if (debug_linker>1) fprintf(stderr, "size=%4d  ", (int)shdr[i].sh_size );
      if (debug_linker>1) fprintf(stderr, "offs=%4d  ", (int)shdr[i].sh_offset );
      if (debug_linker>1) fprintf(stderr, "  (%p .. %p)  ",
               ehdrC + shdr[i].sh_offset,
		      ehdrC + shdr[i].sh_offset + shdr[i].sh_size - 1);

      if (shdr[i].sh_type == SHT_REL) {
	  if (debug_linker>1) fprintf(stderr, "Rel  " );
      } else if (shdr[i].sh_type == SHT_RELA) {
	  if (debug_linker>1) fprintf(stderr, "RelA " );
      } else {
	  if (debug_linker>1) fprintf(stderr,"     ");
      }
      if (sh_strtab) {
	  if (debug_linker>1) fprintf(stderr, "sname=%s\n", 
             sh_strtab + shdr[i].sh_name );
      }
   }

   if (debug_linker>1) fprintf(stderr, "\nString tables\n" );
   strtab = NULL;
   nstrtab = 0;
   for (i = 0; i < ehdr->e_shnum; i++) {
      if (shdr[i].sh_type == SHT_STRTAB
          
          && i != ehdr->e_shstrndx
          && 0 != memcmp(".stabstr", sh_strtab + shdr[i].sh_name, 8)
         ) {
         if (debug_linker>1) 
            fprintf(stderr,"   section %d is a normal string table\n", i );
         strtab = ehdrC + shdr[i].sh_offset;
         nstrtab++;
      }
   }
   if (nstrtab != 1) {
      fprintf(stderr,"%s: no string tables, or too many\n", oc->fileName);
      return 0;
   }

   nsymtabs = 0;
   if (debug_linker>1) fprintf(stderr, "\nSymbol tables\n" );
   for (i = 0; i < ehdr->e_shnum; i++) {
      if (shdr[i].sh_type != SHT_SYMTAB) continue;
      if (debug_linker>1) fprintf(stderr, "section %d is a symbol table\n", i );
      nsymtabs++;
      stab = (Elf_Sym*) (ehdrC + shdr[i].sh_offset);
      nent = shdr[i].sh_size / sizeof(Elf_Sym);
      if (debug_linker>1) fprintf(stderr,  
            "   number of entries is apparently %d (%lld rem)\n",
               nent,
               (Long)(shdr[i].sh_size % sizeof(Elf_Sym))
             );
      if (0 != shdr[i].sh_size % sizeof(Elf_Sym)) {
         fprintf(stderr,"%s: non-integral number of symbol table entries\n", 
                        oc->fileName);
         return 0;
      }
      for (j = 0; j < nent; j++) {
         if (debug_linker>1) fprintf(stderr, "   %2d  ", j );
         if (debug_linker>1) fprintf(stderr, "  sec=%-5d  size=%-3d  val=%5p  ",
                             (int)stab[j].st_shndx,
                             (int)stab[j].st_size,
                             (char*)stab[j].st_value );

         if (debug_linker>1) fprintf(stderr, "type=" );
         switch (ELF_ST_TYPE(stab[j].st_info)) {
            case STT_NOTYPE:  if (debug_linker>1) fprintf(stderr, "notype " ); break;
            case STT_OBJECT:  if (debug_linker>1) fprintf(stderr, "object " ); break;
            case STT_FUNC  :  if (debug_linker>1) fprintf(stderr, "func   " ); break;
            case STT_SECTION: if (debug_linker>1) fprintf(stderr, "section" ); break;
            case STT_FILE:    if (debug_linker>1) fprintf(stderr, "file   " ); break;
            default:          if (debug_linker>1) fprintf(stderr, "?      " ); break;
         }
         if (debug_linker>1) fprintf(stderr, "  " );

         if (debug_linker>1) fprintf(stderr, "bind=" );
         switch (ELF_ST_BIND(stab[j].st_info)) {
            case STB_LOCAL :  if (debug_linker>1) fprintf(stderr, "local " ); break;
            case STB_GLOBAL:  if (debug_linker>1) fprintf(stderr, "global" ); break;
            case STB_WEAK  :  if (debug_linker>1) fprintf(stderr, "weak  " ); break;
            default:          if (debug_linker>1) fprintf(stderr, "?     " ); break;
         }
         if (debug_linker>1) fprintf(stderr, "  " );

         if (debug_linker>1) fprintf(stderr, "name=%s\n", strtab + stab[j].st_name );
      }
   }

   if (nsymtabs == 0) {
      fprintf(stderr,"%s: didn't find any symbol tables\n", oc->fileName);
      return 0;
   }

   return 1;
}



static int
ocGetNames_ELF ( ObjectCode* oc )
{
   int i, j, k, nent;
   Elf_Sym* stab;

   char*     ehdrC     = (char*)(oc->image);
   Elf_Ehdr* ehdr      = (Elf_Ehdr*)ehdrC;
   char*     strtab    = findElfSection ( ehdrC, SHT_STRTAB );
   Elf_Shdr* shdr      = (Elf_Shdr*) (ehdrC + ehdr->e_shoff);

   char*     sh_strtab = ehdrC + shdr[ehdr->e_shstrndx].sh_offset;
   char*     sec_name;

   assert(global_symbol_table != NULL);

   if (!strtab) {
      fprintf(stderr,"%s: no strtab\n", oc->fileName);
      return 0;
   }

   k = 0;
   for (i = 0; i < ehdr->e_shnum; i++) {
      Elf_Shdr    hdr    = shdr[i];
      SectionKind kind   = SECTIONKIND_OTHER;
      int         is_bss = FALSE;

      if (hdr.sh_type == SHT_PROGBITS
          && (hdr.sh_flags & SHF_ALLOC) && (hdr.sh_flags & SHF_EXECINSTR)) {
         
         kind = SECTIONKIND_CODE_OR_RODATA;
      }
      else
      if (hdr.sh_type == SHT_PROGBITS
          && (hdr.sh_flags & SHF_ALLOC) && (hdr.sh_flags & SHF_WRITE)) {
         
         kind = SECTIONKIND_RWDATA;
      }
      else
      if (hdr.sh_type == SHT_PROGBITS
          && (hdr.sh_flags & SHF_ALLOC) && !(hdr.sh_flags & SHF_WRITE)) {
         
         kind = SECTIONKIND_CODE_OR_RODATA;
      }
      else
      if (hdr.sh_type == SHT_NOBITS
          && (hdr.sh_flags & SHF_ALLOC) && (hdr.sh_flags & SHF_WRITE)) {
         
         kind = SECTIONKIND_RWDATA;
         is_bss = TRUE;
      }

      if (is_bss && shdr[i].sh_size > 0) {
         char* zspace = calloc(1, shdr[i].sh_size);
         shdr[i].sh_offset = ((char*)zspace) - ((char*)ehdrC);
	 if (1)
         fprintf(stderr, "BSS section at %p, size %lld\n",
                         zspace, (Long)shdr[i].sh_size);
      }

      
      sec_name = sh_strtab + shdr[i].sh_name;
      if (kind == SECTIONKIND_OTHER
          && (0 == strcmp(".debug_info", sec_name)
              || 0 == strcmp(".debug_line", sec_name)
              || 0 == strcmp(".debug_pubnames", sec_name)
              || 0 == strcmp(".debug_aranges", sec_name)
              || 0 == strcmp(".debug_frame", sec_name))) {
         kind = SECTIONKIND_CODE_OR_RODATA;
      }

      
      if (kind != SECTIONKIND_OTHER && shdr[i].sh_size > 0) {
         addProddableBlock(oc, ehdrC + shdr[i].sh_offset, shdr[i].sh_size);
         
         
      }

      if (shdr[i].sh_type != SHT_SYMTAB) continue;

      
      stab = (Elf_Sym*) (ehdrC + shdr[i].sh_offset);
      nent = shdr[i].sh_size / sizeof(Elf_Sym);

      oc->n_symbols = nent;
      oc->symbols = mymalloc(oc->n_symbols * sizeof(char*));

      for (j = 0; j < nent; j++) {

         char  isLocal = FALSE; 
         char* ad      = NULL;
         char* nm      = strtab + stab[j].st_name;
         int   secno   = stab[j].st_shndx;


         if (secno == SHN_COMMON) {
            isLocal = FALSE;
#           if defined(__x86_64__)
            ad = calloc_below2G(1, stab[j].st_size);
#           else
            ad = calloc(1, stab[j].st_size);
#           endif
    

	    if (0)
            fprintf(stderr, "COMMON symbol, size %lld name %s  allocd %p\n",
                            (Long)stab[j].st_size, nm, ad);
	 }
         else
         if ( ( ELF_ST_BIND(stab[j].st_info)==STB_GLOBAL
                || ELF_ST_BIND(stab[j].st_info)==STB_LOCAL
              )
              
              && stab[j].st_shndx != SHN_UNDEF
	      
              && stab[j].st_shndx < SHN_LORESERVE
              &&
	      
              ( ELF_ST_TYPE(stab[j].st_info)==STT_FUNC ||
                ELF_ST_TYPE(stab[j].st_info)==STT_OBJECT ||
                ELF_ST_TYPE(stab[j].st_info)==STT_NOTYPE
              )
            ) {
	    
            assert(secno > 0 && secno < ehdr->e_shnum);
            ad = ehdrC + shdr[ secno ].sh_offset + stab[j].st_value;
            if (ELF_ST_BIND(stab[j].st_info)==STB_LOCAL) {
               isLocal = TRUE;
            } else {
#ifdef ELF_FUNCTION_DESC
               if (ELF_ST_TYPE(stab[j].st_info) == STT_FUNC)
                   ad = (char *)allocateFunctionDesc((Elf_Addr)ad);
#endif
               if (0|| debug_linker) 
                   fprintf(stderr, "addOTabName(GLOB): %10p  %s %s\n",
                                      ad, oc->fileName, nm );
               isLocal = FALSE;
            }
         }

         

         if (ad != NULL) {
            assert(nm != NULL);
	    oc->symbols[j] = nm;
            
            if (isLocal) {
               
            } else {
	      
	      paranoid_addto_StringMap(global_symbol_table, nm, ad);
            }
         } else {
            
            if (debug_linker>1) fprintf(stderr, "skipping `%s'\n",
                                   strtab + stab[j].st_name );
            oc->symbols[j] = NULL;
         }

      }
   }

   return 1;
}




static
int loadObj( char *path )
{
   ObjectCode* oc;
   struct stat st;
   int r;
   int fd, pagesize;
   char* p;

   initLinker();

   fprintf(stderr, "==== loadObj %s ====\n", path );

   
   {
       ObjectCode *o;
       int is_dup = 0;
       for (o = global_object_list; o; o = o->next) {
          if (0 == strcmp(o->fileName, path))
             is_dup = 1;
       }
       if (is_dup) {
	 fprintf(stderr,
            "\n\n"
            "GHCi runtime linker: warning: looks like you're trying to load the\n"
            "same object file twice:\n"
            "   %s\n"
            , path);
	 exit(1);
       }
   }

   oc = mymalloc(sizeof(ObjectCode));

   oc->formatName = "ELF";

   r = stat(path, &st);
   if (r == -1) { return 0; }

   
   oc->fileName = mymalloc( strlen(path)+1 );
   strcpy(oc->fileName, path);

   oc->fileSize          = st.st_size;
   oc->symbols           = NULL;
   oc->sections          = NULL;
   oc->lochash           = new_StringMap();
   oc->proddables        = NULL;
   oc->fixup             = NULL;
   oc->fixup_used        = 0;
   oc->fixup_size        = 0;

   
   oc->next              = global_object_list;
   global_object_list    = oc;

   fd = open(path, O_RDONLY);
   if (fd == -1) {
      fprintf(stderr,"loadObj: can't open `%s'\n", path);
      exit(1);
   }


   pagesize = getpagesize();
   
   
   p = mymalloc(N_FIXUP_PAGES * pagesize + oc->fileSize);
   if (0) fprintf(stderr,"XXXX p = %p\n", p);
   if (p == NULL) {
      fprintf(stderr,"loadObj: failed to allocate space for `%s'\n", path);
      exit(1);
   }

   oc->fixup = p;
   oc->fixup_size = N_FIXUP_PAGES * pagesize;
   oc->fixup_used = 0;
   oc->image = &(p[ oc->fixup_size ]);

   r = read(fd, oc->image, oc->fileSize);
   if (r != oc->fileSize) {
      fprintf(stderr,"loadObj: failed to read `%s'\n", path);
      exit(1);
   }

   fprintf(stderr, "loaded %s at %p (fixup = %p)\n", 
                   oc->fileName, oc->image, oc->fixup );

   close(fd);

   
   r = ocVerifyImage_ELF ( oc );
   if (!r) { return r; }

   
   r = ocGetNames_ELF ( oc );
   if (!r) { return r; }

   
   oc->status = OBJECT_LOADED;

#ifdef ppc32_TARGET_ARCH
   invalidate_icache(oc->image, oc->fileSize);
#endif

   return 1;
}



static
int resolveObjs( void )
{
    ObjectCode *oc;
    int r;

    initLinker();

    for (oc = global_object_list; oc; oc = oc->next) {
	if (oc->status != OBJECT_RESOLVED) {
	    r = ocResolve_ELF ( oc );
	    if (!r) { return r; }
	    oc->status = OBJECT_RESOLVED;
	}
    }
    return 1;
}



void* linker_top_level_LINK ( int n_object_names, char** object_names )
{
   int   r, i;
   void* mainp;

   initLinker();
   for (i = 0; i < n_object_names; i++) {
      
      r = loadObj( object_names[i] );
      if (r != 1) return NULL;
   }
   r = resolveObjs();
   if (r != 1) return NULL;
   mainp = search_StringMap ( global_symbol_table, "entry" );
   if (mainp == NULL) return NULL;
   printf("switchback: Linker: success!\n");
   return mainp;
}


#endif

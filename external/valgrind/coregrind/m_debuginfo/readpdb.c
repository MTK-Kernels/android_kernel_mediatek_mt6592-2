

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.
   Spring 2008:
      derived from readelf.c and valgrind-20031012-wine/vg_symtab2.c
      derived from wine-1.0/tools/winedump/pdb.c and msc.c

   Copyright (C) 2000-2013 Julian Seward
      jseward@acm.org
   Copyright 2006 Eric Pouech (winedump/pdb.c and msc.c)
      GNU Lesser General Public License version 2.1 or later applies.
   Copyright (C) 2008 BitWagon Software LLC

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
#include "pub_core_debuginfo.h"
#include "pub_core_vki.h"          
#include "pub_core_libcbase.h"
#include "pub_core_libcassert.h"
#include "pub_core_libcfile.h"     
#include "pub_core_libcprint.h"
#include "pub_core_libcproc.h"     
#include "pub_core_options.h"      
#include "pub_core_xarray.h"       
#include "pub_core_redir.h"

#include "priv_misc.h"             
#include "priv_image.h"
#include "priv_d3basics.h"
#include "priv_storage.h"
#include "priv_readpdb.h"          







typedef  UInt   DWORD;
typedef  UShort WORD;
typedef  UChar  BYTE;



#define   OFFSET_OF(__c,__f)   ((int)(((char*)&(((__c*)0)->__f))-((char*)0)))
#define   WIN32_PATH_MAX 256

#pragma pack(2)
typedef struct _IMAGE_DOS_HEADER {
    unsigned short  e_magic;      
    unsigned short  e_cblp;       
    unsigned short  e_cp;         
    unsigned short  e_crlc;       
    unsigned short  e_cparhdr;    
    unsigned short  e_minalloc;   
    unsigned short  e_maxalloc;   
    unsigned short  e_ss;         
    unsigned short  e_sp;         
    unsigned short  e_csum;       
    unsigned short  e_ip;         
    unsigned short  e_cs;         
    unsigned short  e_lfarlc;     
    unsigned short  e_ovno;       
    unsigned short  e_res[4];     
    unsigned short  e_oemid;      
    unsigned short  e_oeminfo;    
    unsigned short  e_res2[10];   
    unsigned long   e_lfanew;     
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

#define IMAGE_DOS_SIGNATURE    0x5A4D     
#define IMAGE_OS2_SIGNATURE    0x454E     
#define IMAGE_OS2_SIGNATURE_LE 0x454C     
#define IMAGE_OS2_SIGNATURE_LX 0x584C     
#define IMAGE_VXD_SIGNATURE    0x454C     
#define IMAGE_NT_SIGNATURE     0x00004550 


#define IMAGE_SUBSYSTEM_UNKNOWN     0
#define IMAGE_SUBSYSTEM_NATIVE      1
#define IMAGE_SUBSYSTEM_WINDOWS_GUI 2  
#define IMAGE_SUBSYSTEM_WINDOWS_CUI 3  
#define IMAGE_SUBSYSTEM_OS2_CUI     5
#define IMAGE_SUBSYSTEM_POSIX_CUI   7

typedef struct _IMAGE_FILE_HEADER {
  unsigned short  Machine;
  unsigned short  NumberOfSections;
  unsigned long   TimeDateStamp;
  unsigned long   PointerToSymbolTable;
  unsigned long   NumberOfSymbols;
  unsigned short  SizeOfOptionalHeader;
  unsigned short  Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY {
  unsigned long VirtualAddress;
  unsigned long Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

typedef struct _IMAGE_OPTIONAL_HEADER {

  

  unsigned short Magic;  
  unsigned char  MajorLinkerVersion;
  unsigned char  MinorLinkerVersion;
  unsigned long  SizeOfCode;
  unsigned long  SizeOfInitializedData;
  unsigned long  SizeOfUninitializedData;
  unsigned long  AddressOfEntryPoint;        
  unsigned long  BaseOfCode;
  unsigned long  BaseOfData;

  

  unsigned long ImageBase;
  unsigned long SectionAlignment;            
  unsigned long FileAlignment;
  unsigned short MajorOperatingSystemVersion;
  unsigned short MinorOperatingSystemVersion;
  unsigned short MajorImageVersion;
  unsigned short MinorImageVersion;
  unsigned short MajorSubsystemVersion;      
  unsigned short MinorSubsystemVersion;
  unsigned long Win32VersionValue;
  unsigned long SizeOfImage;
  unsigned long SizeOfHeaders;
  unsigned long CheckSum;                    
  unsigned short Subsystem;
  unsigned short DllCharacteristics;
  unsigned long SizeOfStackReserve;
  unsigned long SizeOfStackCommit;
  unsigned long SizeOfHeapReserve;           
  unsigned long SizeOfHeapCommit;
  unsigned long LoaderFlags;
  unsigned long NumberOfRvaAndSizes;
  IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]; 
  
} IMAGE_OPTIONAL_HEADER, *PIMAGE_OPTIONAL_HEADER;

typedef struct _IMAGE_NT_HEADERS {
  unsigned long Signature;        
  IMAGE_FILE_HEADER FileHeader;                 
  IMAGE_OPTIONAL_HEADER OptionalHeader;         
} IMAGE_NT_HEADERS, *PIMAGE_NT_HEADERS;

#define IMAGE_SIZEOF_SHORT_NAME 8

typedef struct _IMAGE_SECTION_HEADER {
  unsigned char Name[IMAGE_SIZEOF_SHORT_NAME];
  union {
    unsigned long PhysicalAddress;
    unsigned long VirtualSize;
  } Misc;
  unsigned long VirtualAddress;
  unsigned long SizeOfRawData;
  unsigned long PointerToRawData;
  unsigned long PointerToRelocations;
  unsigned long PointerToLinenumbers;
  unsigned short NumberOfRelocations;
  unsigned short NumberOfLinenumbers;
  unsigned long Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

#define	IMAGE_SIZEOF_SECTION_HEADER 40

#define IMAGE_FIRST_SECTION(ntheader) \
  ((PIMAGE_SECTION_HEADER)((LPunsigned char)&((PIMAGE_NT_HEADERS)(ntheader))->OptionalHeader + \
                           ((PIMAGE_NT_HEADERS)(ntheader))->FileHeader.SizeOfOptionalHeader))


#define IMAGE_SCN_CNT_CODE			0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA		0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA	0x00000080

#define	IMAGE_SCN_LNK_OTHER			0x00000100
#define	IMAGE_SCN_LNK_INFO			0x00000200
#define	IMAGE_SCN_LNK_REMOVE			0x00000800
#define	IMAGE_SCN_LNK_COMDAT			0x00001000

#define	IMAGE_SCN_MEM_FARDATA			0x00008000

#define	IMAGE_SCN_MEM_PURGEABLE			0x00020000
#define	IMAGE_SCN_MEM_16BIT			0x00020000
#define	IMAGE_SCN_MEM_LOCKED			0x00040000
#define	IMAGE_SCN_MEM_PRELOAD			0x00080000

#define	IMAGE_SCN_ALIGN_1BYTES			0x00100000
#define	IMAGE_SCN_ALIGN_2BYTES			0x00200000
#define	IMAGE_SCN_ALIGN_4BYTES			0x00300000
#define	IMAGE_SCN_ALIGN_8BYTES			0x00400000
#define	IMAGE_SCN_ALIGN_16BYTES			0x00500000  
#define IMAGE_SCN_ALIGN_32BYTES			0x00600000
#define IMAGE_SCN_ALIGN_64BYTES			0x00700000

#define IMAGE_SCN_LNK_NRELOC_OVFL		0x01000000


#define IMAGE_SCN_MEM_DISCARDABLE		0x02000000
#define IMAGE_SCN_MEM_NOT_CACHED		0x04000000
#define IMAGE_SCN_MEM_NOT_PAGED			0x08000000
#define IMAGE_SCN_MEM_SHARED			0x10000000
#define IMAGE_SCN_MEM_EXECUTE			0x20000000
#define IMAGE_SCN_MEM_READ			0x40000000
#define IMAGE_SCN_MEM_WRITE			0x80000000

#pragma pack()

typedef struct _GUID  
{
    unsigned int   Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;


#pragma pack(1)
typedef struct _PDB_FILE
{
    unsigned long size;
    unsigned long unknown;

} PDB_FILE, *PPDB_FILE;

struct PDB_JG_HEADER
{
    
    
    unsigned int   blocksize;  
    unsigned short freelist;
    unsigned short total_alloc;
    PDB_FILE toc;
    unsigned short toc_block[ 1 ];
};

struct PDB_DS_HEADER
{
    
    unsigned int  block_size;
    unsigned int unknown1;
    unsigned int num_pages;
    unsigned int toc_size;
    unsigned int unknown2;
    unsigned int toc_page;
};

struct PDB_JG_TOC
{
    unsigned int  nFiles;
    PDB_FILE file[ 1 ];

};

struct PDB_DS_TOC
{
    unsigned int num_files;
    unsigned int file_size[1];
};

struct PDB_JG_ROOT
{
    unsigned int  version;
    unsigned int  TimeDateStamp;
    unsigned int  age;
    unsigned int  cbNames;
    char names[ 1 ];
};

struct PDB_DS_ROOT
{
    unsigned int version;
    unsigned int TimeDateStamp;
    unsigned int age;
    GUID guid;
    unsigned int cbNames;
    char names[1];
};

typedef struct _PDB_TYPES_OLD
{
    unsigned long  version;
    unsigned short first_index;
    unsigned short last_index;
    unsigned long  type_size;
    unsigned short file;
    unsigned short pad;

} PDB_TYPES_OLD, *PPDB_TYPES_OLD;

typedef struct _PDB_TYPES
{
    unsigned long  version;
    unsigned long  type_offset;
    unsigned long  first_index;
    unsigned long  last_index;
    unsigned long  type_size;
    unsigned short file;
    unsigned short pad;
    unsigned long  hash_size;
    unsigned long  hash_base;
    unsigned long  hash_offset;
    unsigned long  hash_len;
    unsigned long  search_offset;
    unsigned long  search_len;
    unsigned long  unknown_offset;
    unsigned long  unknown_len;

} PDB_TYPES, *PPDB_TYPES;

typedef struct _PDB_SYMBOL_RANGE
{
    unsigned short segment;
    unsigned short pad1;
    unsigned long  offset;
    unsigned long  size;
    unsigned long  characteristics;
    unsigned short index;
    unsigned short pad2;

} PDB_SYMBOL_RANGE, *PPDB_SYMBOL_RANGE;

typedef struct _PDB_SYMBOL_RANGE_EX
{
    unsigned short segment;
    unsigned short pad1;
    unsigned long  offset;
    unsigned long  size;
    unsigned long  characteristics;
    unsigned short index;
    unsigned short pad2;
    unsigned long  timestamp;
    unsigned long  unknown;

} PDB_SYMBOL_RANGE_EX, *PPDB_SYMBOL_RANGE_EX;

typedef struct _PDB_SYMBOL_FILE
{
    unsigned long  unknown1;
    PDB_SYMBOL_RANGE range;
    unsigned short flag;
    unsigned short file;
    unsigned long  symbol_size;
    unsigned long  lineno_size;
    unsigned long  unknown2;
    unsigned long  nSrcFiles;
    unsigned long  attribute;
    char filename[ 1 ];

} PDB_SYMBOL_FILE, *PPDB_SYMBOL_FILE;

typedef struct _PDB_SYMBOL_FILE_EX
{
    unsigned long  unknown1;
    PDB_SYMBOL_RANGE_EX range;
    unsigned short flag;
    unsigned short file;
    unsigned long  symbol_size;
    unsigned long  lineno_size;
    unsigned long  unknown2;
    unsigned long  nSrcFiles;
    unsigned long  attribute;
    unsigned long  reserved[ 2 ];
    char filename[ 1 ];

} PDB_SYMBOL_FILE_EX, *PPDB_SYMBOL_FILE_EX;

typedef struct _PDB_SYMBOL_SOURCE
{
    unsigned short nModules;
    unsigned short nSrcFiles;
    unsigned short table[ 1 ];

} PDB_SYMBOL_SOURCE, *PPDB_SYMBOL_SOURCE;

typedef struct _PDB_SYMBOL_IMPORT
{
    unsigned long unknown1;
    unsigned long unknown2;
    unsigned long TimeDateStamp;
    unsigned long nRequests;
    char filename[ 1 ];

} PDB_SYMBOL_IMPORT, *PPDB_SYMBOL_IMPORT;

typedef struct _PDB_SYMBOLS_OLD
{
    unsigned short hash1_file;
    unsigned short hash2_file;
    unsigned short gsym_file;
    unsigned short pad;
    unsigned long  module_size;
    unsigned long  offset_size;
    unsigned long  hash_size;
    unsigned long  srcmodule_size;

} PDB_SYMBOLS_OLD, *PPDB_SYMBOLS_OLD;

typedef struct _PDB_SYMBOLS
{
    unsigned long  signature;
    unsigned long  version;
    unsigned long  unknown;
    unsigned long  hash1_file;
    unsigned long  hash2_file;
    unsigned long  gsym_file;
    unsigned long  module_size;
    unsigned long  offset_size;
    unsigned long  hash_size;
    unsigned long  srcmodule_size;
    unsigned long  pdbimport_size;
    unsigned long  resvd[ 5 ];

} PDB_SYMBOLS, *PPDB_SYMBOLS;
#pragma pack()



struct p_string  
{
    unsigned char               namelen;
    char                        name[1];
};

union codeview_symbol
{
    struct
    {
        short int	        len;
        short int	        id;
    } generic;

    struct
    {
	short int	        len;
	short int	        id;
	unsigned int	        offset;
	unsigned short	        segment;
	unsigned short	        symtype;
        struct p_string         p_name;
    } data_v1;

    struct
    {
	short int	        len;
	short int	        id;
	unsigned int	        symtype;
	unsigned int	        offset;
	unsigned short	        segment;
        struct p_string         p_name;
    } data_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            symtype;
        unsigned int            offset;
        unsigned short          segment;
        char                    name[1];  
    } data_v3;

    struct
    {
	short int	        len;
	short int	        id;
	unsigned int	        pparent;
	unsigned int	        pend;
	unsigned int	        next;
	unsigned int	        offset;
	unsigned short	        segment;
	unsigned short	        thunk_len;
	unsigned char	        thtype;
        struct p_string         p_name;
    } thunk_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            pparent;
        unsigned int            pend;
        unsigned int            next;
        unsigned int            offset;
        unsigned short          segment;
        unsigned short          thunk_len;
        unsigned char           thtype;
        char                    name[1];  
    } thunk_v3;

    struct
    {
	short int	        len;
	short int	        id;
	unsigned int	        pparent;
	unsigned int	        pend;
	unsigned int	        next;
	unsigned int	        proc_len;
	unsigned int	        debug_start;
	unsigned int	        debug_end;
	unsigned int	        offset;
	unsigned short	        segment;
	unsigned short	        proctype;
	unsigned char	        flags;
        struct p_string         p_name;
    } proc_v1;

    struct
    {
	short int	        len;
	short int	        id;
	unsigned int	        pparent;
	unsigned int	        pend;
	unsigned int	        next;
	unsigned int	        proc_len;
	unsigned int	        debug_start;
	unsigned int	        debug_end;
	unsigned int	        proctype;
	unsigned int	        offset;
	unsigned short	        segment;
	unsigned char	        flags;
        struct p_string         p_name;
    } proc_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            pparent;
        unsigned int            pend;
        unsigned int            next;
        unsigned int            proc_len;
        unsigned int            debug_start;
        unsigned int            debug_end;
        unsigned int            proctype;
        unsigned int            offset;
        unsigned short          segment;
        unsigned char           flags;
        char                    name[1];  
    } proc_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            symtype;
        unsigned int            offset;
        unsigned short          segment;
        struct p_string         p_name;
    } public_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            symtype;
        unsigned int            offset;
        unsigned short          segment;
        char                    name[1];  
    } public_v3;

    struct
    {
	short int	        len;	        
	short int	        id;		
	unsigned int	        offset;	        
	unsigned short	        symtype;
        struct p_string         p_name;
    } stack_v1;

    struct
    {
	short int	        len;	        
	short int	        id;		
	unsigned int	        offset;	        
	unsigned int	        symtype;
        struct p_string         p_name;
    } stack_v2;

    struct
    {
        short int               len;            
        short int               id;             
        int                     offset;         
        unsigned int            symtype;
        char                    name[1];  
    } stack_v3;

    struct
    {
        short int               len;            
        short int               id;             
        int                     offset;         
        unsigned int            symtype;
        unsigned short          unknown;
        char                    name[1];  
    } stack_xxxx_v3;

    struct
    {
	short int	        len;	        
	short int	        id;		
        unsigned short          type;
        unsigned short          reg;
        struct p_string         p_name;
        
    } register_v1;

    struct
    {
	short int	        len;	        
	short int	        id;		
        unsigned int            type;           
        unsigned short          reg;
        struct p_string         p_name;
        
    } register_v2;

    struct
    {
	short int	        len;	        
	short int	        id;		
        unsigned int            type;           
        unsigned short          reg;
        char                    name[1];  
        
    } register_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            parent;
        unsigned int            end;
        unsigned int            length;
        unsigned int            offset;
        unsigned short          segment;
        struct p_string         p_name;
    } block_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            parent;
        unsigned int            end;
        unsigned int            length;
        unsigned int            offset;
        unsigned short          segment;
        char                    name[1];  
    } block_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            offset;
        unsigned short          segment;
        unsigned char           flags;
        struct p_string         p_name;
    } label_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            offset;
        unsigned short          segment;
        unsigned char           flags;
        char                    name[1];  
    } label_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned short          type;
        unsigned short          cvalue;         
#if 0
        struct p_string         p_name;
#endif
    } constant_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned                type;
        unsigned short          cvalue;         
#if 0
        struct p_string         p_name;
#endif
    } constant_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned                type;
        unsigned short          cvalue;
#if 0
        char                    name[1];  
#endif
    } constant_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned short          type;
        struct p_string         p_name;
    } udt_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned                type;
        struct p_string         p_name;
    } udt_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            type;
        char                    name[1];  
    } udt_v3;

    struct
    {
        short int               len;
        short int               id;
        char                    signature[4];
        struct p_string         p_name;
    } objname_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            unknown;
        struct p_string         p_name;
    } compiland_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned                unknown1[4];
        unsigned short          unknown2;
        struct p_string         p_name;
    } compiland_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            unknown;
        char                    name[1];  
    } compiland_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            offset;
        unsigned short          segment;
    } ssearch_v1;
};

#define S_COMPILAND_V1  0x0001
#define S_REGISTER_V1   0x0002
#define S_CONSTANT_V1   0x0003
#define S_UDT_V1        0x0004
#define S_SSEARCH_V1    0x0005
#define S_END_V1        0x0006
#define S_SKIP_V1       0x0007
#define S_CVRESERVE_V1  0x0008
#define S_OBJNAME_V1    0x0009
#define S_ENDARG_V1     0x000a
#define S_COBOLUDT_V1   0x000b
#define S_MANYREG_V1    0x000c
#define S_RETURN_V1     0x000d
#define S_ENTRYTHIS_V1  0x000e

#define S_BPREL_V1      0x0200
#define S_LDATA_V1      0x0201
#define S_GDATA_V1      0x0202
#define S_PUB_V1        0x0203
#define S_LPROC_V1      0x0204
#define S_GPROC_V1      0x0205
#define S_THUNK_V1      0x0206
#define S_BLOCK_V1      0x0207
#define S_WITH_V1       0x0208
#define S_LABEL_V1      0x0209
#define S_CEXMODEL_V1   0x020a
#define S_VFTPATH_V1    0x020b
#define S_REGREL_V1     0x020c
#define S_LTHREAD_V1    0x020d
#define S_GTHREAD_V1    0x020e

#define S_PROCREF_V1    0x0400
#define S_DATAREF_V1    0x0401
#define S_ALIGN_V1      0x0402
#define S_LPROCREF_V1   0x0403

#define S_REGISTER_V2   0x1001 
#define S_CONSTANT_V2   0x1002
#define S_UDT_V2        0x1003
#define S_COBOLUDT_V2   0x1004
#define S_MANYREG_V2    0x1005
#define S_BPREL_V2      0x1006
#define S_LDATA_V2      0x1007
#define S_GDATA_V2      0x1008
#define S_PUB_V2        0x1009
#define S_LPROC_V2      0x100a
#define S_GPROC_V2      0x100b
#define S_VFTTABLE_V2   0x100c
#define S_REGREL_V2     0x100d
#define S_LTHREAD_V2    0x100e
#define S_GTHREAD_V2    0x100f
#if 0
#define S_XXXXXXXXX_32  0x1012  
#endif
#define S_COMPILAND_V2  0x1013

#define S_COMPILAND_V3  0x1101
#define S_THUNK_V3      0x1102
#define S_BLOCK_V3      0x1103
#define S_LABEL_V3      0x1105
#define S_REGISTER_V3   0x1106
#define S_CONSTANT_V3   0x1107
#define S_UDT_V3        0x1108
#define S_BPREL_V3      0x110B
#define S_LDATA_V3      0x110C
#define S_GDATA_V3      0x110D
#define S_PUB_V3        0x110E
#define S_LPROC_V3      0x110F
#define S_GPROC_V3      0x1110
#define S_BPREL_XXXX_V3 0x1111  
#define S_MSTOOL_V3     0x1116  
#define S_PUB_FUNC1_V3  0x1125  
#define S_PUB_FUNC2_V3  0x1127



struct pdb_reader
{
   void* (*read_file)(const struct pdb_reader*, unsigned, unsigned *);
   
   UChar* pdbimage;      
   SizeT  uu_n_pdbimage; 
   union {
      struct {
         struct PDB_JG_HEADER* header;
         struct PDB_JG_TOC* toc;
         struct PDB_JG_ROOT* root;
      } jg;
      struct {
         struct PDB_DS_HEADER* header;
         struct PDB_DS_TOC* toc;
         struct PDB_DS_ROOT* root;
      } ds;
   } u;
};


static void* pdb_ds_read( const struct pdb_reader* pdb,
                          const unsigned* block_list,
                          unsigned  size )
{
   unsigned  blocksize, nBlocks;
   UChar* buffer;
   UInt i;

   if (!size) return NULL;
   if (size > 512 * 1024 * 1024) {
      VG_(umsg)("LOAD_PDB_DEBUGINFO: pdb_ds_read: implausible size "
                "(%u); skipping -- possible invalid .pdb file?\n", size);
      return NULL;
   }

   blocksize = pdb->u.ds.header->block_size;
   nBlocks   = (size + blocksize - 1) / blocksize;
   buffer    = ML_(dinfo_zalloc)("di.readpe.pdr.1", nBlocks * blocksize);
   for (i = 0; i < nBlocks; i++)
      VG_(memcpy)( buffer + i * blocksize,
                   pdb->pdbimage + block_list[i] * blocksize,
                   blocksize );
   return buffer;
}


static void* pdb_jg_read( const struct pdb_reader* pdb,
                          const unsigned short* block_list,
                          int size )
{
   unsigned  blocksize, nBlocks;
   UChar* buffer;
   UInt i;
   
   if ( !size ) return NULL;

   blocksize = pdb->u.jg.header->blocksize;
   nBlocks = (size + blocksize-1) / blocksize;
   buffer = ML_(dinfo_zalloc)("di.readpe.pjr.1", nBlocks * blocksize);
   for ( i = 0; i < nBlocks; i++ )
      VG_(memcpy)( buffer + i*blocksize,
                   pdb->pdbimage + block_list[i]*blocksize, blocksize );
   return buffer;
}


static void* find_pdb_header( void* pdbimage,
                              unsigned* signature )
{
   static const HChar pdbtxt[]= "Microsoft C/C++";
   HChar* txteof = VG_(strchr)(pdbimage, '\032');
   if (! txteof)
      return NULL;
   if (0!=VG_(strncmp)(pdbimage, pdbtxt, -1+ sizeof(pdbtxt)))
      return NULL;

   *signature = *(unsigned*)(1+ txteof);
   HChar *img_addr = pdbimage;    
   return ((~3& (3+ (4+ 1+ (txteof - img_addr)))) + img_addr);
}


static void* pdb_ds_read_file( const struct pdb_reader* reader,
                               unsigned  file_number,
                               unsigned* plength )
{
   unsigned i, *block_list;
   if (!reader->u.ds.toc || file_number >= reader->u.ds.toc->num_files)
      return NULL;
   if (reader->u.ds.toc->file_size[file_number] == 0
       || reader->u.ds.toc->file_size[file_number] == 0xFFFFFFFF)
      return NULL;

   block_list
      = reader->u.ds.toc->file_size + reader->u.ds.toc->num_files;
   for (i = 0; i < file_number; i++)
      block_list += (reader->u.ds.toc->file_size[i] 
                     + reader->u.ds.header->block_size - 1)
                    /
                    reader->u.ds.header->block_size;
   if (plength)
      *plength = reader->u.ds.toc->file_size[file_number];
   return pdb_ds_read( reader, block_list,
                       reader->u.ds.toc->file_size[file_number]);
}


static void* pdb_jg_read_file( const struct pdb_reader* pdb,
                               unsigned fileNr,
                               unsigned *plength )
{
   
   unsigned blocksize = pdb->u.jg.header->blocksize;
   struct PDB_JG_TOC* toc = pdb->u.jg.toc;
   unsigned i;
   unsigned short* block_list;

   if ( !toc || fileNr >= toc->nFiles )
       return NULL;

   block_list
      = (unsigned short *) &toc->file[ toc->nFiles ];
   for ( i = 0; i < fileNr; i++ )
      block_list += (toc->file[i].size + blocksize-1) / blocksize;

   if (plength)
      *plength = toc->file[fileNr].size;
   return pdb_jg_read( pdb, block_list, toc->file[fileNr].size );
}


static void pdb_ds_init( struct pdb_reader * reader,
                         UChar* pdbimage,
                         SizeT  n_pdbimage )
{
   reader->read_file     = pdb_ds_read_file;
   reader->pdbimage      = pdbimage;
   reader->uu_n_pdbimage = n_pdbimage;
   reader->u.ds.toc
      = pdb_ds_read(
           reader,
           (unsigned*)(reader->u.ds.header->block_size 
                       * reader->u.ds.header->toc_page 
                       + reader->pdbimage),
           reader->u.ds.header->toc_size
        );
}


static void pdb_jg_init( struct pdb_reader* reader,
                         void* pdbimage,
                         unsigned n_pdbimage )
{
   reader->read_file     = pdb_jg_read_file;
   reader->pdbimage      = pdbimage;
   reader->uu_n_pdbimage = n_pdbimage;
   reader->u.jg.toc = pdb_jg_read(reader,
                                  reader->u.jg.header->toc_block,
                                  reader->u.jg.header->toc.size);
}


static 
void pdb_check_root_version_and_timestamp( const HChar* pdbname,
                                           ULong  pdbmtime,
                                           unsigned  version,
                                           UInt TimeDateStamp )
{
   switch ( version ) {
      case 19950623:      
      case 19950814:
      case 19960307:      
      case 19970604:      
      case 20000404:      
         break;
      default:
         if (VG_(clo_verbosity) > 1)
            VG_(umsg)("LOAD_PDB_DEBUGINFO: "
                      "Unknown .pdb root block version %d\n", version );
   }
   if ( TimeDateStamp != pdbmtime ) {
      if (VG_(clo_verbosity) > 1)
         VG_(umsg)("LOAD_PDB_DEBUGINFO: Wrong time stamp of .PDB file "
                   "%s (0x%08x, 0x%08llx)\n",
                   pdbname, TimeDateStamp, pdbmtime );
   }
}


static DWORD pdb_get_file_size( const struct pdb_reader* reader, unsigned idx )
{
   if (reader->read_file == pdb_jg_read_file)
      return reader->u.jg.toc->file[idx].size;
   else
      return reader->u.ds.toc->file_size[idx];
}


static void pdb_convert_types_header( PDB_TYPES *types, char* image )
{
   VG_(memset)( types, 0, sizeof(PDB_TYPES) );
   if ( !image )
      return;
   if ( *(unsigned long *)image < 19960000 ) {  
      
      PDB_TYPES_OLD *old = (PDB_TYPES_OLD *)image;
      types->version     = old->version;
      types->type_offset = sizeof(PDB_TYPES_OLD);
      types->type_size   = old->type_size;
      types->first_index = old->first_index;
      types->last_index  = old->last_index;
      types->file        = old->file;
   } else {
      
      *types = *(PDB_TYPES *)image;
   }
}


static void pdb_convert_symbols_header( PDB_SYMBOLS *symbols,
                                        int *header_size, char* image )
{
   VG_(memset)( symbols, 0, sizeof(PDB_SYMBOLS) );
   if ( !image )
      return;
   if ( *(unsigned long *)image != 0xffffffff ) {
      
      PDB_SYMBOLS_OLD *old     = (PDB_SYMBOLS_OLD *)image;
      symbols->version         = 0;
      symbols->module_size     = old->module_size;
      symbols->offset_size     = old->offset_size;
      symbols->hash_size       = old->hash_size;
      symbols->srcmodule_size  = old->srcmodule_size;
      symbols->pdbimport_size  = 0;
      symbols->hash1_file      = old->hash1_file;
      symbols->hash2_file      = old->hash2_file;
      symbols->gsym_file       = old->gsym_file;
      *header_size = sizeof(PDB_SYMBOLS_OLD);
   } else {
      
      *symbols = *(PDB_SYMBOLS *)image;
      *header_size = sizeof(PDB_SYMBOLS);
   }
}



static ULong DEBUG_SnarfCodeView(
                DebugInfo* di,
                PtrdiffT bias,
                const IMAGE_SECTION_HEADER* sectp,
                const void* root, 
                Int offset,
                Int size
             )
{
   Int    i, length;
   DiSym  vsym;
   const  HChar* nmstr;
   HChar  symname[4096 ];  

   Bool  debug = di->trace_symtab;
   ULong n_syms_read = 0;

   if (debug)
      VG_(umsg)("BEGIN SnarfCodeView addr=%p offset=%d length=%d\n", 
                root, offset, size );

   VG_(memset)(&vsym, 0, sizeof(vsym));  
   for ( i = offset; i < size; i += length )
   {
      const union codeview_symbol *sym =
         (const union codeview_symbol *)((const char *)root + i);

      length = sym->generic.len + 2;

      
      switch ( sym->generic.id ) {

      default:
         if (0) {
            const int *isym = (const int *)sym;
            VG_(printf)("unknown id 0x%x len=0x%x at %p\n",
                        sym->generic.id, sym->generic.len, sym);
            VG_(printf)("  %8x  %8x  %8x  %8x\n", 
                        isym[1], isym[2], isym[3], isym[4]);
            VG_(printf)("  %8x  %8x  %8x  %8x\n",
                        isym[5], isym[6], isym[7], isym[8]);
         }
         break;
      case S_GDATA_V1:
      case S_LDATA_V1:
      case S_PUB_V1:
         VG_(memcpy)(symname, sym->data_v1.p_name.name,
                              sym->data_v1.p_name.namelen);
         symname[sym->data_v1.p_name.namelen] = '\0';

         if (debug)
            VG_(umsg)("  Data %s\n", symname );

         if (0 ) {
            nmstr = ML_(addStr)(di, symname, sym->data_v1.p_name.namelen);
            vsym.avmas.main = bias + sectp[sym->data_v1.segment-1].VirtualAddress
                                 + sym->data_v1.offset;
            SET_TOCPTR_AVMA(vsym.avmas, 0);
            vsym.pri_name  = nmstr;
            vsym.sec_names = NULL;
            vsym.size      = sym->data_v1.p_name.namelen;
                             
            vsym.isText    = (sym->generic.id == S_PUB_V1);
            vsym.isIFunc   = False;
            ML_(addSym)( di, &vsym );
            n_syms_read++;
         }
         break;
      case S_GDATA_V2:
      case S_LDATA_V2:
      case S_PUB_V2: {
         Int const k = sym->data_v2.p_name.namelen;
         VG_(memcpy)(symname, sym->data_v2.p_name.name, k);
         symname[k] = '\0';

         if (debug)
            VG_(umsg)("  S_GDATA_V2/S_LDATA_V2/S_PUB_V2 %s\n", symname );

         if (sym->generic.id==S_PUB_V2 ) {
            nmstr = ML_(addStr)(di, symname, k);
            vsym.avmas.main = bias + sectp[sym->data_v2.segment-1].VirtualAddress
                                  + sym->data_v2.offset;
            SET_TOCPTR_AVMA(vsym.avmas, 0);
            vsym.pri_name  = nmstr;
            vsym.sec_names = NULL;
            vsym.size      = 4000;
                             
                             
            vsym.isText    = !!(IMAGE_SCN_CNT_CODE 
                                & sectp[sym->data_v2.segment-1].Characteristics);
            vsym.isIFunc   = False;
            ML_(addSym)( di, &vsym );
            n_syms_read++;
         }
         break;
      }
      case S_PUB_V3:
      
      case S_PUB_FUNC1_V3:
      case S_PUB_FUNC2_V3: {
         Int k = sym->public_v3.len - (-1+ sizeof(sym->public_v3));
         if ((-1+ sizeof(symname)) < k)
            k = -1+ sizeof(symname);
         VG_(memcpy)(symname, sym->public_v3.name, k);
         symname[k] = '\0';

         if (debug)
            VG_(umsg)("  S_PUB_FUNC1_V3/S_PUB_FUNC2_V3/S_PUB_V3 %s\n",
                      symname );

         if (1  
) {
            nmstr = ML_(addStr)(di, symname, k);
            vsym.avmas.main = bias + sectp[sym->public_v3.segment-1].VirtualAddress
                                  + sym->public_v3.offset;
            SET_TOCPTR_AVMA(vsym.avmas, 0);
            vsym.pri_name  = nmstr;
            vsym.sec_names = NULL;
            vsym.size      = 4000;
                             
                             
            vsym.isText    = !!(IMAGE_SCN_CNT_CODE
                                & sectp[sym->data_v2.segment-1].Characteristics);
            vsym.isIFunc   = False;
            ML_(addSym)( di, &vsym );
            n_syms_read++;
         }
         break;
      }

      case S_THUNK_V3:
      case S_THUNK_V1:
          
         break;

      case S_GPROC_V1:
      case S_LPROC_V1:
         VG_(memcpy)(symname, sym->proc_v1.p_name.name,
                              sym->proc_v1.p_name.namelen);
         symname[sym->proc_v1.p_name.namelen] = '\0';
         nmstr = ML_(addStr)(di, symname, sym->proc_v1.p_name.namelen);
         vsym.avmas.main = bias + sectp[sym->proc_v1.segment-1].VirtualAddress
                               + sym->proc_v1.offset;
         SET_TOCPTR_AVMA(vsym.avmas, 0);
         vsym.pri_name  = nmstr;
         vsym.sec_names = NULL;
         vsym.size      = sym->proc_v1.proc_len;
         vsym.isText    = True;
         vsym.isIFunc   = False;
         if (debug)
            VG_(umsg)("  Adding function %s addr=%#lx length=%d\n",
                      symname, vsym.avmas.main, vsym.size );
         ML_(addSym)( di, &vsym );
         n_syms_read++;
         break;

      case S_GPROC_V2:
      case S_LPROC_V2:
         VG_(memcpy)(symname, sym->proc_v2.p_name.name,
                              sym->proc_v2.p_name.namelen);
         symname[sym->proc_v2.p_name.namelen] = '\0';
         nmstr = ML_(addStr)(di, symname, sym->proc_v2.p_name.namelen);
         vsym.avmas.main = bias + sectp[sym->proc_v2.segment-1].VirtualAddress
                               + sym->proc_v2.offset;
         SET_TOCPTR_AVMA(vsym.avmas, 0);
         vsym.pri_name  = nmstr;
         vsym.sec_names = NULL;
         vsym.size      = sym->proc_v2.proc_len;
         vsym.isText    = True;
         vsym.isIFunc   = False;
         if (debug)
            VG_(umsg)("  Adding function %s addr=%#lx length=%d\n",
                      symname, vsym.avmas.main, vsym.size );
         ML_(addSym)( di, &vsym );
         n_syms_read++;
         break;
      case S_LPROC_V3:
      case S_GPROC_V3: {
         if (debug)
            VG_(umsg)("  S_LPROC_V3/S_GPROC_V3 %s\n", sym->proc_v3.name );

         if (1) {
            nmstr = ML_(addStr)(di, sym->proc_v3.name,
                                    VG_(strlen)(sym->proc_v3.name));
            vsym.avmas.main = bias + sectp[sym->proc_v3.segment-1].VirtualAddress
                                  + sym->proc_v3.offset;
            SET_TOCPTR_AVMA(vsym.avmas, 0);
            vsym.pri_name  = nmstr;
            vsym.sec_names = NULL;
            vsym.size      = sym->proc_v3.proc_len;
            vsym.isText    = 1;
            vsym.isIFunc   = False;
            ML_(addSym)( di, &vsym );
            n_syms_read++;
         }
         break;
      }
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      


      case S_BPREL_XXXX_V3:
      case S_BPREL_V3:
      case S_BPREL_V2:
      case S_BPREL_V1:
         
         break;

      case S_LABEL_V3:  
      case S_LABEL_V1:
         break;

      case S_SSEARCH_V1:
      case S_ALIGN_V1:
      case S_MSTOOL_V3:
      case S_UDT_V3:
      case S_UDT_V2:
      case S_UDT_V1:
      case S_CONSTANT_V3:
      case S_CONSTANT_V1:
      case S_OBJNAME_V1:
      case S_END_V1:
      case S_COMPILAND_V3:
      case S_COMPILAND_V2:
      case S_COMPILAND_V1:
      case S_BLOCK_V3:
      case S_BLOCK_V1:
      case S_REGISTER_V3:
      case S_REGISTER_V2:
      case S_REGISTER_V1:
         
         break;

      case S_PROCREF_V1:
      case S_DATAREF_V1:
      case S_LPROCREF_V1: {
         const unsigned char *name = (const unsigned char *)sym + length;
         length += (*name + 1 + 3) & ~3;
         break;
      }
      } 

   } 

   if (debug)
      VG_(umsg)("END SnarfCodeView addr=%p offset=%d length=%d\n", 
                root, offset, size );
   return n_syms_read;
}



union any_size
{
          char const *c;
         short const *s;
           int const *i;
  unsigned int const *ui;
};

struct startend
{
  unsigned int          start;
  unsigned int          end;
};

static ULong DEBUG_SnarfLinetab(
          DebugInfo* di,
          PtrdiffT bias,
          const IMAGE_SECTION_HEADER* sectp,
          const void* linetab,
          Int size
       )
{
   
   Int                file_segcount;
   HChar              filename[WIN32_PATH_MAX];
   const UInt         * filetab;
   const UChar        * fn;
   Int                i;
   Int                k;
   const UInt         * lt_ptr;
   Int                nfile;
   Int                nseg;
   union any_size     pnt;
   union any_size     pnt2;
   const struct startend * start;
   Int                this_seg;

   Bool  debug = di->trace_symtab;
   ULong n_lines_read = 0;

   if (debug)
      VG_(umsg)("BEGIN SnarfLineTab linetab=%p size=%d\n", linetab, size);

   pnt.c = linetab;
   nfile = *pnt.s++;
   nseg  = *pnt.s++;

   filetab = pnt.ui;

   nseg = 0;
   for (i = 0; i < nfile; i++) {
      pnt2.c = (const HChar *)linetab + filetab[i];
      nseg += *pnt2.s;
   }

   this_seg = 0;
   for (i = 0; i < nfile; i++) {
      const HChar *fnmstr;
      const HChar *dirstr;
      UInt  fnmdirstr_ix;

      pnt2.c = (const HChar *)linetab + filetab[i];
      file_segcount = *pnt2.s;

      pnt2.ui++;
      lt_ptr = pnt2.ui;
      start = (const struct startend *) (lt_ptr + file_segcount);

      fn = (const UChar*) (start + file_segcount);
      vg_assert(WIN32_PATH_MAX >= 256);
      VG_(memset)(filename, 0, sizeof(filename));
      VG_(memcpy)(filename, fn + 1, *fn);
      vg_assert(filename[ sizeof(filename)-1 ] == 0);
      filename[(Int)*fn] = 0;
      fnmstr = VG_(strrchr)(filename, '\\');
      if (fnmstr == NULL)
         fnmstr = filename;
      else 
         ++fnmstr;
      k = VG_(strlen)(fnmstr);
      dirstr = ML_(addStr)(di, filename, *fn - k);
      fnmstr = ML_(addStr)(di, fnmstr, k);
      fnmdirstr_ix = ML_(addFnDn) (di, fnmstr, dirstr);

      for (k = 0; k < file_segcount; k++, this_seg++) {
         Int linecount;
         Int segno;

         pnt2.c = (const HChar *)linetab + lt_ptr[k];

         segno = *pnt2.s++;
         linecount = *pnt2.s++;

         if ( linecount > 0 ) {
            UInt j;

            if (debug)
               VG_(umsg)(
                  "  Adding %d lines for file %s segment %d addr=%#x end=%#x\n",
                  linecount, filename, segno, start[k].start, start[k].end );

            for ( j = 0; j < linecount; j++ ) {
               Addr startaddr = bias + sectp[segno-1].VirtualAddress
                                     + pnt2.ui[j];
               Addr endaddr   = bias + sectp[segno-1].VirtualAddress
                                     + ((j < (linecount - 1))
                                           ? pnt2.ui[j+1] 
                                           : start[k].end);
               if (debug)
                  VG_(umsg)(
                     "  Adding line %d addr=%#lx end=%#lx\n", 
                     ((const unsigned short *)(pnt2.ui + linecount))[j],
                     startaddr, endaddr );
                  ML_(addLineInfo)(
                     di, 
                     fnmdirstr_ix,
                     startaddr, endaddr,
                     ((const unsigned short *)(pnt2.ui + linecount))[j], j );
                  n_lines_read++;
               }
            }
        }
    }

   if (debug)
      VG_(umsg)("END SnarfLineTab linetab=%p size=%d\n", 
                linetab, size );

    return n_lines_read;
}




typedef struct codeview_linetab2_file
{
    DWORD       offset;         
    WORD        unk;            
    BYTE        md5[16];        
    WORD        pad0;           
} codeview_linetab2_file;

typedef struct codeview_linetab2_block
{
    DWORD       header;         
    DWORD       size_of_block;  
    DWORD       start;          
    DWORD       seg;            
    DWORD       size;           
    DWORD       file_offset;    
    DWORD       nlines;         
    DWORD       size_lines;     
    struct {
        DWORD   offset;         
        DWORD   lineno;         
    } l[1];                     
} codeview_linetab2_block;

static ULong codeview_dump_linetab2(
                DebugInfo* di,
                Addr bias,
                const IMAGE_SECTION_HEADER* sectp,
                const HChar* linetab,
                DWORD size,
                const HChar* strimage,
                DWORD strsize,
                const HChar* pfx
             )
{
   DWORD       offset;
   unsigned    i;
   const codeview_linetab2_block* lbh;
   const codeview_linetab2_file* fd;

   Bool  debug = di->trace_symtab;
   ULong n_line2s_read = 0;

   if (*(const DWORD*)linetab != 0x000000f4)
      return 0;
   offset = *((const DWORD*)linetab + 1);
   lbh = (const codeview_linetab2_block*)(linetab + 8 + offset);

   while ((const HChar*)lbh < linetab + size) {

      UInt filedirname_ix;
      Addr svma_s, svma_e;
      if (lbh->header != 0x000000f2) {
         
         if (debug)
            VG_(printf)("%sblock end %x\n", pfx, lbh->header);
         break;
      }
      if (debug)
         VG_(printf)("%sblock from %04x:%08x-%08x (size %u) (%u lines)\n",
                     pfx, lbh->seg, lbh->start, lbh->start + lbh->size - 1,
                     lbh->size, lbh->nlines);
      fd = (const codeview_linetab2_file*)(linetab + 8 + lbh->file_offset);
      if (debug)
         VG_(printf)(
            "%s  md5=%02x%02x%02x%02x%02x%02x%02x%02x"
                    "%02x%02x%02x%02x%02x%02x%02x%02x\n",
             pfx, fd->md5[ 0], fd->md5[ 1], fd->md5[ 2], fd->md5[ 3],
                  fd->md5[ 4], fd->md5[ 5], fd->md5[ 6], fd->md5[ 7],
                  fd->md5[ 8], fd->md5[ 9], fd->md5[10], fd->md5[11],
                  fd->md5[12], fd->md5[13], fd->md5[14], fd->md5[15] );
      
      const HChar* filename = NULL; 
      const HChar* dirname  = NULL; 
      if (strimage) {
         const HChar* strI = strimage + fd->offset;
         HChar* strM  = ML_(dinfo_strdup)("di.readpe.cdl2.1", strI);
         HChar* fname = VG_(strrchr)(strM, '\\');
         if (fname == NULL) {
            filename = ML_(addStr)(di, strM, -1);
            dirname  = NULL;
         } else {
            *fname++ = '\0';
            filename = ML_(addStr)(di, fname, -1);
            dirname  = ML_(addStr)(di, strM, -1);
         }
         ML_(dinfo_free)(strM);
      } else {
         filename = ML_(addStr)(di, "???", -1);
         dirname  = NULL;
      }

      if (debug)
         VG_(printf)("%s  file=%s\n", pfx, filename);

      filedirname_ix = ML_(addFnDn) (di, filename, dirname);

      for (i = 0; i < lbh->nlines; i++) {
         if (debug)
            VG_(printf)("%s  offset=%08x line=%d\n",
                        pfx, lbh->l[i].offset, lbh->l[i].lineno ^ 0x80000000);
      }

      if (lbh->nlines > 1) {
         for (i = 0; i < lbh->nlines-1; i++) {
            svma_s = sectp[lbh->seg - 1].VirtualAddress + lbh->start
                     + lbh->l[i].offset;
            svma_e = sectp[lbh->seg - 1].VirtualAddress + lbh->start
                     + lbh->l[i+1].offset-1;
            if (debug)
               VG_(printf)("%s  line %d: %08lx to %08lx\n",
                           pfx, lbh->l[i].lineno ^ 0x80000000, svma_s, svma_e);
            ML_(addLineInfo)( di, 
                              filedirname_ix,
                              bias + svma_s,
                              bias + svma_e + 1,
                              lbh->l[i].lineno ^ 0x80000000, 0 );
            n_line2s_read++;
         }
         svma_s = sectp[lbh->seg - 1].VirtualAddress + lbh->start
                  + lbh->l[ lbh->nlines-1].offset;
         svma_e = sectp[lbh->seg - 1].VirtualAddress + lbh->start
                  + lbh->size - 1;
         if (debug)
            VG_(printf)("%s  line %d: %08lx to %08lx\n",
                        pfx, lbh->l[ lbh->nlines-1  ].lineno ^ 0x80000000,
                        svma_s, svma_e);
          ML_(addLineInfo)( di, 
                            filedirname_ix,
                            bias + svma_s,
                            bias + svma_e + 1,
                            lbh->l[lbh->nlines-1].lineno ^ 0x80000000, 0 );
          n_line2s_read++;
       }

       lbh = (const codeview_linetab2_block*)
                ((const char*)lbh + 8 + lbh->size_of_block);
    }
    return n_line2s_read;
}



static Int cmp_FPO_DATA_for_canonicalisation ( const void* f1V,
                                               const void* f2V )
{
   const FPO_DATA* f1 = f1V;
   const FPO_DATA* f2 = f2V;
   if (f1->ulOffStart < f2->ulOffStart) return -1;
   if (f1->ulOffStart > f2->ulOffStart) return  1;
   if (f1->cbProcSize < f2->cbProcSize) return -1;
   if (f1->cbProcSize > f2->cbProcSize) return  1;
   return 0; 
}

static unsigned get_stream_by_name(const struct pdb_reader* pdb, const char* name)
{
    const DWORD* pdw;
    const DWORD* ok_bits;
    DWORD        cbstr, count;
    DWORD        string_idx, stream_idx;
    unsigned     i;
    const char*  str;

    if (pdb->read_file == pdb_jg_read_file)
    {
        str = pdb->u.jg.root->names;
        cbstr = pdb->u.jg.root->cbNames;
    }
    else
    {
        str = pdb->u.ds.root->names;
        cbstr = pdb->u.ds.root->cbNames;
    }

    pdw = (const DWORD*)(str + cbstr);
    pdw++; 
    count = *pdw++;

    
    ok_bits = pdw;
    pdw += *ok_bits++ + 1;
    if (*pdw++ != 0)
    {
        if (VG_(clo_verbosity) > 1)
           VG_(umsg)("LOAD_PDB_DEBUGINFO: "
                     "get_stream_by_name: unexpected value\n");
        return -1;
    }

    for (i = 0; i < count; i++)
    {
        if (ok_bits[i / 32] & (1 << (i % 32)))
        {
            string_idx = *pdw++;
            stream_idx = *pdw++;
            if (!VG_(strcmp)(name, &str[string_idx])) return stream_idx;
        }
    }
    return -1;
}
 

static void *read_string_table(const struct pdb_reader* pdb)
{
    unsigned    stream_idx;
    void*       ret;

    stream_idx = get_stream_by_name(pdb, "/names");
    if (stream_idx == -1) return NULL;
    ret = pdb->read_file(pdb, stream_idx,0);
    if (ret && *(const DWORD*)ret == 0xeffeeffe) {
       return ret;
    }
    if (VG_(clo_verbosity) > 1)
       VG_(umsg)("LOAD_PDB_DEBUGINFO: read_string_table: "
                 "wrong header 0x%08x, expecting 0xeffeeffe\n",
                 *(const DWORD*)ret);
    ML_(dinfo_free)( ret );
    return NULL;
}

static void pdb_dump( const struct pdb_reader* pdb,
                      DebugInfo* di,
                      Addr       pe_avma,
                      PtrdiffT   pe_bias,
                      const IMAGE_SECTION_HEADER* sectp_avma )
{
   Int header_size;

   PDB_TYPES types;
   PDB_SYMBOLS symbols;
   unsigned len_modimage;
   char *modimage;
   const char *file; 

   Bool debug = di->trace_symtab;

   ULong n_fpos_read = 0, n_syms_read = 0,
         n_lines_read = 0, n_line2s_read = 0;

   

   char* types_image   = pdb->read_file( pdb, 2, 0 );
   char* symbols_image = pdb->read_file( pdb, 3, 0 );

   DWORD filessize  = 0;
   char* filesimage = read_string_table(pdb);
   if (filesimage) {
      filessize = *(const DWORD*)(filesimage + 8);
   } else {
      VG_(umsg)("LOAD_PDB_DEBUGINFO: pdb_dump: string table not found\n");
   }

   vg_assert(sizeof(FPO_DATA) == 16);
   if (di->text_present) { 
      unsigned sz = 0;
      di->fpo = pdb->read_file( pdb, 5, &sz );

      
      
      
      
      while (sz > 0 && (sz % sizeof(FPO_DATA)) != 0)
         sz--;

      di->fpo_size = sz;
      if (0) VG_(printf)("FPO: got fpo_size %lu\n", (UWord)sz);
      vg_assert(0 == (di->fpo_size % sizeof(FPO_DATA)));
      di->fpo_base_avma = pe_avma;
   } else {
      vg_assert(di->fpo == NULL);
      vg_assert(di->fpo_size == 0);
   }

   
   if (di->fpo && di->fpo_size > 0) {
      Word i, j;
      Bool anyChanges;
      Int itersAvail = 10;

      vg_assert(sizeof(di->fpo[0]) == 16);
      di->fpo_size /= sizeof(di->fpo[0]);

      
      do {

         vg_assert(itersAvail >= 0); 
         itersAvail--;

         anyChanges = False;

         
         VG_(ssort)( di->fpo, (SizeT)di->fpo_size, (SizeT)sizeof(FPO_DATA),
                              cmp_FPO_DATA_for_canonicalisation );
         
         j = 0;
         for (i = 0; i < di->fpo_size; i++) {
            if (di->fpo[i].cbProcSize == 0) {
               anyChanges = True;
               continue;
            }
            di->fpo[j++] = di->fpo[i];
         }
         vg_assert(j >= 0 && j <= di->fpo_size);
         di->fpo_size = j;

         
         if (di->fpo_size > 1) {
            j = 1;
            for (i = 1; i < di->fpo_size; i++) {
               Bool dup
                  = di->fpo[j-1].ulOffStart == di->fpo[i].ulOffStart
                    && di->fpo[j-1].cbProcSize == di->fpo[i].cbProcSize;
               if (dup) {
                 anyChanges = True;
                 continue;
               }
               di->fpo[j++] = di->fpo[i];
            }
            vg_assert(j >= 0 && j <= di->fpo_size);
            di->fpo_size = j;
         }

         
         for (i = 1; i < di->fpo_size; i++) {
            vg_assert(di->fpo[i-1].ulOffStart <= di->fpo[i].ulOffStart);
            if (di->fpo[i-1].ulOffStart + di->fpo[i-1].cbProcSize 
                > di->fpo[i].ulOffStart) {
               anyChanges = True;
               di->fpo[i-1].cbProcSize
                  = di->fpo[i].ulOffStart - di->fpo[i-1].ulOffStart;
            }
         }

      } while (anyChanges);
      

      for (i = 0; i < di->fpo_size; i++) {
         vg_assert(di->fpo[i].cbProcSize > 0);

         if (i > 0) {
            vg_assert(di->fpo[i-1].ulOffStart < di->fpo[i].ulOffStart);
            vg_assert(di->fpo[i-1].ulOffStart + di->fpo[i-1].cbProcSize
                      <= di->fpo[i].ulOffStart);
         }
      }

      for (i = 0; i < di->fpo_size; i++) {
         di->fpo[i].ulOffStart += pe_avma;
         
         
         vg_assert(0xFFFFFFFF - di->fpo[i].ulOffStart 
                   >= di->fpo[i].cbProcSize);
      }

      Addr min_avma = ~(Addr)0;
      Addr max_avma = (Addr)0;
      vg_assert(di->text_present);
      j = 0;
      for (i = 0; i < di->fpo_size; i++) {
         if ((Addr)(di->fpo[i].ulOffStart) >= di->text_avma
             && (Addr)(di->fpo[i].ulOffStart + di->fpo[i].cbProcSize)
                <= di->text_avma + di->text_size) {
            
            if (di->fpo[i].ulOffStart < min_avma)
               min_avma = di->fpo[i].ulOffStart;
            if (di->fpo[i].ulOffStart + di->fpo[i].cbProcSize - 1 > max_avma)
               max_avma = di->fpo[i].ulOffStart + di->fpo[i].cbProcSize - 1;
            
            di->fpo[j++] = di->fpo[i];
            if (0)
            VG_(printf)("FPO: keep text=[0x%lx,0x%lx) 0x%lx 0x%lx\n",
                        di->text_avma, di->text_avma + di->text_size,
                        (Addr)di->fpo[i].ulOffStart,
                        (Addr)di->fpo[i].ulOffStart 
                        + (Addr)di->fpo[i].cbProcSize - 1);
         } else {
            if (0)
            VG_(printf)("FPO: SKIP text=[0x%lx,0x%lx) 0x%lx 0x%lx\n",
                        di->text_avma, di->text_avma + di->text_size,
                        (Addr)di->fpo[i].ulOffStart,
                        (Addr)di->fpo[i].ulOffStart 
                        + (Addr)di->fpo[i].cbProcSize - 1);
            
         }
      }
      vg_assert(j >= 0 && j <= di->fpo_size);
      di->fpo_size = j;

      
      
      if (di->fpo_size > 0) {
         vg_assert(min_avma <= max_avma); 
         di->fpo_minavma = min_avma;
         di->fpo_maxavma = max_avma;
      } else {
         di->fpo_minavma = 0;
         di->fpo_maxavma = 0;
      }

      if (0) {
         VG_(printf)("FPO: min/max avma %#lx %#lx\n",
                     di->fpo_minavma, di->fpo_maxavma);
      }

      n_fpos_read += (ULong)di->fpo_size;
   }
   

   pdb_convert_types_header( &types, types_image );
   switch ( types.version ) {
      case 19950410:      
      case 19951122:
      case 19961031:      
      case 20040203:      
         break;
      default:
         if (VG_(clo_verbosity) > 1)
            VG_(umsg)("LOAD_PDB_DEBUGINFO: "
                      "Unknown .pdb type info version %ld\n", types.version );
   }

   header_size = 0;
   pdb_convert_symbols_header( &symbols, &header_size, symbols_image );
   switch ( symbols.version ) {
      case 0:            
      case 19960307:     
      case 19970606:     
      case 19990903:     
         break;
      default:
         if (VG_(clo_verbosity) > 1)
            VG_(umsg)("LOAD_PDB_DEBUGINFO: "
                      "Unknown .pdb symbol info version %ld\n",
                      symbols.version );
   }

   modimage = pdb->read_file( pdb, symbols.gsym_file, &len_modimage );
   if (modimage) {
      if (debug)
         VG_(umsg)("\n");
      if (VG_(clo_verbosity) > 1)
         VG_(umsg)("LOAD_PDB_DEBUGINFO: Reading global symbols\n" );
      DEBUG_SnarfCodeView( di, pe_avma, sectp_avma, modimage, 0, len_modimage );
      ML_(dinfo_free)( modimage );
   }

   file = symbols_image + header_size;
   while ( file - symbols_image < header_size + symbols.module_size ) {
      int file_nr,  symbol_size, lineno_size;
      const char *file_name;

      if ( symbols.version < 19970000 ) {
         const PDB_SYMBOL_FILE *sym_file = (const PDB_SYMBOL_FILE *) file;
         file_nr     = sym_file->file;
         file_name   = sym_file->filename;
          
         symbol_size = sym_file->symbol_size;
         lineno_size = sym_file->lineno_size;
      } else {
         const PDB_SYMBOL_FILE_EX *sym_file = (const PDB_SYMBOL_FILE_EX *) file;
         file_nr     = sym_file->file;
         file_name   = sym_file->filename;
          
         symbol_size = sym_file->symbol_size;
         lineno_size = sym_file->lineno_size;
      }

      modimage = pdb->read_file( pdb, file_nr, 0 );
      if (modimage) {
         Int total_size;
         if (0) VG_(printf)("lineno_size %d symbol_size %d\n",
                            lineno_size, symbol_size );

         total_size = pdb_get_file_size(pdb, file_nr);

         if (symbol_size) {
            if (debug)
               VG_(umsg)("\n");
            if (VG_(clo_verbosity) > 1)
               VG_(umsg)("LOAD_PDB_DEBUGINFO: Reading symbols for %s\n",
                                        file_name );
            n_syms_read 
               += DEBUG_SnarfCodeView( di, pe_avma, sectp_avma, modimage,
                                           sizeof(unsigned long),
                                           symbol_size );
         }

         if (lineno_size) {
            if (debug)
               VG_(umsg)("\n");
            if (VG_(clo_verbosity) > 1)
               VG_(umsg)("LOAD_PDB_DEBUGINFO: "
                         "Reading lines for %s\n", file_name );
            n_lines_read
               += DEBUG_SnarfLinetab( di, pe_avma, sectp_avma,
                                          modimage + symbol_size, lineno_size );
         }

         if (0) VG_(printf)("Reading lines for %s\n", file_name );
         n_line2s_read
            += codeview_dump_linetab2(
                  di, pe_avma, sectp_avma,
                      (HChar*)modimage + symbol_size + lineno_size,
                      total_size - (symbol_size + lineno_size),
                  filesimage ? filesimage + 12 : NULL,
                  filessize, "        "
               );

         ML_(dinfo_free)( modimage );
      }

      file_name += VG_(strlen)(file_name) + 1;
      file = (const char *)( 
                (unsigned long)(file_name
                                + VG_(strlen)(file_name) + 1 + 3) & ~3 );
   }

   if ( symbols_image ) ML_(dinfo_free)( symbols_image );
   if ( types_image ) ML_(dinfo_free)( types_image );
   if ( pdb->u.jg.toc ) ML_(dinfo_free)( pdb->u.jg.toc );

   if (VG_(clo_verbosity) > 1) {
      VG_(dmsg)("LOAD_PDB_DEBUGINFO:"
                "    # symbols read = %llu\n", n_syms_read );
      VG_(dmsg)("LOAD_PDB_DEBUGINFO:"
                "    # lines   read = %llu\n", n_lines_read );
      VG_(dmsg)("LOAD_PDB_DEBUGINFO:"
                "    # line2s  read = %llu\n", n_line2s_read );
      VG_(dmsg)("LOAD_PDB_DEBUGINFO:"
                "    # fpos    read = %llu\n", n_fpos_read );
   }
}



Bool ML_(read_pdb_debug_info)(
        DebugInfo* di,
        Addr       obj_avma,
        PtrdiffT   obj_bias,
        void*      pdbimage,
        SizeT      n_pdbimage,
        const HChar* pdbname,
        ULong      pdbmtime
     )
{
   Char*    pe_seg_avma;
   Int      i;
   Addr     mapped_avma, mapped_end_avma;
   unsigned signature;
   void*    hdr;
   struct pdb_reader     reader;
   IMAGE_DOS_HEADER*     dos_avma;
   IMAGE_NT_HEADERS*     ntheaders_avma;
   IMAGE_SECTION_HEADER* sectp_avma;
   IMAGE_SECTION_HEADER* pe_sechdr_avma;

   if (VG_(clo_verbosity) > 1)
       VG_(umsg)("LOAD_PDB_DEBUGINFO: Processing PDB file %s\n", pdbname );

   dos_avma = (IMAGE_DOS_HEADER *)obj_avma;
   if (dos_avma->e_magic != IMAGE_DOS_SIGNATURE)
      return False;

   ntheaders_avma
      = (IMAGE_NT_HEADERS *)((Char*)dos_avma + dos_avma->e_lfanew);
   if (ntheaders_avma->Signature != IMAGE_NT_SIGNATURE)
      return False;

   sectp_avma
      = (IMAGE_SECTION_HEADER *)(
           (Char*)ntheaders_avma
           + OFFSET_OF(IMAGE_NT_HEADERS, OptionalHeader)
           + ntheaders_avma->FileHeader.SizeOfOptionalHeader
        );

   
   di->soname = ML_(dinfo_strdup)("di.readpdb.rpdi.1", pdbname);

   pe_seg_avma
      = (Char*)ntheaders_avma
        + OFFSET_OF(IMAGE_NT_HEADERS, OptionalHeader)
        + ntheaders_avma->FileHeader.SizeOfOptionalHeader;

   
   for ( i = 0;
         i < ntheaders_avma->FileHeader.NumberOfSections;
         i++, pe_seg_avma += sizeof(IMAGE_SECTION_HEADER) ) {
      pe_sechdr_avma = (IMAGE_SECTION_HEADER *)pe_seg_avma;

      if (VG_(clo_verbosity) > 1) {
         
         char name[9];
         VG_(memcpy)(name, pe_sechdr_avma->Name, 8);
         name[8] = '\0';
         VG_(umsg)("LOAD_PDB_DEBUGINFO:"
                   "   Scanning PE section %ps at avma %#lx svma %#lx\n",
                   name, obj_avma + pe_sechdr_avma->VirtualAddress,
                   pe_sechdr_avma->VirtualAddress);
      }

      if (pe_sechdr_avma->Characteristics & IMAGE_SCN_MEM_DISCARDABLE)
         continue;

      mapped_avma     = (Addr)obj_avma + pe_sechdr_avma->VirtualAddress;
      mapped_end_avma = mapped_avma + pe_sechdr_avma->Misc.VirtualSize;

      DebugInfoMapping map;
      map.avma = mapped_avma;
      map.size = pe_sechdr_avma->Misc.VirtualSize;
      map.foff = pe_sechdr_avma->PointerToRawData;
      map.ro   = False;

      if (pe_sechdr_avma->Characteristics & IMAGE_SCN_CNT_CODE) {
         if (!(pe_sechdr_avma->Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)) {
            map.rx   = True;
            map.rw   = False;
            VG_(addToXA)(di->fsm.maps, &map);
            di->fsm.have_rx_map = True;

            di->text_present = True;
            if (di->text_avma == 0) {
               di->text_svma = pe_sechdr_avma->VirtualAddress;
               di->text_avma = mapped_avma;
               di->text_size = pe_sechdr_avma->Misc.VirtualSize;
            } else {
               di->text_size = mapped_end_avma - di->text_avma;
            }
         }
      }
      else if (pe_sechdr_avma->Characteristics 
               & IMAGE_SCN_CNT_INITIALIZED_DATA) {
         map.rx   = False;
         map.rw   = True;
         VG_(addToXA)(di->fsm.maps, &map);
         di->fsm.have_rw_map = True;

         di->data_present = True;
         if (di->data_avma == 0) {
            di->data_avma = mapped_avma;
            di->data_size = pe_sechdr_avma->Misc.VirtualSize;
         } else {
            di->data_size = mapped_end_avma - di->data_avma;
         }
      }
      else if (pe_sechdr_avma->Characteristics
               & IMAGE_SCN_CNT_UNINITIALIZED_DATA) {
         di->bss_present = True;
         if (di->bss_avma == 0) {
            di->bss_avma = mapped_avma;
            di->bss_size = pe_sechdr_avma->Misc.VirtualSize;
         } else {
            di->bss_size = mapped_end_avma - di->bss_avma;
         }
      }
   }

   if (di->fsm.have_rx_map && di->fsm.have_rw_map && !di->have_dinfo) {
      vg_assert(di->fsm.filename);
      TRACE_SYMTAB("\n");
      TRACE_SYMTAB("------ start PE OBJECT with PDB INFO "
                   "---------------------\n");
      TRACE_SYMTAB("------ name = %s\n", di->fsm.filename);
      TRACE_SYMTAB("\n");
   }

   di->text_bias = obj_bias;

   if (VG_(clo_verbosity) > 1) {
      for (i = 0; i < VG_(sizeXA)(di->fsm.maps); i++) {
         const DebugInfoMapping* map = VG_(indexXA)(di->fsm.maps, i);
         if (map->rx)
            VG_(dmsg)("LOAD_PDB_DEBUGINFO: "
                      "rx_map: avma %#lx size %7lu foff %llu\n",
                      map->avma, map->size, (Off64T)map->foff);
      }
      for (i = 0; i < VG_(sizeXA)(di->fsm.maps); i++) {
         const DebugInfoMapping* map = VG_(indexXA)(di->fsm.maps, i);
         if (map->rw)
            VG_(dmsg)("LOAD_PDB_DEBUGINFO: "
                      "rw_map: avma %#lx size %7lu foff %llu\n",
                      map->avma, map->size, (Off64T)map->foff);
      }

      VG_(dmsg)("LOAD_PDB_DEBUGINFO: "
                "  text: avma %#lx svma %#lx size %7lu bias %#lx\n",
                di->text_avma, di->text_svma,
                di->text_size, di->text_bias);
   }

   signature = 0;
   hdr = find_pdb_header( pdbimage, &signature );
   if (0==hdr)
      return False; 

   VG_(memset)(&reader, 0, sizeof(reader));
   reader.u.jg.header = hdr;

   if (0==VG_(strncmp)((char const *)&signature, "DS\0\0", 4)) {
      struct PDB_DS_ROOT* root;
      pdb_ds_init( &reader, pdbimage, n_pdbimage );
      root = reader.read_file( &reader, 1, 0 );
      reader.u.ds.root = root;
      if (root) {
         pdb_check_root_version_and_timestamp(
            pdbname, pdbmtime, root->version, root->TimeDateStamp );
      }
      pdb_dump( &reader, di, obj_avma, obj_bias, sectp_avma );
      if (root) {
         ML_(dinfo_free)( root );
      }
   }
   else
   if (0==VG_(strncmp)((char const *)&signature, "JG\0\0", 4)) {
      struct PDB_JG_ROOT* root;
      pdb_jg_init( &reader, pdbimage, n_pdbimage );
      root = reader.read_file( &reader, 1, 0 );
      reader.u.jg.root = root;	
      if (root) {
         pdb_check_root_version_and_timestamp(
            pdbname, pdbmtime, root->version, root->TimeDateStamp);
      }
      pdb_dump( &reader, di, obj_avma, obj_bias, sectp_avma );
      if (root) {
         ML_(dinfo_free)( root );
      }
   }

   if (1) {
      TRACE_SYMTAB("\n------ Canonicalising the "
                   "acquired info ------\n");
      
      ML_(canonicaliseTables)( di );
      
      TRACE_SYMTAB("\n------ Notifying m_redir ------\n");
      VG_(redir_notify_new_DebugInfo)( di );
      
      di->have_dinfo = True;
   } else {
      TRACE_SYMTAB("\n------ PE with PDB reading failed ------\n");
   }

   TRACE_SYMTAB("\n");
   TRACE_SYMTAB("------ name = %s\n", di->fsm.filename);
   TRACE_SYMTAB("------ end PE OBJECT with PDB INFO "
                "--------------------\n");
   TRACE_SYMTAB("\n");

   return True;
}



HChar* ML_(find_name_of_pdb_file)( const HChar* pename )
{
   Bool   do_cleanup = False;
   HChar  tmpnameroot[50];     
   HChar  tmpname[VG_(mkstemp_fullname_bufsz)(sizeof tmpnameroot - 1)];
   Int    fd, r;
   HChar* res = NULL;

   if (!pename)
      goto out;

   fd = -1;
   VG_(memset)(tmpnameroot, 0, sizeof(tmpnameroot));
   VG_(sprintf)(tmpnameroot, "petmp%d", VG_(getpid)());
   VG_(memset)(tmpname, 0, sizeof(tmpname));
   fd = VG_(mkstemp)( tmpnameroot, tmpname );
   if (fd == -1) {
      VG_(umsg)("LOAD_PDB_DEBUGINFO: "
                "Find PDB file: Can't create temporary file %s\n", tmpname);
      goto out;
   }
   do_cleanup = True;

   const HChar* sh      = "/bin/sh";
   const HChar* strings = "/usr/bin/strings";
   const HChar* egrep   = "/usr/bin/egrep";

   
   Int cmdlen = VG_(strlen)(strings) + VG_(strlen)(pename)
                + VG_(strlen)(egrep) + VG_(strlen)(tmpname)
                + 100;
   HChar* cmd = ML_(dinfo_zalloc)("di.readpe.fnopf.cmd", cmdlen);
   VG_(sprintf)(cmd, "%s -c \"%s '%s' | %s '\\.pdb$|\\.PDB$' >> %s\"",
                     sh, strings, pename, egrep, tmpname);
   vg_assert(cmd[cmdlen-1] == 0);
   if (0) VG_(printf)("QQQQQQQQ: %s\n", cmd);

   r = VG_(system)( cmd );
   if (r) {
      VG_(dmsg)("LOAD_PDB_DEBUGINFO: "
                "Find PDB file: Command failed:\n   %s\n", cmd);
      goto out;
   }

   
   struct vg_stat stat_buf;
   VG_(memset)(&stat_buf, 0, sizeof(stat_buf));

   SysRes sr = VG_(stat)(tmpname, &stat_buf);
   if (sr_isError(sr)) {
      VG_(umsg)("LOAD_PDB_DEBUGINFO: Find PDB file: can't stat %s\n", tmpname);
      goto out;
   }

   Int szB = (Int)stat_buf.size;
   if (szB == 0) {
      VG_(umsg)("LOAD_PDB_DEBUGINFO: Find PDB file: %s is empty\n", tmpname);
      goto out;
   }
   
   if (szB < 6 || szB > 1024) {
      VG_(umsg)("LOAD_PDB_DEBUGINFO: Find PDB file: %s has implausible size %d\n",
                tmpname, szB);
      goto out;
   }

   HChar* pdbname = ML_(dinfo_zalloc)("di.readpe.fnopf.pdbname", szB + 1);
   pdbname[szB] = 0;

   Int nread = VG_(read)(fd, pdbname, szB);
   if (nread != szB) {
      VG_(umsg)("LOAD_PDB_DEBUGINFO: Find PDB file: read of %s failed\n", tmpname);
      goto out;
   }
   vg_assert(pdbname[szB] == 0);

   Bool saw_dot = False;
   Int  saw_n_crs = 0;
   Int  i;
   for (i = 0; pdbname[i]; i++) {
      if (pdbname[i] == '.')  saw_dot = True;
      if (pdbname[i] == '\n') saw_n_crs++;
   }
   if (!saw_dot || saw_n_crs != 1 || pdbname[szB-1] != '\n') {
      VG_(umsg)("LOAD_PDB_DEBUGINFO: Find PDB file: can't make sense of: %s\n", pdbname);
      goto out;
   }
   
   pdbname[szB-1] = 0;

   if (0) VG_(printf)("QQQQQQQQ: got %s\n", pdbname);

   res = pdbname;
   goto out;

  out:
   if (do_cleanup) {
      VG_(close)(fd);
      VG_(unlink)( tmpname );
   }
   return res;
}

#endif 


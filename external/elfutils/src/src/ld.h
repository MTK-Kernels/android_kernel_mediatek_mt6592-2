/* Copyright (C) 2001, 2002, 2003, 2005, 2006, 2008, 2009 Red Hat, Inc.
   This file is part of elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2001.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef LD_H
#define LD_H	1

#include <dlfcn.h>
#include <obstack.h>
#include <stdbool.h>
#include <stdio.h>
#include "xelf.h"


#define ERRBUFSIZE	(512)

#define VER_CHR	'@'


enum extract_rule
  {
    defaultextract,	
    weakextract,	
    allextract		
  };


enum file_type
  {
    no_file_type = 0,		
    executable_file_type,	
    dso_file_type,		
    dso_needed_file_type,	
    relocatable_file_type,	
    archive_file_type		
  };


struct usedfiles
{
  
  struct usedfiles *next;
  
  bool group_start;
  
  bool group_end;
  struct usedfiles *group_backref;

  
  const char *fname;
  
  const char *rfname;
  const char *soname;
  
  struct Ebl_Strent *sonameent;

  
  dev_t dev;
  ino_t ino;

  enum
    {
      not_opened,
      opened,
      in_archive,
      closed
    } status;

  
  enum extract_rule extract_rule;

  
  bool lazyload;

  bool used;

  bool as_needed;

  int archive_seq;

  
  struct usedfiles *archive_file;

  enum file_type file_type;
  
  Elf *elf;

  
#if NATIVE_ELF != 0
  XElf_Ehdr *ehdr;
# define FILEINFO_EHDR(fi) (*(fi))
#else
  XElf_Ehdr ehdr;
# define FILEINFO_EHDR(fi) (fi)
#endif

  size_t shstrndx;

  
  struct scninfo
  {
    Elf_Scn *scn;
    
#if NATIVE_ELF != 0
    XElf_Shdr *shdr;
# define SCNINFO_SHDR(si) (*(si))
#else
    XElf_Shdr shdr;
# define SCNINFO_SHDR(si) (si)
#endif
    
    XElf_Off offset;
    
    Elf32_Word outscnndx;
    
    Elf32_Word allsectionsidx;
    
    bool used;
    
    bool unused_comdat;
    
    bool comdat_group;
    
    Elf32_Word grpid;
    
    struct usedfiles *fileinfo;
    struct symbol *symbols;
    size_t relsize;
    struct scninfo *next;
  } *scninfo;

  
  struct scninfo *groups;

  Elf_Data *symtabdata;
  
  Elf_Data *xndxdata;
  
  Elf_Data *dynsymtabdata;
  
  Elf_Data *versymdata;
  
  Elf_Data *verdefdata;
  
  size_t nverdef;
  XElf_Versym *verdefused;
  
  size_t nverdefused;
  
  struct Ebl_Strent **verdefent;
  
  Elf_Data *verneeddata;
  
  Elf32_Word symstridx;
  
  Elf32_Word dynsymstridx;
  
  size_t nsymtab;
  size_t nlocalsymbols;
  size_t ndynsymtab;
  
  Elf_Scn *dynscn;

  
  Elf32_Word *symindirect;
  Elf32_Word *dynsymindirect;
  struct symbol **symref;
  struct symbol **dynsymref;

  int fd;
  bool fd_passed;

  
  bool has_merge_sections;
};


static inline int
ld_file_rel_p (struct usedfiles *file)
{
  return (elf_kind (file->elf) == ELF_K_ELF
	  && FILEINFO_EHDR (file->ehdr).e_type == ET_REL);
}

static inline int
ld_file_dso_p (struct usedfiles *file)
{
  return (elf_kind (file->elf) == ELF_K_ELF
	  && FILEINFO_EHDR (file->ehdr).e_type == ET_DYN);
}

static inline int
ld_file_ar_p (struct usedfiles *file)
{
  return elf_kind (file->elf) == ELF_K_AR;
}


struct pathelement
{
  
  struct pathelement *next;
  
  const char *pname;
  int exist;
};


struct ld_state;


struct callbacks
{
  const char **(*lib_extensions) (struct ld_state *)
       __attribute__ ((__const__));
#define LIB_EXTENSION(state) \
  DL_CALL_FCT ((state)->callbacks.lib_extensions, (state))

  int (*file_process) (int fd, struct usedfiles *, struct ld_state *,
		       struct usedfiles **);
#define FILE_PROCESS(fd, file, state, nextp) \
  DL_CALL_FCT ((state)->callbacks.file_process, (fd, file, state, nextp))

  
  int (*file_close) (struct usedfiles *, struct ld_state *);
#define FILE_CLOSE(file, state) \
  DL_CALL_FCT ((state)->callbacks.file_close, (file, state))

  void (*create_sections) (struct ld_state *);
#define CREATE_SECTIONS(state) \
  DL_CALL_FCT ((state)->callbacks.create_sections, (state))

  
  int (*flag_unresolved) (struct ld_state *);
#define FLAG_UNRESOLVED(state) \
  DL_CALL_FCT ((state)->callbacks.flag_unresolved, (state))

  void (*generate_sections) (struct ld_state *);
#define GENERATE_SECTIONS(state) \
  DL_CALL_FCT ((state)->callbacks.generate_sections, (state))

  int (*open_outfile) (struct ld_state *, int, int, int);
#define OPEN_OUTFILE(state, machine, class, data) \
  DL_CALL_FCT ((state)->callbacks.open_outfile, (state, machine, class, data))

  
  int (*create_outfile) (struct ld_state *);
#define CREATE_OUTFILE(state) \
  DL_CALL_FCT ((state)->callbacks.create_outfile, (state))

  
  void (*relocate_section) (struct ld_state *, Elf_Scn *, struct scninfo *,
			    const Elf32_Word *);
#define RELOCATE_SECTION(state, outscn, first, dblindirect) \
  DL_CALL_FCT ((state)->callbacks.relocate_section, (state, outscn, first,    \
						     dblindirect))

  void (*count_relocations) (struct ld_state *, struct scninfo *);
#define COUNT_RELOCATIONS(state, scninfo) \
  DL_CALL_FCT ((state)->callbacks.count_relocations, (state, scninfo))

  
  void (*create_relocations) (struct ld_state *, const Elf32_Word *);
#define CREATE_RELOCATIONS(state, dlbindirect) \
  DL_CALL_FCT ((state)->callbacks.create_relocations, (state, dblindirect))

  
  int (*finalize) (struct ld_state *);
#define FINALIZE(state) \
  DL_CALL_FCT ((state)->callbacks.finalize, (state))

  
  bool (*special_section_number_p) (struct ld_state *, size_t);
#define SPECIAL_SECTION_NUMBER_P(state, number) \
  DL_CALL_FCT ((state)->callbacks.special_section_number_p, (state, number))

  
  bool (*section_type_p) (struct ld_state *, XElf_Word);
#define SECTION_TYPE_P(state, type) \
  DL_CALL_FCT ((state)->callbacks.section_type_p, (state, type))

  
  XElf_Xword (*dynamic_section_flags) (struct ld_state *);
#define DYNAMIC_SECTION_FLAGS(state) \
  DL_CALL_FCT ((state)->callbacks.dynamic_section_flags, (state))

  
  void (*initialize_plt) (struct ld_state *, Elf_Scn *scn);
#define INITIALIZE_PLT(state, scn) \
  DL_CALL_FCT ((state)->callbacks.initialize_plt, (state, scn))

  
  void (*initialize_pltrel) (struct ld_state *, Elf_Scn *scn);
#define INITIALIZE_PLTREL(state, scn) \
  DL_CALL_FCT ((state)->callbacks.initialize_pltrel, (state, scn))

  
  void (*finalize_plt) (struct ld_state *, size_t, size_t, struct symbol **);
#define FINALIZE_PLT(state, nsym, nsym_dyn, ndxtosym) \
  DL_CALL_FCT ((state)->callbacks.finalize_plt, (state, nsym, nsym_dyn, \
						 ndxtosym))

  
  void (*initialize_got) (struct ld_state *, Elf_Scn *scn);
#define INITIALIZE_GOT(state, scn) \
  DL_CALL_FCT ((state)->callbacks.initialize_got, (state, scn))

  
  void (*initialize_gotplt) (struct ld_state *, Elf_Scn *scn);
#define INITIALIZE_GOTPLT(state, scn) \
  DL_CALL_FCT ((state)->callbacks.initialize_gotplt, (state, scn))

  int (*rel_type) (struct ld_state *);
#define REL_TYPE(state) \
  DL_CALL_FCT ((state)->callbacks.rel_type, (state))
};


struct symbol
{
  
  const char *name;
  
  XElf_Xword size;
  
  size_t symidx;
  
  size_t outsymidx;

  
  size_t scndx;
  struct usedfiles *file;
  
  Elf32_Word symscndx;

  unsigned int outdynsymidx:16;

  
  unsigned int type:4;
  
  unsigned int defined:1;
  unsigned int common:1;
  unsigned int weak:1;
  unsigned int added:1;
  unsigned int merged:1;
  unsigned int local:1;
  unsigned int hidden:1;
  
  unsigned int on_dsolist:1;
  unsigned int need_copy:1;
  unsigned int in_dso:1;

  union
  {
    void *handle;
    XElf_Addr value;
  } merge;

  
  struct symbol *next;
  struct symbol *previous;
  struct symbol *next_in_scn;
};


#include <symbolhash.h>

struct filename_list
{
  const char *name;
  struct usedfiles *real;
  struct filename_list *next;
  bool group_start;
  bool group_end;
  bool as_needed;
};


struct expression
{
  enum expression_tag
    {
      exp_num,
      exp_sizeof_headers,
      exp_pagesize,
      exp_id,
      exp_mult,
      exp_div,
      exp_mod,
      exp_plus,
      exp_minus,
      exp_and,
      exp_or,
      exp_align
    } tag;

  union
  {
    uintmax_t num;
    struct expression *child;
    struct
    {
      struct expression *left;
      struct expression *right;
    } binary;
    const char *str;
  } val;
};


struct input_section_name
{
  const char *name;
  bool sort_flag;
};

struct filemask_section_name
{
  const char *filemask;
  const char *excludemask;
  struct input_section_name *section_name;
  bool keep_flag;
};

struct assignment
{
  const char *variable;
  struct expression *expression;
  struct symbol *sym;
  bool provide_flag;
};


struct input_rule
{
  enum
    {
      input_section,
      input_assignment
    } tag;

  union
  {
    struct assignment *assignment;
    struct filemask_section_name *section;
  } val;

  struct input_rule *next;
};


struct output_section
{
  const char *name;
  struct input_rule *input;
  XElf_Addr max_alignment;
  bool ignored;
};


struct output_rule
{
  enum
    {
      output_section,
      output_assignment
    } tag;

  union
  {
    struct assignment *assignment;
    struct output_section section;
  } val;

  struct output_rule *next;
};


struct output_segment
{
  int mode;
  struct output_rule *output_rules;
  struct output_segment *next;

  XElf_Off offset;
  XElf_Addr addr;
  XElf_Xword align;
};


struct id_list
{
  union
  {
    enum id_type
      {
	id_str,		
	id_all,		
	id_wild		
      } id_type;
    struct
    {
      bool local;
      const char *versionname;
    } s;
  } u;
  const char *id;
  struct id_list *next;
};


struct version
{
  struct version *next;
  struct id_list *local_names;
  struct id_list *global_names;
  const char *versionname;
  const char *parentname;
};


struct scnhead
{
  
  const char *name;

  
  XElf_Xword flags;

  
  XElf_Word type;

  XElf_Word entsize;

  
  const char *grp_signature;

  
  XElf_Word align;

  enum scn_kind
    {
      scn_normal,		
      scn_dot_interp,		
      scn_dot_got,		
      scn_dot_gotplt,		
      scn_dot_dynrel,		
      scn_dot_dynamic,		
      scn_dot_dynsym,		
      scn_dot_dynstr,		
      scn_dot_hash,		
      scn_dot_gnu_hash,		
      scn_dot_plt,		
      scn_dot_pltrel,		
      scn_dot_version,		
      scn_dot_version_r,	
      scn_dot_note_gnu_build_id	
    } kind;

  
  bool used;

  
  size_t relsize;

  int segment_nr;

  XElf_Word scnidx;

  XElf_Word scnsymidx;

  
  XElf_Addr addr;

  struct Ebl_Strent *nameent;

  struct symbol *symbols;

  
  struct scninfo *last;
};


#include <sectionhash.h>

#include <versionhash.h>


struct ld_state
{
  
  Ebl *ebl;

  
  struct usedfiles *archives;
  
  struct usedfiles *tailarchives;
  
  bool group_start_requested;
  
  struct usedfiles *group_start_archive;

  
  struct usedfiles *dsofiles;
  
  size_t ndsofiles;
  
  struct usedfiles *relfiles;

  
  struct usedfiles *needed;

  
  struct filename_list *srcfiles;

  
  struct pathelement *paths;
  
  struct pathelement *tailpaths;

  
  struct pathelement *rpath;
  struct pathelement *rpath_link;
  struct pathelement *runpath;
  struct pathelement *runpath_link;
  struct Ebl_Strent *rxxpath_strent;
  int rxxpath_tag;

  
  struct pathelement *ld_library_path1;
  struct pathelement *ld_library_path2;

  
  const char *outfname;
  
  const char *tempfname;
  
  int outfd;
  
  Elf *outelf;

  
  enum file_type file_type;

  
  bool is_system_library;

  
  size_t pagesize;

  
  const char *interp;
  
  Elf32_Word interpscnidx;

  
  unsigned long int optlevel;

  
  bool statically;

  bool as_needed;

  
  enum extract_rule extract_rule;

  
  int last_archive_used;

  bool trace_files;

  bool muldefs;

  
  bool nodefs;

  
  bool add_ld_comment;

  
  enum
    {
      strip_none,
      strip_debug,
      strip_all,
      strip_everything
    } strip;

  
  struct callbacks callbacks;

  
  const char *entry;

  
  struct output_segment *output_segments;

  
  struct symbol *lscript_syms;
  size_t nlscript_syms;

  
  ld_symbol_tab symbol_tab;

  
  ld_section_tab section_tab;

  
  struct scnhead **allsections;
  size_t nallsections;
  size_t nusedsections;
  size_t nnotesections;

  
  struct symbol *unresolved;
  
  size_t nunresolved;
  
  size_t nunresolved_nonweak;

  
  struct symbol *common_syms;
  
  struct scninfo *common_section;

  struct symbol *from_dso;
  
  size_t nfrom_dso;
  
  size_t ndynsym;
  
  size_t nplt;
  
  size_t ngot;
  
  size_t ncopy;
  
  struct scninfo *copy_section;

  
  size_t nsymtab;
  size_t nlocalsymbols;

  
  struct symbol *init_symbol;
  struct symbol *fini_symbol;

  struct output_segment *default_output_segments;
  
  struct pathelement *default_paths;

#ifndef BASE_ELF_NAME
  
  void *ldlib;
#endif

  
  struct Ebl_Strtab *shstrtab;

  
  bool need_symtab;
  
  Elf32_Word symscnidx;
  
  Elf32_Word xndxscnidx;
  
  Elf32_Word strscnidx;

  
  bool need_dynsym;
  
  Elf32_Word dynsymscnidx;
  
  Elf32_Word dynstrscnidx;
  
  size_t hashscnidx;
  size_t gnuhashscnidx;

  
  Elf32_Word pltscnidx;
  
  size_t nplt_used;
  
  Elf32_Word pltrelscnidx;

  
  Elf32_Word gotscnidx;
  
  Elf32_Word gotpltscnidx;

  
  Elf32_Word reldynscnidx;

  
  Elf32_Word versymscnidx;
  Elf32_Word verneedscnidx;
  
  
  int nverdefused;
  
  int nverdeffile;
  
  int nextveridx;

  
  bool need_tls;
  XElf_Addr tls_start;
  XElf_Addr tls_tcb;

  ld_version_str_tab version_str_tab;
  bool default_bind_local;
  bool default_bind_global;

  
  enum execstack
    {
      execstack_false = 0,
      execstack_true,
      execstack_false_force
    } execstack;

  
  bool gc_sections;

  
  Elf32_Word *dblindirect;

  
  struct scngroup
  {
    Elf32_Word outscnidx;
    int nscns;
    struct member
    {
      struct scnhead *scn;
      struct member *next;
    } *member;
    struct Ebl_Strent *nameent;
    struct symbol *symbol;
    struct scngroup *next;
  } *groups;

  
  bool need_got;
  
  size_t nrel_got;

  
  int ndynamic;
  
  int ndynamic_filled;
  
  Elf32_Word dynamicscnidx;

  
  Elf32_Word dt_flags;
  
  Elf32_Word dt_flags_1;
  
  Elf32_Word dt_feature_1;

  
  bool lazyload;

  
  bool eh_frame_hdr;

  
  enum
    {
      hash_style_none = 0,
      hash_style_sysv = 1,
#define GENERATE_SYSV_HASH ((ld_state.hash_style & hash_style_sysv) != 0)
      hash_style_gnu = 2
#define GENERATE_GNU_HASH ((ld_state.hash_style & hash_style_gnu) != 0)
    }
  hash_style;


  bool export_all_dynamic;

  
  const char *build_id;
  Elf32_Word buildidscnidx;

  
  const char *soname;

  
  struct scninfo *rellist;
  
  size_t relsize_total;

  
  struct symbol *got_symbol;
  
  struct symbol *dyn_symbol;

  
  struct obstack smem;
};



extern int ldparse (void);

extern FILE *ldin;

extern const char *ldin_fname;

extern int ldlineno;

extern int ld_scan_version_script;

extern int verbose;
extern int conserve_memory;


extern struct ld_state ld_state;



extern void ld_new_searchdir (const char *dir);

extern struct usedfiles *ld_new_inputfile (const char *fname,
					   enum file_type type);



extern int ld_prepare_state (const char *emulation);


extern bool dynamically_linked_p (void);


extern bool linked_from_dso_p (struct scninfo *scninfo, size_t symidx);
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
extern inline bool
linked_from_dso_p (struct scninfo *scninfo, size_t symidx)
{
  struct usedfiles *file = scninfo->fileinfo;

  if (symidx < file->nlocalsymbols)
    return false;

  struct symbol *sym = file->symref[symidx];

  return sym->defined && sym->in_dso;
}

#endif	

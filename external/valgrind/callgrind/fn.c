
/*
   This file is part of Callgrind, a Valgrind tool for call tracing.

   Copyright (C) 2002-2013, Josef Weidendorfer (Josef.Weidendorfer@gmx.de)

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

#include "global.h"

#define N_INITIAL_FN_ARRAY_SIZE 10071

static fn_array current_fn_active;

static Addr runtime_resolve_addr = 0;
static int  runtime_resolve_length = 0;

struct chunk_t { int start, len; };
struct pattern
{
    const HChar* name;
    int len;
    struct chunk_t chunk[];
};

__attribute__((unused))    
static Bool check_code(obj_node* obj,
                       UChar code[], struct pattern* pat)
{
    Bool found;
    Addr addr, end;
    int chunk, start, len;

    CLG_ASSERT((pat->chunk[0].start == 0) && (pat->chunk[0].len >2));
    
    CLG_DEBUG(1, "check_code: %s, pattern %s, check %d bytes of [%x %x %x...]\n",
              obj->name, pat->name, pat->chunk[0].len, code[0], code[1], code[2]);

    end = obj->start + obj->size - pat->len;
    addr = obj->start;
    while(addr < end) {
	found = (VG_(memcmp)( (void*)addr, code, pat->chunk[0].len) == 0);

        if (found) {
	    chunk = 1;
	    while(1) {		
		start = pat->chunk[chunk].start;
		len   = pat->chunk[chunk].len;
		if (len == 0) break;

		CLG_ASSERT(len >2);
                CLG_DEBUG(1, " found chunk %d at %#lx, checking %d bytes "
                             "of [%x %x %x...]\n",
                          chunk-1, addr - obj->start, len,
			  code[start], code[start+1], code[start+2]);

                if (VG_(memcmp)( (void*)(addr+start), code+start, len) != 0) {
                    found = False;
                    break;
                }
		chunk++;
	    }

            if (found) {
		CLG_DEBUG(1, "found at offset %#lx.\n", addr - obj->start);
		if (VG_(clo_verbosity) > 1)
		    VG_(message)(Vg_DebugMsg, "Found runtime_resolve (%s): "
                                              "%s +%#lx=%#lx, length %d\n",
				 pat->name, obj->name + obj->last_slash_pos,
				 addr - obj->start, addr, pat->len);
		    
		runtime_resolve_addr   = addr;
		runtime_resolve_length = pat->len;
		return True;
	    }
        }
        addr++;
    }
    CLG_DEBUG(1, " found nothing.\n");
    return False;
}


static Bool search_runtime_resolve(obj_node* obj)
{
#if defined(VGP_x86_linux)
    static UChar code[] = {
	 0x50, 0x51, 0x52, 0x8b, 0x54, 0x24, 0x10, 0x8b,
	 0x44, 0x24, 0x0c, 0xe8, 0x70, 0x01, 0x00, 0x00,
	 0x5a, 0x59, 0x87, 0x04, 0x24, 0xc2, 0x08, 0x00 };
    
    static struct pattern pat = {
	"x86-def", 24, {{ 0,12 }, { 16,8 }, { 24,0}} };

    
    static UChar code_28[] = {
	 0x50, 0x51, 0x52, 0x8b, 0x54, 0x24, 0x10, 0x8b,
	 0x44, 0x24, 0x0c, 0xe8, 0x70, 0x01, 0x00, 0x00,
	 0x5a, 0x8b, 0x0c, 0x24, 0x89, 0x04, 0x24, 0x8b,
	 0x44, 0x24, 0x04, 0xc2, 0x0c, 0x00 };
    static struct pattern pat_28 = {
	"x86-glibc2.8", 30, {{ 0,12 }, { 16,14 }, { 30,0}} };

    if (VG_(strncmp)(obj->name, "/lib/ld", 7) != 0) return False;
    if (check_code(obj, code, &pat)) return True;
    if (check_code(obj, code_28, &pat_28)) return True;
    return False;
#endif

#if defined(VGP_ppc32_linux)
    static UChar code[] = {
	 0x94, 0x21, 0xff, 0xc0, 0x90, 0x01, 0x00, 0x0c,
	 0x90, 0x61, 0x00, 0x10, 0x90, 0x81, 0x00, 0x14,
	 0x7d, 0x83, 0x63, 0x78, 0x90, 0xa1, 0x00, 0x18,
	 0x7d, 0x64, 0x5b, 0x78, 0x90, 0xc1, 0x00, 0x1c,
	 0x7c, 0x08, 0x02, 0xa6, 0x90, 0xe1, 0x00, 0x20,
	 0x90, 0x01, 0x00, 0x30, 0x91, 0x01, 0x00, 0x24,
	 0x7c, 0x00, 0x00, 0x26, 0x91, 0x21, 0x00, 0x28,
	 0x91, 0x41, 0x00, 0x2c, 0x90, 0x01, 0x00, 0x08,
	 0x48, 0x00, 0x02, 0x91, 0x7c, 0x69, 0x03, 0xa6, 
	 0x80, 0x01, 0x00, 0x30, 0x81, 0x41, 0x00, 0x2c,
	 0x81, 0x21, 0x00, 0x28, 0x7c, 0x08, 0x03, 0xa6,
	 0x81, 0x01, 0x00, 0x24, 0x80, 0x01, 0x00, 0x08,
	 0x80, 0xe1, 0x00, 0x20, 0x80, 0xc1, 0x00, 0x1c,
	0x7c, 0x0f, 0xf1, 0x20, 0x80, 0xa1, 0x00, 0x18,
	0x80, 0x81, 0x00, 0x14, 0x80, 0x61, 0x00, 0x10,
	0x80, 0x01, 0x00, 0x0c, 0x38, 0x21, 0x00, 0x40,
	0x4e, 0x80, 0x04, 0x20 };
    static struct pattern pat = {
	"ppc32-def", 132, {{ 0,65 }, { 68,64 }, { 132,0 }} };

    if (VG_(strncmp)(obj->name, "/lib/ld", 7) != 0) return False;
    return check_code(obj, code, &pat);
#endif

#if defined(VGP_amd64_linux)
    static UChar code[] = {
	 0x48, 0x83, 0xec, 0x38, 0x48, 0x89, 0x04, 0x24,
	 0x48, 0x89, 0x4c, 0x24, 0x08, 0x48, 0x89, 0x54, 0x24, 0x10,
	 0x48, 0x89, 0x74, 0x24, 0x18, 0x48, 0x89, 0x7c, 0x24, 0x20,
	 0x4c, 0x89, 0x44, 0x24, 0x28, 0x4c, 0x89, 0x4c, 0x24, 0x30,
	 0x48, 0x8b, 0x74, 0x24, 0x40, 0x49, 0x89, 0xf3,
	 0x4c, 0x01, 0xde, 0x4c, 0x01, 0xde, 0x48, 0xc1, 0xe6, 0x03,
	 0x48, 0x8b, 0x7c, 0x24, 0x38, 0xe8, 0xee, 0x01, 0x00, 0x00,
	 0x49, 0x89, 0xc3, 0x4c, 0x8b, 0x4c, 0x24, 0x30,
	 0x4c, 0x8b, 0x44, 0x24, 0x28, 0x48, 0x8b, 0x7c, 0x24, 0x20,
	 0x48, 0x8b, 0x74, 0x24, 0x18, 0x48, 0x8b, 0x54, 0x24, 0x10,
	 0x48, 0x8b, 0x4c, 0x24, 0x08, 0x48, 0x8b, 0x04, 0x24,
	0x48, 0x83, 0xc4, 0x48, 0x41, 0xff, 0xe3 };
    static struct pattern pat = {
	"amd64-def", 110, {{ 0,62 }, { 66,44 }, { 110,0 }} };

    if ((VG_(strncmp)(obj->name, "/lib/ld", 7) != 0) &&
	(VG_(strncmp)(obj->name, "/lib64/ld", 9) != 0)) return False;
    return check_code(obj, code, &pat);
#endif

    
    return False;
}



static obj_node* obj_table[N_OBJ_ENTRIES];

void CLG_(init_obj_table)()
{
    Int i;
    for (i = 0; i < N_OBJ_ENTRIES; i++)
	obj_table[i] = 0;
}

#define HASH_CONSTANT   256

static UInt str_hash(const HChar *s, UInt table_size)
{
    int hash_value = 0;
    for ( ; *s; s++)
        hash_value = (HASH_CONSTANT * hash_value + *s) % table_size;
    return hash_value;
}


static const HChar* anonymous_obj = "???";

static __inline__ 
obj_node* new_obj_node(DebugInfo* di, obj_node* next)
{
   Int i;
   obj_node* obj;

   obj = (obj_node*) CLG_MALLOC("cl.fn.non.1", sizeof(obj_node));
   obj->name  = di ? VG_(strdup)( "cl.fn.non.2",
                                  VG_(DebugInfo_get_filename)(di) )
                   : anonymous_obj;
   for (i = 0; i < N_FILE_ENTRIES; i++) {
      obj->files[i] = NULL;
   }
   CLG_(stat).distinct_objs ++;
   obj->number  = CLG_(stat).distinct_objs;
   obj->start   = di ? VG_(DebugInfo_get_text_avma)(di) : 0;
   obj->size    = di ? VG_(DebugInfo_get_text_size)(di) : 0;
   obj->offset  = di ? VG_(DebugInfo_get_text_bias)(di) : 0;
   obj->next    = next;

   
   obj->last_slash_pos = 0;
   i = 0;
   while(obj->name[i]) {
	if (obj->name[i]=='/') obj->last_slash_pos = i+1;
	i++;
   }

   if (runtime_resolve_addr == 0) search_runtime_resolve(obj);

   return obj;
}

obj_node* CLG_(get_obj_node)(DebugInfo* di)
{
    obj_node*    curr_obj_node;
    UInt         objname_hash;
    const HChar* obj_name;
    
    obj_name = di ? VG_(DebugInfo_get_filename)(di) : anonymous_obj;

    
    objname_hash = str_hash(obj_name, N_OBJ_ENTRIES);
    curr_obj_node = obj_table[objname_hash];
    while (NULL != curr_obj_node && 
	   VG_(strcmp)(obj_name, curr_obj_node->name) != 0) {
	curr_obj_node = curr_obj_node->next;
    }
    if (NULL == curr_obj_node) {
	obj_table[objname_hash] = curr_obj_node = 
	    new_obj_node(di, obj_table[objname_hash]);
    }

    return curr_obj_node;
}


static __inline__ 
file_node* new_file_node(const HChar *filename,
			 obj_node* obj, file_node* next)
{
  Int i;
  file_node* file = (file_node*) CLG_MALLOC("cl.fn.nfn.1",
                                           sizeof(file_node));
  file->name  = VG_(strdup)("cl.fn.nfn.2", filename);
  for (i = 0; i < N_FN_ENTRIES; i++) {
    file->fns[i] = NULL;
  }
  CLG_(stat).distinct_files++;
  file->number  = CLG_(stat).distinct_files;
  file->obj     = obj;
  file->next      = next;
  return file;
}

 
file_node* CLG_(get_file_node)(obj_node* curr_obj_node,
                               const HChar *dir, const HChar *file)
{
    file_node* curr_file_node;
    UInt       filename_hash;

    
    HChar filename[VG_(strlen)(dir) + 1 + VG_(strlen)(file) + 1];
    VG_(strcpy)(filename, dir);
    if (filename[0] != '\0') {
       VG_(strcat)(filename, "/");
    }
    VG_(strcat)(filename, file);

    
    filename_hash = str_hash(filename, N_FILE_ENTRIES);
    curr_file_node = curr_obj_node->files[filename_hash];
    while (NULL != curr_file_node && 
	   VG_(strcmp)(filename, curr_file_node->name) != 0) {
	curr_file_node = curr_file_node->next;
    }
    if (NULL == curr_file_node) {
	curr_obj_node->files[filename_hash] = curr_file_node = 
	    new_file_node(filename, curr_obj_node, 
			  curr_obj_node->files[filename_hash]);
    }

    return curr_file_node;
}

static void resize_fn_array(void);

static __inline__ 
fn_node* new_fn_node(const HChar *fnname,
		     file_node* file, fn_node* next)
{
    fn_node* fn = (fn_node*) CLG_MALLOC("cl.fn.nfnnd.1",
                                         sizeof(fn_node));
    fn->name = VG_(strdup)("cl.fn.nfnnd.2", fnname);

    CLG_(stat).distinct_fns++;
    fn->number   = CLG_(stat).distinct_fns;
    fn->last_cxt = 0;
    fn->pure_cxt = 0;
    fn->file     = file;
    fn->next     = next;

    fn->dump_before  = False;
    fn->dump_after   = False;
    fn->zero_before  = False;
    fn->toggle_collect = False;
    fn->skip         = False;
    fn->pop_on_jump  = CLG_(clo).pop_on_jump;
    fn->is_malloc    = False;
    fn->is_realloc   = False;
    fn->is_free      = False;

    fn->group        = 0;
    fn->separate_callers    = CLG_(clo).separate_callers;
    fn->separate_recursions = CLG_(clo).separate_recursions;

#if CLG_ENABLE_DEBUG
    fn->verbosity    = -1;
#endif

    if (CLG_(stat).distinct_fns >= current_fn_active.size)
	resize_fn_array();

    return fn;
}


static
fn_node* get_fn_node_infile(file_node* curr_file_node,
			    const HChar *fnname)
{
    fn_node* curr_fn_node;
    UInt     fnname_hash;

    CLG_ASSERT(curr_file_node != 0);

    
    fnname_hash = str_hash(fnname, N_FN_ENTRIES);
    curr_fn_node = curr_file_node->fns[fnname_hash];
    while (NULL != curr_fn_node && 
	   VG_(strcmp)(fnname, curr_fn_node->name) != 0) {
	curr_fn_node = curr_fn_node->next;
    }
    if (NULL == curr_fn_node) {
	curr_file_node->fns[fnname_hash] = curr_fn_node = 
            new_fn_node(fnname, curr_file_node,
			curr_file_node->fns[fnname_hash]);
    }

    return curr_fn_node;
}


static __inline__
fn_node* get_fn_node_inseg(DebugInfo* di,
			   const HChar *dirname,
			   const HChar *filename,
			   const HChar *fnname)
{
  obj_node  *obj  = CLG_(get_obj_node)(di);
  file_node *file = CLG_(get_file_node)(obj, dirname, filename);
  fn_node   *fn   = get_fn_node_infile(file, fnname);

  return fn;
}


Bool CLG_(get_debug_info)(Addr instr_addr,
                          const HChar **dir,
                          const HChar **file,
                          const HChar **fn_name, UInt* line_num,
                          DebugInfo** pDebugInfo)
{
  Bool found_file_line, found_fn, result = True;
  UInt line;
  
  CLG_DEBUG(6, "  + get_debug_info(%#lx)\n", instr_addr);

  if (pDebugInfo) {
      *pDebugInfo = VG_(find_DebugInfo)(instr_addr);

      
   }

   found_file_line = VG_(get_filename_linenum)(instr_addr,
					       file,
					       dir,
					       &line);
   found_fn = VG_(get_fnname)(instr_addr, fn_name);

   if (!found_file_line && !found_fn) {
     CLG_(stat).no_debug_BBs++;
     *file = "???";
     *fn_name = "???";
     if (line_num) *line_num=0;
     result = False;

   } else if ( found_file_line &&  found_fn) {
     CLG_(stat).full_debug_BBs++;
     if (line_num) *line_num=line;

   } else if ( found_file_line && !found_fn) {
     CLG_(stat).file_line_debug_BBs++;
     *fn_name = "???";
     if (line_num) *line_num=line;

   } else   {
     CLG_(stat).fn_name_debug_BBs++;
     *file = "???";
     if (line_num) *line_num=0;
   }

   CLG_DEBUG(6, "  - get_debug_info(%#lx): seg '%s', fn %s\n",
	    instr_addr,
	    !pDebugInfo   ? "-" :
	    (*pDebugInfo) ? VG_(DebugInfo_get_filename)(*pDebugInfo) :
	    "(None)",
	    *fn_name);

  return result;
}

static BB* exit_bb = 0;


fn_node* CLG_(get_fn_node)(BB* bb)
{
    const HChar *fnname, *filename, *dirname;
    DebugInfo* di;
    UInt       line_num;
    fn_node*   fn;

    
    if (bb->fn) return bb->fn;

    CLG_DEBUG(3,"+ get_fn_node(BB %#lx)\n", bb_addr(bb));

    CLG_(get_debug_info)(bb_addr(bb),
                         &dirname, &filename, &fnname, &line_num, &di);

    if (0 == VG_(strcmp)(fnname, "???")) {
	int p;
        static HChar buf[32];  
	
	if (sizeof(Addr) == 4)
	    p = VG_(sprintf)(buf, "%#08lx", bb->offset);
	else 	    
	    
	    p = VG_(sprintf)(buf, "%#016lx", bb->offset);

	VG_(sprintf)(buf + p, "%s", 
		     (bb->sect_kind == Vg_SectData) ? " [Data]" :
		     (bb->sect_kind == Vg_SectBSS)  ? " [BSS]"  :
		     (bb->sect_kind == Vg_SectGOT)  ? " [GOT]"  :
		     (bb->sect_kind == Vg_SectPLT)  ? " [PLT]"  : "");
        fnname = buf;
    }
    else {
      if (VG_(get_fnname_if_entry)(bb_addr(bb), &fnname))
	bb->is_entry = 1;
    }

    if (0 == VG_(strcmp)(fnname, "vgPlain___libc_freeres_wrapper")
	&& exit_bb) {
      CLG_(get_debug_info)(bb_addr(exit_bb),
                           &dirname, &filename, &fnname, &line_num, &di);
	
	CLG_DEBUG(1, "__libc_freeres_wrapper renamed to _exit\n");
    }
    if (0 == VG_(strcmp)(fnname, "_exit") && !exit_bb)
	exit_bb = bb;
    
    if (runtime_resolve_addr && 
	(bb_addr(bb) >= runtime_resolve_addr) &&
	(bb_addr(bb) < runtime_resolve_addr + runtime_resolve_length)) {
	
      fnname = "_dl_runtime_resolve";
    }

    
    fn = get_fn_node_inseg( di, dirname, filename, fnname);

    if (fn->pure_cxt == 0) {

      fn_node* pure[2];
      pure[0] = 0;
      pure[1] = fn;
      fn->pure_cxt = CLG_(get_cxt)(pure+1);

      if (bb->sect_kind == Vg_SectPLT)	
	fn->skip = CLG_(clo).skip_plt;

      if (VG_(strcmp)(fn->name, "_dl_runtime_resolve")==0) {
	  fn->pop_on_jump = True;

	  if (VG_(clo_verbosity) > 1)
	      VG_(message)(Vg_DebugMsg, "Symbol match: found runtime_resolve:"
                                        " %s +%#lx=%#lx\n",
		      bb->obj->name + bb->obj->last_slash_pos,
		      bb->offset, bb_addr(bb));
      }

      fn->is_malloc  = (VG_(strcmp)(fn->name, "malloc")==0);
      fn->is_realloc = (VG_(strcmp)(fn->name, "realloc")==0);
      fn->is_free    = (VG_(strcmp)(fn->name, "free")==0);

      CLG_(update_fn_config)(fn);
    }


    bb->fn   = fn;
    bb->line = line_num;

    if (dirname[0]) {
       CLG_DEBUG(3,"- get_fn_node(BB %#lx): %s (in %s:%u)\n",
                 bb_addr(bb), fnname, filename, line_num);
    } else
       CLG_DEBUG(3,"- get_fn_node(BB %#lx): %s (in %s/%s:%u)\n",
                 bb_addr(bb), fnname, dirname, filename, line_num);

    return fn;
}




UInt* CLG_(get_fn_entry)(Int n)
{
  CLG_ASSERT(n < current_fn_active.size);
  return current_fn_active.array + n;
}

void CLG_(init_fn_array)(fn_array* a)
{
  Int i;

  CLG_ASSERT(a != 0);

  a->size = N_INITIAL_FN_ARRAY_SIZE;
  if (a->size <= CLG_(stat).distinct_fns)
    a->size = CLG_(stat).distinct_fns+1;
  
  a->array = (UInt*) CLG_MALLOC("cl.fn.gfe.1",
                                a->size * sizeof(UInt));
  for(i=0;i<a->size;i++)
    a->array[i] = 0;
}

void CLG_(copy_current_fn_array)(fn_array* dst)
{
  CLG_ASSERT(dst != 0);

  dst->size  = current_fn_active.size;
  dst->array = current_fn_active.array;
}

fn_array* CLG_(get_current_fn_array)()
{
  return &current_fn_active;
}

void CLG_(set_current_fn_array)(fn_array* a)
{
  CLG_ASSERT(a != 0);

  current_fn_active.size  = a->size;
  current_fn_active.array = a->array;
  if (current_fn_active.size <= CLG_(stat).distinct_fns)
    resize_fn_array();
}

static void resize_fn_array(void)
{
    UInt* new_array;
    Int i, newsize;

    newsize = current_fn_active.size;
    while (newsize <= CLG_(stat).distinct_fns) newsize *=2;

    CLG_DEBUG(0, "Resize fn_active_array: %d => %d\n",
	     current_fn_active.size, newsize);

    new_array = (UInt*) CLG_MALLOC("cl.fn.rfa.1", newsize * sizeof(UInt));
    for(i=0;i<current_fn_active.size;i++)
      new_array[i] = current_fn_active.array[i];
    while(i<newsize)
	new_array[i++] = 0;

    VG_(free)(current_fn_active.array);
    current_fn_active.size = newsize;
    current_fn_active.array = new_array;
    CLG_(stat).fn_array_resizes++;
}



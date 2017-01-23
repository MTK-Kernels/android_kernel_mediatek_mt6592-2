/* Defs for interface to demanglers.
   Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998, 2000, 2001, 2002,
   2003, 2004, 2005, 2007, 2008, 2009, 2010 Free Software Foundation, Inc.
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2, or
   (at your option) any later version.

   In addition to the permissions in the GNU Library General Public
   License, the Free Software Foundation gives you unlimited
   permission to link the compiled version of this file into
   combinations with other programs, and to distribute those
   combinations without any restriction coming from the use of this
   file.  (The Library Public License restrictions do apply in other
   respects; for example, they cover modification of the file, and
   distribution when not linked into a combined executable.)

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */


#if !defined (DEMANGLE_H)
#define DEMANGLE_H

#if 0 
#include "libiberty.h"
#endif 

#ifdef __cplusplus
extern "C" {
#endif 


#define DMGL_NO_OPTS	 0		
#define DMGL_PARAMS	 (1 << 0)	
#define DMGL_ANSI	 (1 << 1)	
#define DMGL_JAVA	 (1 << 2)	
#define DMGL_VERBOSE	 (1 << 3)	
#define DMGL_TYPES	 (1 << 4)	
#define DMGL_RET_POSTFIX (1 << 5)       
#define DMGL_RET_DROP	 (1 << 6)       

#define DMGL_AUTO	 (1 << 8)
#define DMGL_GNU	 (1 << 9)
#define DMGL_LUCID	 (1 << 10)
#define DMGL_ARM	 (1 << 11)
#define DMGL_HP 	 (1 << 12)       
#define DMGL_EDG	 (1 << 13)
#define DMGL_GNU_V3	 (1 << 14)
#define DMGL_GNAT	 (1 << 15)

#define DMGL_STYLE_MASK (DMGL_AUTO|DMGL_GNU|DMGL_LUCID|DMGL_ARM|DMGL_HP|DMGL_EDG|DMGL_GNU_V3|DMGL_JAVA|DMGL_GNAT)


extern enum demangling_styles
{
  no_demangling = -1,
  unknown_demangling = 0,
  auto_demangling = DMGL_AUTO,
  gnu_demangling = DMGL_GNU,
  lucid_demangling = DMGL_LUCID,
  arm_demangling = DMGL_ARM,
  hp_demangling = DMGL_HP,
  edg_demangling = DMGL_EDG,
  gnu_v3_demangling = DMGL_GNU_V3,
  java_demangling = DMGL_JAVA,
  gnat_demangling = DMGL_GNAT
} current_demangling_style;


#define NO_DEMANGLING_STYLE_STRING            "none"
#define AUTO_DEMANGLING_STYLE_STRING	      "auto"
#define GNU_DEMANGLING_STYLE_STRING    	      "gnu"
#define LUCID_DEMANGLING_STYLE_STRING	      "lucid"
#define ARM_DEMANGLING_STYLE_STRING	      "arm"
#define HP_DEMANGLING_STYLE_STRING	      "hp"
#define EDG_DEMANGLING_STYLE_STRING	      "edg"
#define GNU_V3_DEMANGLING_STYLE_STRING        "gnu-v3"
#define JAVA_DEMANGLING_STYLE_STRING          "java"
#define GNAT_DEMANGLING_STYLE_STRING          "gnat"


#define CURRENT_DEMANGLING_STYLE current_demangling_style
#define AUTO_DEMANGLING (((int) CURRENT_DEMANGLING_STYLE) & DMGL_AUTO)
#define GNU_DEMANGLING (((int) CURRENT_DEMANGLING_STYLE) & DMGL_GNU)
#define LUCID_DEMANGLING (((int) CURRENT_DEMANGLING_STYLE) & DMGL_LUCID)
#define ARM_DEMANGLING (((int) CURRENT_DEMANGLING_STYLE) & DMGL_ARM)
#define HP_DEMANGLING (((int) CURRENT_DEMANGLING_STYLE) & DMGL_HP)
#define EDG_DEMANGLING (((int) CURRENT_DEMANGLING_STYLE) & DMGL_EDG)
#define GNU_V3_DEMANGLING (((int) CURRENT_DEMANGLING_STYLE) & DMGL_GNU_V3)
#define JAVA_DEMANGLING (((int) CURRENT_DEMANGLING_STYLE) & DMGL_JAVA)
#define GNAT_DEMANGLING (((int) CURRENT_DEMANGLING_STYLE) & DMGL_GNAT)


extern const struct demangler_engine
{
  const char *const demangling_style_name;
  const enum demangling_styles demangling_style;
  const char *const demangling_style_doc;
} libiberty_demanglers[];

extern char *
ML_(cplus_demangle) (const char *mangled, int options);

extern int
cplus_demangle_opname (const char *opname, char *result, int options);

extern const char *
cplus_mangle_opname (const char *opname, int options);


extern void
set_cplus_marker_for_demangling (int ch);

extern enum demangling_styles 
cplus_demangle_set_style (enum demangling_styles style);

extern enum demangling_styles 
cplus_demangle_name_to_style (const char *name);

typedef void (*demangle_callbackref) (const char *, size_t, void *);

extern int
cplus_demangle_v3_callback (const char *mangled, int options,
                            demangle_callbackref callback, void *opaque);

extern char*
cplus_demangle_v3 (const char *mangled, int options);

extern int
java_demangle_v3_callback (const char *mangled,
                           demangle_callbackref callback, void *opaque);

extern char*
java_demangle_v3 (const char *mangled);

char *
ada_demangle (const char *mangled, int options);

enum gnu_v3_ctor_kinds {
  gnu_v3_complete_object_ctor = 1,
  gnu_v3_base_object_ctor,
  gnu_v3_complete_object_allocating_ctor,
  gnu_v3_unified_ctor,
  gnu_v3_object_ctor_group
};

extern enum gnu_v3_ctor_kinds
	is_gnu_v3_mangled_ctor (const char *name);


enum gnu_v3_dtor_kinds {
  gnu_v3_deleting_dtor = 1,
  gnu_v3_complete_object_dtor,
  gnu_v3_base_object_dtor,
  gnu_v3_unified_dtor,
  gnu_v3_object_dtor_group
};

extern enum gnu_v3_dtor_kinds
	is_gnu_v3_mangled_dtor (const char *name);



enum demangle_component_type
{
  
  DEMANGLE_COMPONENT_NAME,
  DEMANGLE_COMPONENT_QUAL_NAME,
  DEMANGLE_COMPONENT_LOCAL_NAME,
  DEMANGLE_COMPONENT_TYPED_NAME,
  DEMANGLE_COMPONENT_TEMPLATE,
  DEMANGLE_COMPONENT_TEMPLATE_PARAM,
  
  DEMANGLE_COMPONENT_FUNCTION_PARAM,
  DEMANGLE_COMPONENT_CTOR,
  
  DEMANGLE_COMPONENT_DTOR,
  DEMANGLE_COMPONENT_VTABLE,
  DEMANGLE_COMPONENT_VTT,
  DEMANGLE_COMPONENT_CONSTRUCTION_VTABLE,
  DEMANGLE_COMPONENT_TYPEINFO,
  DEMANGLE_COMPONENT_TYPEINFO_NAME,
  DEMANGLE_COMPONENT_TYPEINFO_FN,
  DEMANGLE_COMPONENT_THUNK,
  DEMANGLE_COMPONENT_VIRTUAL_THUNK,
  DEMANGLE_COMPONENT_COVARIANT_THUNK,
  
  DEMANGLE_COMPONENT_JAVA_CLASS,
  DEMANGLE_COMPONENT_GUARD,
  
  DEMANGLE_COMPONENT_TLS_INIT,
  DEMANGLE_COMPONENT_TLS_WRAPPER,
  DEMANGLE_COMPONENT_REFTEMP,
  DEMANGLE_COMPONENT_HIDDEN_ALIAS,
  DEMANGLE_COMPONENT_SUB_STD,
  DEMANGLE_COMPONENT_RESTRICT,
  DEMANGLE_COMPONENT_VOLATILE,
  DEMANGLE_COMPONENT_CONST,
  DEMANGLE_COMPONENT_RESTRICT_THIS,
  DEMANGLE_COMPONENT_VOLATILE_THIS,
  DEMANGLE_COMPONENT_CONST_THIS,
  DEMANGLE_COMPONENT_REFERENCE_THIS,
  DEMANGLE_COMPONENT_RVALUE_REFERENCE_THIS,
  DEMANGLE_COMPONENT_VENDOR_TYPE_QUAL,
  DEMANGLE_COMPONENT_POINTER,
  DEMANGLE_COMPONENT_REFERENCE,
  DEMANGLE_COMPONENT_RVALUE_REFERENCE,
  
  DEMANGLE_COMPONENT_COMPLEX,
  
  DEMANGLE_COMPONENT_IMAGINARY,
  
  DEMANGLE_COMPONENT_BUILTIN_TYPE,
  
  DEMANGLE_COMPONENT_VENDOR_TYPE,
  DEMANGLE_COMPONENT_FUNCTION_TYPE,
  DEMANGLE_COMPONENT_ARRAY_TYPE,
  DEMANGLE_COMPONENT_PTRMEM_TYPE,
  
  DEMANGLE_COMPONENT_FIXED_TYPE,
  DEMANGLE_COMPONENT_VECTOR_TYPE,
  DEMANGLE_COMPONENT_ARGLIST,
  DEMANGLE_COMPONENT_TEMPLATE_ARGLIST,
  DEMANGLE_COMPONENT_INITIALIZER_LIST,
  DEMANGLE_COMPONENT_OPERATOR,
  DEMANGLE_COMPONENT_EXTENDED_OPERATOR,
  DEMANGLE_COMPONENT_CAST,
  
  DEMANGLE_COMPONENT_NULLARY,
  DEMANGLE_COMPONENT_UNARY,
  DEMANGLE_COMPONENT_BINARY,
  DEMANGLE_COMPONENT_BINARY_ARGS,
  DEMANGLE_COMPONENT_TRINARY,
  DEMANGLE_COMPONENT_TRINARY_ARG1,
  DEMANGLE_COMPONENT_TRINARY_ARG2,
  DEMANGLE_COMPONENT_LITERAL,
  DEMANGLE_COMPONENT_LITERAL_NEG,
  DEMANGLE_COMPONENT_JAVA_RESOURCE,
  DEMANGLE_COMPONENT_COMPOUND_NAME,
  
  DEMANGLE_COMPONENT_CHARACTER,
  
  DEMANGLE_COMPONENT_NUMBER,
  
  DEMANGLE_COMPONENT_DECLTYPE,
  
  DEMANGLE_COMPONENT_GLOBAL_CONSTRUCTORS,
  
  DEMANGLE_COMPONENT_GLOBAL_DESTRUCTORS,
  
  DEMANGLE_COMPONENT_LAMBDA,
  
  DEMANGLE_COMPONENT_DEFAULT_ARG,
  
  DEMANGLE_COMPONENT_UNNAMED_TYPE,
  DEMANGLE_COMPONENT_TRANSACTION_CLONE,
  DEMANGLE_COMPONENT_NONTRANSACTION_CLONE,
  
  DEMANGLE_COMPONENT_PACK_EXPANSION,
  
  DEMANGLE_COMPONENT_TAGGED_NAME,
  
  DEMANGLE_COMPONENT_CLONE
};


struct demangle_operator_info;
struct demangle_builtin_type_info;


struct demangle_component
{
  
  enum demangle_component_type type;

  union
  {
    
    struct
    {
      const char *s;
      int len;
    } s_name;

    
    struct
    {
      
      const struct demangle_operator_info *op;
    } s_operator;

    
    struct
    {
      
      int args;
      
      struct demangle_component *name;
    } s_extended_operator;

    
    struct
    {
      
      struct demangle_component *length;
      
      short accum;
      
      short sat;
    } s_fixed;

    
    struct
    {
      
      enum gnu_v3_ctor_kinds kind;
      
      struct demangle_component *name;
    } s_ctor;

    
    struct
    {
      
      enum gnu_v3_dtor_kinds kind;
      
      struct demangle_component *name;
    } s_dtor;

    
    struct
    {
      
      const struct demangle_builtin_type_info *type;
    } s_builtin;

    
    struct
    {
      
      const char* string;
      
      int len;
    } s_string;

    
    struct
    {
      
      long number;
    } s_number;

    
    struct
    {
      int character;
    } s_character;

    
    struct
    {
      
      struct demangle_component *left;
      
      struct demangle_component *right;
    } s_binary;

    struct
    {
      
      struct demangle_component *sub;
      
      int num;
    } s_unary_num;

  } u;
};



extern int
cplus_demangle_fill_component (struct demangle_component *fill,
                               enum demangle_component_type,
                               struct demangle_component *left,
                               struct demangle_component *right);


extern int
cplus_demangle_fill_name (struct demangle_component *fill,
                          const char *, int);


extern int
cplus_demangle_fill_builtin_type (struct demangle_component *fill,
                                  const char *type_name);


extern int
cplus_demangle_fill_operator (struct demangle_component *fill,
                              const char *opname, int args);


extern int
cplus_demangle_fill_extended_operator (struct demangle_component *fill,
                                       int numargs,
                                       struct demangle_component *nm);


extern int
cplus_demangle_fill_ctor (struct demangle_component *fill,
                          enum gnu_v3_ctor_kinds kind,
                          struct demangle_component *name);


extern int
cplus_demangle_fill_dtor (struct demangle_component *fill,
                          enum gnu_v3_dtor_kinds kind,
                          struct demangle_component *name);


extern struct demangle_component *
cplus_demangle_v3_components (const char *mangled, int options, void **mem);


extern char *
cplus_demangle_print (int options,
                      const struct demangle_component *tree,
                      int estimated_length,
                      size_t *p_allocated_size);


extern int
cplus_demangle_print_callback (int options,
                               const struct demangle_component *tree,
                               demangle_callbackref callback, void *opaque);

#ifdef __cplusplus
}
#endif 

#endif	

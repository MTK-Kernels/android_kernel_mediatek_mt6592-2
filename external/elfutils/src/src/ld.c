/* Copyright (C) 2001-2010, 2012 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <argp.h>
#include <assert.h>
#include <error.h>
#include <fcntl.h>
#include <libelf.h>
#include <libintl.h>
#include <locale.h>
#include <mcheck.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <system.h>
#include "ld.h"
#include "list.h"


static void print_version (FILE *stream, struct argp_state *state);
ARGP_PROGRAM_VERSION_HOOK_DEF = print_version;

ARGP_PROGRAM_BUG_ADDRESS_DEF = PACKAGE_BUGREPORT;


enum
  {
    ARGP_whole_archive = 300,
    ARGP_no_whole_archive,
    ARGP_static,
    ARGP_dynamic,
    ARGP_pagesize,
    ARGP_rpath_link,
    ARGP_runpath,
    ARGP_runpath_link,
    ARGP_version_script,
    ARGP_gc_sections,
    ARGP_no_gc_sections,
    ARGP_no_undefined,
    ARGP_conserve,
    ARGP_as_needed,
    ARGP_no_as_needed,
    ARGP_eh_frame_hdr,
    ARGP_hash_style,
    ARGP_build_id,
#if YYDEBUG
    ARGP_yydebug,
#endif
  };


static const struct argp_option options[] =
{
  { NULL, 0, NULL, 0, N_("Input File Control:"), 0 },
  { "whole-archive", ARGP_whole_archive, NULL, 0,
    N_("Include whole archives in the output from now on."), 0 },
  { "no-whole-archive", ARGP_no_whole_archive, NULL, 0,
    N_("Stop including the whole archives in the output."), 0 },
  { NULL, 'l', N_("FILE"), OPTION_HIDDEN, NULL, 0 },
  { "start-group", '(', NULL, 0, N_("Start a group."), 0 },
  { "end-group", ')', NULL, 0, N_("End a group."), 0 },
  { NULL, 'L', N_("PATH"), 0,
    N_("Add PATH to list of directories files are searched in."), 0 },
  { "as-needed", ARGP_as_needed, NULL, 0,
    N_("Only set DT_NEEDED for following dynamic libs if actually used"), 0 },
  { "no-as-needed", ARGP_no_as_needed, NULL, 0,
    N_("Always set DT_NEEDED for following dynamic libs"), 0 },
  { "rpath-link", ARGP_rpath_link, "PATH", OPTION_HIDDEN, NULL, 0 },
  { NULL, 'i', NULL, 0, N_("Ignore LD_LIBRARY_PATH environment variable."),
    0 },

  { NULL, 0, NULL, 0, N_("Output File Control:"), 0 },
  { "output", 'o', N_("FILE"), 0, N_("Place output in FILE."), 0 },
  { NULL, 'z', "KEYWORD", OPTION_HIDDEN, NULL, 0 },
  { "-z nodefaultlib", '\0', NULL, OPTION_DOC,
    N_("Object is marked to not use default search path at runtime."), 0 },
  { "-z allextract", '\0', NULL, OPTION_DOC,
    N_("Same as --whole-archive."), 0 },
  { "-z defaultextract", '\0', NULL, OPTION_DOC, N_("\
Default rules of extracting from archive; weak references are not enough."),
    0 },
  { "-z weakextract", '\0', NULL, OPTION_DOC,
    N_("Weak references cause extraction from archive."), 0 },
  { "-z muldefs", '\0', NULL, OPTION_DOC,
    N_("Allow multiple definitions; first is used."), 0 },
  { "-z defs | nodefs", '\0', NULL, OPTION_DOC,
    N_("Disallow/allow undefined symbols in DSOs."), 0 },
    { "no-undefined", ARGP_no_undefined, NULL, OPTION_HIDDEN, NULL, 0 },
  { "-z origin", '\0', NULL, OPTION_DOC,
    N_("Object requires immediate handling of $ORIGIN."), 0 },
  { "-z now", '\0', NULL, OPTION_DOC,
    N_("Relocation will not be processed lazily."), 0 },
  { "-z nodelete", '\0', NULL, OPTION_DOC,
    N_("Object cannot be unloaded at runtime."), 0 },
  { "-z initfirst", '\0', NULL, OPTION_DOC,
    N_("Mark object to be initialized first."), 0 },
  { "-z lazyload | nolazyload", '\0', NULL, OPTION_DOC,
    N_("Enable/disable lazy-loading flag for following dependencies."), 0 },
  { "-z nodlopen", '\0', NULL, OPTION_DOC,
    N_("Mark object as not loadable with 'dlopen'."), 0 },
  { "-z ignore | record", '\0', NULL, OPTION_DOC,
    N_("Ignore/record dependencies on unused DSOs."), 0 },
  { "-z systemlibrary", '\0', NULL, OPTION_DOC,
    N_("Generated DSO will be a system library."), 0 },
  { "entry", 'e', N_("ADDRESS"), 0, N_("Set entry point address."), 0 },
  { "static", ARGP_static, NULL, OPTION_HIDDEN, NULL, 0 },
  { "-B static", ARGP_static, NULL, OPTION_DOC,
    N_("Do not link against shared libraries."), 0 },
  { "dynamic", ARGP_dynamic, NULL, OPTION_HIDDEN, NULL, 0 },
  { "-B dynamic", ARGP_dynamic, NULL, OPTION_DOC,
    N_("Prefer linking against shared libraries."), 0 },
  { "export-dynamic", 'E', NULL, 0, N_("Export all dynamic symbols."), 0 },
  { "strip-all", 's', NULL, 0, N_("Strip all symbols."), 0 },
  { "strip-debug", 'S', NULL, 0, N_("Strip debugging symbols."), 0 },
  { "pagesize", ARGP_pagesize, "SIZE", 0,
    N_("Assume pagesize for the target system to be SIZE."), 0 },
  { "rpath", 'R', "PATH", OPTION_HIDDEN, NULL, 0 },
  { "runpath", ARGP_runpath, "PATH", 0, N_("Set runtime DSO search path."),
    0 },
  { "runpath-link", ARGP_runpath_link, "PATH", 0,
    N_("Set link time DSO search path."), 0 },
  { "shared", 'G', NULL, 0, N_("Generate dynamic shared object."), 0 },
  { NULL, 'r', NULL, 0L, N_("Generate relocatable object."), 0 },
  { NULL, 'B', "KEYWORD", OPTION_HIDDEN, "", 0 },
  { "-B local", 'B', NULL, OPTION_DOC,
    N_("Causes symbol not assigned to a version be reduced to local."), 0 },
  { "gc-sections", ARGP_gc_sections, NULL, 0, N_("Remove unused sections."),
    0 },
  { "no-gc-sections", ARGP_no_gc_sections, NULL, 0,
    N_("Don't remove unused sections."), 0 },
  { "soname", 'h', "NAME", 0, N_("Set soname of shared object."), 0 },
  { "dynamic-linker", 'I', "NAME", 0, N_("Set the dynamic linker name."), 0 },
  { NULL, 'Q', "YN", OPTION_HIDDEN, NULL, 0 },
  { "-Q y | n", 'Q', NULL, OPTION_DOC,
    N_("Add/suppress addition indentifying link-editor to .comment section."),
    0 },
  { "eh-frame-hdr", ARGP_eh_frame_hdr, NULL, 0,
    N_("Create .eh_frame_hdr section"), 0 },
  { "hash-style", ARGP_hash_style, "STYLE", 0,
    N_("Set hash style to sysv, gnu or both."), 0 },
  { "build-id", ARGP_build_id, "STYLE", OPTION_ARG_OPTIONAL,
    N_("Generate build ID note (md5, sha1 (default), uuid)."), 0 },

  { NULL, 0, NULL, 0, N_("Linker Operation Control:"), 0 },
  { "verbose", 'v', NULL, 0, N_("Verbose messages."), 0 },
  { "trace", 't', NULL, 0, N_("Trace file opens."), 0 },
  { "conserve-memory", ARGP_conserve, NULL, 0,
    N_("Trade speed for less memory usage"), 0 },
  { NULL, 'O', N_("LEVEL"), OPTION_ARG_OPTIONAL,
    N_("Set optimization level to LEVEL."), 0 },
  { NULL, 'c', N_("FILE"), 0, N_("Use linker script in FILE."), 0 },
#if YYDEBUG
  { "yydebug", ARGP_yydebug, NULL, 0,
    N_("Select to get parser debug information"), 0 },
#endif
  { "version-script", ARGP_version_script, "FILE", 0,
    N_("Read version information from FILE."), 0 },
  { "emulation", 'm', "NAME", 0, N_("Set emulation to NAME."), 0 },

  { NULL, 0, NULL, 0, NULL, 0 }
};

static const char doc[] = N_("Combine object and archive files.");

static const char args_doc[] = N_("[FILE]...");

static void replace_args (int argc, char *argv[]);
static error_t parse_opt_1st (int key, char *arg, struct argp_state *state);
static error_t parse_opt_2nd (int key, char *arg, struct argp_state *state);

static struct argp argp_1st =
{
  options, parse_opt_1st, args_doc, doc, NULL, NULL, NULL
};
static struct argp argp_2nd =
{
  options, parse_opt_2nd, args_doc, doc, NULL, NULL, NULL
};


struct ld_state ld_state;

static struct file_list
{
  const char *name;
  struct file_list *next;
} *input_file_list;

int verbose;

int conserve_memory;

static const char *emulation;

static int group_level;

static struct usedfiles *last_file;

static const char *linker_script = SRCDIR "/elf32-i386.script";

static int error_loading;


static char *ld_library_path1;

int ld_scan_version_script;

const char *ldin_fname;

extern int lddebug;


static void parse_z_option (const char *arg);
static void parse_z_option_2 (const char *arg);
static void parse_B_option (const char *arg);
static void parse_B_option_2 (const char *arg);
static void determine_output_format (void);
static void load_needed (void);
static void collect_sections (void);
static void add_rxxpath (struct pathelement **pathp, const char *str);
static void gen_rxxpath_data (void);
static void read_version_script (const char *fname);
static void create_lscript_symbols (void);
static void create_special_section_symbol (struct symbol **symp,
					   const char *name);


int
main (int argc, char *argv[])
{
  int remaining;
  int err;

#ifndef NDEBUG
  
  mtrace ();
#endif

  
  if (sizeof (off_t) != sizeof (off64_t))
    abort ();

  
  __fsetlocking (stdin, FSETLOCKING_BYCALLER);
  __fsetlocking (stdout, FSETLOCKING_BYCALLER);
  __fsetlocking (stderr, FSETLOCKING_BYCALLER);

  
  setlocale (LC_ALL, "");

  
  bindtextdomain (PACKAGE_TARNAME, LOCALEDIR);

  
  textdomain (PACKAGE_TARNAME);

  
  elf_version (EV_CURRENT);

  ld_library_path1 = getenv ("LD_LIBRARY_PATH");

  
#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
  obstack_init (&ld_state.smem);

  
  replace_args (argc, argv);

  argp_parse (&argp_1st, argc, argv, ARGP_IN_ORDER, &remaining, NULL);

  
  if (input_file_list == NULL)
    {
      error (0, 0, gettext ("At least one input file needed"));
      argp_help (&argp_1st, stderr, ARGP_HELP_SEE, "ld");
      exit (EXIT_FAILURE);
    }

  
  determine_output_format ();

  if (unlikely (ld_state.hash_style == hash_style_none))
    ld_state.hash_style = hash_style_sysv;

  
  err = ld_prepare_state (emulation);
  if (err != 0)
    error (EXIT_FAILURE, 0, gettext ("error while preparing linking"));

  ldin = fopen (linker_script, "r");
  if (ldin == NULL)
    error (EXIT_FAILURE, errno, gettext ("cannot open linker script '%s'"),
	   linker_script);
  
  __fsetlocking (ldin, FSETLOCKING_BYCALLER);

  ld_state.srcfiles = NULL;
  ldlineno = 1;
  ld_scan_version_script = 0;
  ldin_fname = linker_script;
  if (ldparse () != 0)
    
    exit (EXIT_FAILURE);
  fclose (ldin);

  ld_state.default_paths = ld_state.paths;
  ld_state.paths = ld_state.tailpaths = NULL;

  
  gen_rxxpath_data ();

  
  argp_parse (&argp_2nd, argc, argv, ARGP_IN_ORDER, &remaining, NULL);
  
  assert (remaining == argc);

  
  while (last_file != NULL)
    
    error_loading |= FILE_PROCESS (-1, last_file, &ld_state, &last_file);

  
  if (error_loading)
    exit (error_loading);

  
  if (group_level > 0)
    {
      error (0, 0, gettext ("-( without matching -)"));
      argp_help (&argp_1st, stderr, ARGP_HELP_SEE, "ld");
      exit (EXIT_FAILURE);
    }

  if (ld_state.file_type != relocatable_file_type)
    {
      load_needed ();

      create_lscript_symbols ();

      if (FLAG_UNRESOLVED (&ld_state) != 0)
	exit (1);
    }

  if (ld_state.file_type != relocatable_file_type)
    {
      void *p ;
      struct scnhead *h;

      p = NULL;
      while ((h = ld_section_tab_iterate (&ld_state.section_tab, &p)) != NULL)
	if (h->type == SHT_REL || h->type == SHT_RELA)
	  {
	    struct scninfo *runp = h->last;
	    do
	      {
		COUNT_RELOCATIONS (&ld_state, runp);

		ld_state.relsize_total += runp->relsize;
	      }
	    while ((runp = runp->next) != h->last);
	  }
    }

  if (ld_state.need_got)
    create_special_section_symbol (&ld_state.got_symbol,
				   "_GLOBAL_OFFSET_TABLE_");
  if (dynamically_linked_p ())
    create_special_section_symbol (&ld_state.dyn_symbol, "_DYNAMIC");

  if (OPEN_OUTFILE (&ld_state, EM_NONE, ELFCLASSNONE, ELFDATANONE) != 0)
    exit (1);

  GENERATE_SECTIONS (&ld_state);

  collect_sections ();

  CREATE_SECTIONS (&ld_state);

  if (CREATE_OUTFILE (&ld_state) != 0)
    exit (1);

  
  err |= FINALIZE (&ld_state);

  return err | (error_message_count != 0);
}


static void
replace_args (int argc, char *argv[])
{
  static const struct
  {
    const char *from;
    const char *to;
  } args[] =
      {
	{ "-export-dynamic", "--export-dynamic" },
	{ "-dynamic-linker", "--dynamic-linker" },
	{ "-static", "--static" },
      };
  const size_t nargs = sizeof (args) / sizeof (args[0]);

  for (int i = 1; i < argc; ++i)
    if (argv[i][0] == '-' && islower (argv[i][1]) && argv[i][2] != '\0')
      for (size_t j = 0; j < nargs; ++j)
	if (strcmp (argv[i], args[j].from) == 0)
	  {
	    argv[i] = (char *) args[j].to;
	    break;
	  }
}


static int
valid_hexarg (const char *arg)
{
  if (strncasecmp (arg, "0x", 2) != 0)
    return 0;

  arg += 2;
  do
    {
      if (isxdigit (arg[0]) && isxdigit (arg[1]))
	{
	  arg += 2;
	  if (arg[0] == '-' || arg[0] == ':')
	    ++arg;
	}
      else
	return 0;
    }
  while (*arg != '\0');

  return 1;
}


static error_t
parse_opt_1st (int key, char *arg,
	       struct argp_state *state __attribute__ ((unused)))
{
  switch (key)
    {
    case 'B':
      parse_B_option (arg);
      break;

    case 'c':
      linker_script = arg;
      break;

    case 'E':
      ld_state.export_all_dynamic = true;
      break;

    case 'G':
      if (ld_state.file_type != no_file_type)
	error (EXIT_FAILURE, 0,
	       gettext ("only one option of -G and -r is allowed"));
      ld_state.file_type = dso_file_type;

      
      ld_state.export_all_dynamic = true;
      break;

    case 'h':
      ld_state.soname = arg;
      break;

    case 'i':
      
      ld_library_path1 = NULL;
      break;

    case 'I':
      ld_state.interp = arg;
      break;

    case 'm':
      if (emulation != NULL)
	error (EXIT_FAILURE, 0, gettext ("more than one '-m' parameter"));
      emulation = arg;
      break;

    case 'Q':
      if (arg[1] == '\0' && (arg[0] == 'y' || arg[0] == 'Y'))
	ld_state.add_ld_comment = true;
      else if (arg[1] == '\0' && (arg[0] == 'n' || arg[0] == 'N'))
	ld_state.add_ld_comment = true;
      else
	error (EXIT_FAILURE, 0, gettext ("unknown option `-%c %s'"), 'Q', arg);
      break;

    case 'r':
      if (ld_state.file_type != no_file_type)
	error (EXIT_FAILURE, 0,
	       gettext ("only one option of -G and -r is allowed"));
      ld_state.file_type = relocatable_file_type;
      break;

    case 'S':
      ld_state.strip = strip_debug;
      break;

    case 't':
      ld_state.trace_files = true;
      break;

    case 'v':
      verbose = 1;
      break;

    case 'z':
      parse_z_option (arg);
      break;

    case ARGP_pagesize:
      {
	char *endp;
	ld_state.pagesize = strtoul (arg, &endp, 0);
	if (*endp != '\0')
	  {
	    if (endp[1] == '\0' && tolower (*endp) == 'k')
	      ld_state.pagesize *= 1024;
	    else if (endp[1] == '\0' && tolower (*endp) == 'm')
	      ld_state.pagesize *= 1024 * 1024;
	    else
	      {
		error (0, 0,
		       gettext ("invalid page size value '%s': ignored"),
		       arg);
		ld_state.pagesize = 0;
	      }
	  }
      }
      break;

    case 'R':
      add_rxxpath (&ld_state.rpath, arg);
      break;

    case ARGP_rpath_link:
      add_rxxpath (&ld_state.rpath_link, arg);
      break;

    case ARGP_runpath:
      add_rxxpath (&ld_state.runpath, arg);
      break;

    case ARGP_runpath_link:
      add_rxxpath (&ld_state.runpath_link, arg);
      break;

    case ARGP_gc_sections:
    case ARGP_no_gc_sections:
      ld_state.gc_sections = key == ARGP_gc_sections;
      break;

    case ARGP_eh_frame_hdr:
      ld_state.eh_frame_hdr = true;
      break;

    case ARGP_hash_style:
      if (strcmp (arg, "gnu") == 0)
	ld_state.hash_style = hash_style_gnu;
      else if (strcmp (arg, "both") == 0)
	ld_state.hash_style = hash_style_gnu | hash_style_sysv;
      else if (strcmp (arg, "sysv") == 0)
	ld_state.hash_style = hash_style_sysv;
      else
	error (EXIT_FAILURE, 0, gettext ("invalid hash style '%s'"), arg);
      break;

    case ARGP_build_id:
      if (arg == NULL)
	ld_state.build_id = "sha1";
      else if (strcmp (arg, "uuid") != 0
	       && strcmp (arg, "md5") != 0
	       && strcmp (arg, "sha1") != 0
	       && !valid_hexarg (arg))
	error (EXIT_FAILURE, 0, gettext ("invalid build-ID style '%s'"), arg);
      else
	ld_state.build_id = arg;
      break;

    case 's':
      if (arg == NULL)
	{
	  if (ld_state.strip == strip_all)
	    ld_state.strip = strip_everything;
	  else
	    ld_state.strip = strip_all;
	  break;
	}
      

    case 'e':
    case 'o':
    case 'O':
    case ARGP_whole_archive:
    case ARGP_no_whole_archive:
    case ARGP_as_needed:
    case ARGP_no_as_needed:
    case 'L':
    case '(':
    case ')':
    case 'l':
    case ARGP_static:
    case ARGP_dynamic:
    case ARGP_version_script:
      
      break;

    case ARGP_KEY_ARG:
      {
	struct file_list *newp;

	newp = (struct file_list *) xmalloc (sizeof (struct file_list));
	newp->name = arg;
#ifndef NDEBUG
	newp->next = NULL;
#endif
	CSNGL_LIST_ADD_REAR (input_file_list, newp);
      }
      break;

#if YYDEBUG
    case ARGP_yydebug:
      lddebug = 1;
      break;
#endif

    case ARGP_no_undefined:
      ld_state.nodefs = false;
      break;

    case ARGP_conserve:
      conserve_memory = 1;
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}


static error_t
parse_opt_2nd (int key, char *arg,
	       struct argp_state *state __attribute__ ((unused)))
{
  static bool group_start_requested;
  static bool group_end_requested;

  switch (key)
    {
    case 'B':
      parse_B_option_2 (arg);
      break;

    case 'e':
      ld_state.entry = arg;
      break;

    case 'o':
      if (ld_state.outfname != NULL)
	{
	  error (0, 0, gettext ("More than one output file name given."));
	see_help:
	  argp_help (&argp_2nd, stderr, ARGP_HELP_SEE, "ld");
	  exit (EXIT_FAILURE);
	}
      ld_state.outfname = arg;
      break;

    case 'O':
      if (arg == NULL)
	ld_state.optlevel = 1;
      else
	{
	  char *endp;
	  unsigned long int level = strtoul (arg, &endp, 10);
	  if (*endp != '\0')
	    {
	      error (0, 0, gettext ("Invalid optimization level `%s'"), arg);
	      goto see_help;
	    }
	  ld_state.optlevel = level;
	}
      break;

    case ARGP_whole_archive:
      ld_state.extract_rule = allextract;
      break;
    case ARGP_no_whole_archive:
      ld_state.extract_rule = defaultextract;
      break;

    case ARGP_as_needed:
      ld_state.as_needed = true;
      break;
    case ARGP_no_as_needed:
      ld_state.as_needed = false;
      break;

    case ARGP_static:
    case ARGP_dynamic:
      
      ld_state.statically = key == ARGP_static;
      break;

    case 'z':
      parse_z_option_2 (arg);
      break;

    case ARGP_version_script:
      read_version_script (arg);
      break;

    case 'L':
      
      ld_new_searchdir (arg);
      break;

    case '(':
      if (verbose && (group_start_requested || !group_end_requested))
	error (0, 0, gettext ("nested -( -) groups are not allowed"));

      
      ++group_level;

      
      group_start_requested = true;
      group_end_requested = false;
      break;

    case ')':
      if (!group_end_requested)
	{
	  if (group_level == 0)
	    {
	      error (0, 0, gettext ("-) without matching -("));
	      goto see_help;
	    }
	}
      else
	last_file->group_end = true;

      if (group_level > 0)
	--group_level;
      break;

    case 'l':
    case ARGP_KEY_ARG:
      {
	while (last_file != NULL)
	  
	  error_loading |= FILE_PROCESS (-1, last_file, &ld_state, &last_file);

	last_file = ld_new_inputfile (arg,
				      key == 'l'
				      ? archive_file_type
				      : relocatable_file_type);
	if (group_start_requested)
	  {
	    last_file->group_start = true;

	    group_start_requested = false;
	    group_end_requested = true;
	  }
      }
      break;

    default:
      break;
    }
  return 0;
}


static void
load_needed (void)
{
  struct usedfiles *first;
  struct usedfiles *runp;

  if (ld_state.needed == NULL)
    return;

  runp = first = ld_state.needed->next;
  do
    {
      struct usedfiles *ignore;
      struct usedfiles *next = runp->next;
      int err;

      err = FILE_PROCESS (-1, runp, &ld_state, &ignore);
      if (err != 0)
	
	exit (err);

      runp = next;
    }
  while (runp != first);
}


static void
print_version (FILE *stream, struct argp_state *state __attribute__ ((unused)))
{
  fprintf (stream, "ld (%s) %s\n", PACKAGE_NAME, PACKAGE_VERSION);
  fprintf (stream, gettext ("\
Copyright (C) %s Red Hat, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"), "2012");
  fprintf (stream, gettext ("Written by %s.\n"), "Ulrich Drepper");
}


static void
parse_z_option (const char *arg)
{
  if (strcmp (arg, "nodefaultlib") == 0
      
      && ld_state.file_type == dso_file_type)
    ld_state.dt_flags_1 |= DF_1_NODEFLIB;
  else if (strcmp (arg, "muldefs") == 0)
    ld_state.muldefs = true;
  else if (strcmp (arg, "nodefs") == 0)
    ld_state.nodefs = true;
  else if (strcmp (arg, "defs") == 0)
    ld_state.nodefs = false;
  else if (strcmp (arg, "now") == 0)
    ld_state.dt_flags |= DF_BIND_NOW;
  else if (strcmp (arg, "origin") == 0)
    ld_state.dt_flags |= DF_ORIGIN;
  else if (strcmp (arg, "nodelete") == 0
	   
	   && ld_state.file_type == dso_file_type)
    ld_state.dt_flags_1 |= DF_1_NODELETE;
  else if (strcmp (arg, "initfirst") == 0)
    ld_state.dt_flags_1 |= DF_1_INITFIRST;
  else if (strcmp (arg, "nodlopen") == 0
	   
	   && ld_state.file_type == dso_file_type)
    ld_state.dt_flags_1 |= DF_1_NOOPEN;
  else if (strcmp (arg, "systemlibrary") == 0)
    ld_state.is_system_library = true;
  else if (strcmp (arg, "execstack") == 0)
    ld_state.execstack = execstack_true;
  else if (strcmp (arg, "noexecstack") == 0)
    ld_state.execstack = execstack_false_force;
  else if (strcmp (arg, "allextract") != 0
	   && strcmp (arg, "defaultextract") != 0
	   && strcmp (arg, "weakextract") != 0
	   && strcmp (arg, "lazyload") != 0
	   && strcmp (arg, "nolazyload") != 0
	   && strcmp (arg, "ignore") != 0
	   && strcmp (arg, "record") != 0)
    error (0, 0, gettext ("unknown option `-%c %s'"), 'z', arg);
}


static void
parse_z_option_2 (const char *arg)
{
  if (strcmp (arg, "allextract") == 0)
    ld_state.extract_rule = allextract;
  else if (strcmp (arg, "defaultextract") == 0)
    ld_state.extract_rule = defaultextract;
  else if (strcmp (arg, "weakextract") == 0)
    ld_state.extract_rule = weakextract;
  else if (strcmp (arg, "lazyload") == 0)
    ld_state.lazyload = true;
  else if (strcmp (arg, "nolazyload") == 0)
    ld_state.lazyload = false;
  else if (strcmp (arg, "ignore") == 0)
    ld_state.as_needed = true;
  else if (strcmp (arg, "record") == 0)
    ld_state.as_needed = false;
}


static void
parse_B_option (const char *arg)
{
  if (strcmp (arg, "local") == 0)
    ld_state.default_bind_local = true;
  else if (strcmp (arg, "symbolic") != 0
	   && strcmp (arg, "static") != 0
	   && strcmp (arg, "dynamic") != 0)
    error (0, 0, gettext ("unknown option '-%c %s'"), 'B', arg);
}


static void
parse_B_option_2 (const char *arg)
{
  if (strcmp (arg, "static") == 0)
    ld_state.statically = true;
  else if (strcmp (arg, "dynamic") == 0)
    ld_state.statically = false;
  else if (strcmp (arg, "symbolic") == 0
	   
	   && ld_state.file_type == dso_file_type)
    ld_state.dt_flags |= DF_SYMBOLIC;
}


static void
determine_output_format (void)
{
  struct file_list *last = input_file_list;
  input_file_list = input_file_list->next;
  last->next = NULL;

  if (emulation != NULL)
    {
      ld_state.ebl = ebl_openbackend_emulation (emulation);

      assert (ld_state.ebl != NULL);
    }
  else
    {
      
      struct file_list *runp = input_file_list;

      while (runp != NULL)
	{
	  int fd = open (runp->name, O_RDONLY);
	  if (fd != -1)
	    {
	      int try (Elf *elf)
		{
		  int result = 0;

		  if (elf == NULL)
		    return 0;

		  if (elf_kind (elf) == ELF_K_ELF)
		    {
		      XElf_Ehdr_vardef(ehdr);

		      
		      xelf_getehdr (elf, ehdr);
		      if (ehdr != NULL)
			ld_state.ebl =
			  ebl_openbackend_machine (ehdr->e_machine);

		      result = 1;
		    }
		  else if (elf_kind (elf) == ELF_K_AR)
		    {
		      Elf *subelf;
		      Elf_Cmd cmd = ELF_C_READ_MMAP;

		      while ((subelf = elf_begin (fd, cmd, elf)) != NULL)
			{
			  cmd = elf_next (subelf);

			  if (try (subelf) != 0)
			    break;
			}
		    }

		  elf_end (elf);

		  return result;
		}

	      if (try (elf_begin (fd, ELF_C_READ_MMAP, NULL)) != 0)
		
		break;
	    }

	  runp = runp->next;
	}

      if (ld_state.ebl == NULL)
	{
	  error (0, 0, gettext ("\
could not find input file to determine output file format"));
	  error (EXIT_FAILURE, 0, gettext ("\
try again with an appropriate '-m' parameter"));
	}
    }

  while (input_file_list != NULL)
    {
      struct file_list *oldp = input_file_list;
      input_file_list = input_file_list->next;
      free (oldp);
    }

  if (ld_state.file_type == no_file_type)
    ld_state.file_type = executable_file_type;
}

void
ld_new_searchdir (const char *dir)
{
  struct pathelement *newpath;

  newpath = (struct pathelement *)
    obstack_calloc (&ld_state.smem, sizeof (struct pathelement));

  newpath->pname = dir;

  
  if (ld_state.tailpaths == NULL)
    ld_state.paths = ld_state.tailpaths = newpath->next = newpath;
  else
    {
      ld_state.tailpaths->next = newpath;
      ld_state.tailpaths = newpath;
      newpath->next = ld_state.paths;
    }
}


struct usedfiles *
ld_new_inputfile (const char *fname, enum file_type type)
{
  struct usedfiles *newfile = (struct usedfiles *)
    obstack_calloc (&ld_state.smem, sizeof (struct usedfiles));

  newfile->soname = newfile->fname = newfile->rfname = fname;
  newfile->file_type = type;
  newfile->extract_rule = ld_state.extract_rule;
  newfile->as_needed = ld_state.as_needed;
  newfile->lazyload = ld_state.lazyload;
  newfile->status = not_opened;

  return newfile;
}


static void
collect_sections (void)
{
  void *p ;
  struct scnhead *h;
  size_t cnt;

  
  ld_state.nallsections = ld_state.section_tab.filled;

  ld_state.allsections =
    (struct scnhead **) obstack_alloc (&ld_state.smem,
				       (ld_state.nallsections + 1)
				       * sizeof (struct scnhead *));

  cnt = 0;
  p = NULL;
  while ((h = ld_section_tab_iterate (&ld_state.section_tab, &p)) != NULL)
    {
      struct scninfo *runp;
      bool used = false;

      if (h->kind == scn_normal)
	{
	  runp = h->last;
	  do
	    {
	      if (h->type == SHT_REL || h->type == SHT_RELA)
		{
		  if (runp->used)
		    runp->used
		      = runp->fileinfo->scninfo[SCNINFO_SHDR (runp->shdr).sh_info].used;
		}

	      
	      used |= runp->used;

	      
	      runp = runp->next;
	    }
	  while (runp != h->last);

	  h->used = used;
	}

      ld_state.allsections[cnt++] = h;
    }
  ld_state.nusedsections = cnt;

  assert (cnt == ld_state.nallsections);
}


static void
add_rxxpath (struct pathelement **pathp, const char *str)
{
  struct pathelement *newp;

  newp = (struct pathelement *) obstack_alloc (&ld_state.smem, sizeof (*newp));
  newp->pname = str;
  newp->exist = 0;
#ifndef NDEBUG
  newp->next = NULL;
#endif

  CSNGL_LIST_ADD_REAR (*pathp, newp);
}


static void
normalize_dirlist (struct pathelement **pathp)
{
  struct pathelement *firstp = *pathp;

  do
    {
      const char *pname = (*pathp)->pname;
      const char *colonp = strchrnul (pname, ':');

      if (colonp != NULL)
	{
	  struct pathelement *lastp = *pathp;
	  struct pathelement *newp;

	  while (1)
	    {
	      if (colonp == pname)
		lastp->pname = ".";
	      else
		lastp->pname = obstack_strndup (&ld_state.smem, pname,
						colonp - pname);

	      if (*colonp == '\0')
		break;
	      pname = colonp + 1;

	      newp = (struct pathelement *) obstack_alloc (&ld_state.smem,
							   sizeof (*newp));
	      newp->next = lastp->next;
	      newp->exist = 0;
	      lastp = lastp->next = newp;

	      colonp = strchrnul (pname, ':');
	    }

	  pathp = &lastp->next;
	}
      else
	pathp = &(*pathp)->next;
    }
  while (*pathp != firstp);
}


static void
gen_rxxpath_data (void)
{
  char *ld_library_path2;

  if (ld_state.rpath != NULL)
    {
      struct pathelement *endp = ld_state.rpath;
      ld_state.rpath = ld_state.rpath->next;
      endp->next = NULL;
    }
  if (ld_state.rpath_link != NULL)
    {
      struct pathelement *endp = ld_state.rpath_link;
      ld_state.rpath_link = ld_state.rpath_link->next;
      endp->next = NULL;
    }

  if (ld_state.runpath != NULL)
    {
      struct pathelement *endp = ld_state.runpath;
      ld_state.runpath = ld_state.runpath->next;
      endp->next = NULL;

      while (ld_state.rpath != NULL)
	{
	  struct pathelement *old = ld_state.rpath;
	  ld_state.rpath = ld_state.rpath->next;
	  free (old);
	}
    }
  if (ld_state.runpath_link != NULL)
    {
      struct pathelement *endp = ld_state.runpath_link;
      ld_state.runpath_link = ld_state.runpath_link->next;
      endp->next = NULL;

      while (ld_state.rpath_link != NULL)
	{
	  struct pathelement *old = ld_state.rpath_link;
	  ld_state.rpath_link = ld_state.rpath_link->next;
	  free (old);
	}

      normalize_dirlist (&ld_state.runpath_link);
    }
  else if (ld_state.rpath_link != NULL)
    
    normalize_dirlist (&ld_state.rpath_link);


  if (ld_library_path1 != NULL)
    {
      ld_library_path2 = strchr (ld_library_path1, ';');
      if (ld_library_path2 == NULL)
	{
	  ld_library_path2 = ld_library_path1;
	  ld_library_path1 = NULL;
	}
      else
	{
	  
	  *ld_library_path2++ = '\0';

	  
	  add_rxxpath (&ld_state.ld_library_path1, ld_library_path1);
	  normalize_dirlist (&ld_state.ld_library_path1);
	}

      add_rxxpath (&ld_state.ld_library_path2, ld_library_path2);
      normalize_dirlist (&ld_state.ld_library_path2);
    }
}


static void
read_version_script (const char *fname)
{
  ldin = fopen (fname, "r");
  if (ldin == NULL)
    error (EXIT_FAILURE, errno, gettext ("cannot read version script '%s'"),
	   fname);
  
  __fsetlocking (ldin, FSETLOCKING_BYCALLER);

  
  ld_scan_version_script = 1;

  ldlineno = 1;
  ldin_fname = fname;
  if (ldparse () != 0)
    
    exit (EXIT_FAILURE);

  fclose (ldin);
}


static void
create_lscript_symbols (void)
{
  if (ld_state.output_segments == NULL)
    return;

  struct output_segment *segment = ld_state.output_segments->next;
  do
    {
      struct output_rule *orule;

      for (orule = segment->output_rules; orule != NULL; orule = orule->next)
	if (orule->tag == output_assignment
	    && strcmp (orule->val.assignment->variable, ".") != 0)
	  {
	    struct symbol *s = ld_state.unresolved;

	    
	    if (likely (s != NULL))
	      {
		struct symbol *first = s;
		const char *providename = orule->val.assignment->variable;

		
		do
		  if (strcmp (s->name, providename) == 0)
		    {
		      
		      if (unlikely (!s->defined))
			{
			  CDBL_LIST_DEL (ld_state.unresolved, s);
			  --ld_state.nunresolved;
			  goto use_it;
			}

		      if (unlikely (!orule->val.assignment->provide_flag))
			{
			  error (0, 0, gettext ("\
duplicate definition of '%s' in linker script"),
				 providename);
			  goto next_rule;
			}
		    }
		while ((s = s->next) != first);
	      }

	    if (orule->val.assignment->provide_flag)
	      continue;

	    
	    s = (struct symbol *)
	      obstack_calloc (&ld_state.smem, sizeof (struct symbol));

	    
	    s->name = orule->val.assignment->variable;

	    
	    unsigned long int hval = elf_hash (s->name);
	    if (unlikely (ld_symbol_tab_insert (&ld_state.symbol_tab,
						hval, s) != 0))
	      {
		free (s);
		struct symbol *old = ld_symbol_tab_find (&ld_state.symbol_tab,
							 hval, s);
		assert (old != NULL);
		free (s);

		if (! old->in_dso)
		  {
		    error (0, 0, gettext ("\
duplicate definition of '%s' in linker script"),
			   s->name);
		    goto next_rule;
		  }

		
		s = old;
	      }

	  use_it:
	    
	    s->defined = 1;
	    s->type = STT_NOTYPE;

	    orule->val.assignment->sym = s;

	    SNGL_LIST_PUSH (ld_state.lscript_syms, s);
	    ++ld_state.nlscript_syms;

	  next_rule:
	    ;
	  }

      segment = segment->next;
    }
  while (segment != ld_state.output_segments->next);
}


static void
create_special_section_symbol (struct symbol **symp, const char *name)
{
  if (*symp == NULL)
    {
      
      struct symbol *newsym = (struct symbol *)
	obstack_calloc (&ld_state.smem, sizeof (*newsym));

      newsym->name = name;
      
      

      
      if (unlikely (ld_symbol_tab_insert (&ld_state.symbol_tab,
					  elf_hash (name), newsym) != 0))
	abort ();

      *symp = newsym;
    }
  else if ((*symp)->defined)
    
    abort ();

  (*symp)->defined = 1;
  (*symp)->local = 1;
  (*symp)->hidden = 1;
  (*symp)->type = STT_OBJECT;

  ++ld_state.nsymtab;
}


#include "debugpred.h"

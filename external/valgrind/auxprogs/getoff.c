/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2014-2014 Philippe Waroquiers

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

   
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <config.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_DLINFO_RTLD_DI_TLS_MODID
#include <link.h>
#include <dlfcn.h>
#endif

static
int is_opt(char* arg, const char *option)
{
   int option_len = strlen(option);
   if (option[option_len-1] == '=')
      return (0 == strncmp(option, arg, option_len));
   else
      return (0 == strcmp(option, arg));
}

static int verbose = 0;

static
void usage (char* progname)
{
   fprintf(stderr,
"Usage: %s [--help] [-h] [-v] [-o <outputfile>]\n"
"Outputs various user space offsets\n"
"By default, outputs on stdout.\n"
"Use -o to output to <outputfile>\n"
"-v : be more verbose\n",
progname);

}

int main (int argc, char** argv)
{
   int i;
   FILE *outputfile;
   int nr_errors = 0;
   
   outputfile = stdout;

   i = 1;
   while (i < argc) {
      if (is_opt(argv[i], "--help") || is_opt(argv[i], "-h")) {
         usage(argv[0]);
         exit(0);
      } else if (is_opt(argv[i], "-v")) {
         verbose++;
      } else if (is_opt(argv[i], "-o")) {
         if (i+1 == argc) {
            fprintf(stderr, 
                    "missing output file for -o option\n"
                    "Use --help for more information.\n");
            exit (1);
         }
         i++;
         outputfile = fopen(argv[i], "w");
         if (outputfile == NULL) {
            fprintf(stderr, "Could not fopen %s in write mode\n", argv[i]);
            perror ("fopen output file failed");
            exit (1);
         }
      } else {
         fprintf (stderr, 
                  "unknown or invalid argument %s\n"
                  "Use --help for more information.\n",
                  argv[i]);
         exit(1);
      }
      i++;
   }

#ifdef HAVE_DLINFO_RTLD_DI_TLS_MODID
   {
      #define MAX_LINKMAP_WORDS 10000
      size_t dummy_link_map[MAX_LINKMAP_WORDS];
      size_t off;
      size_t modid_offset;
      for (off = 0; off < MAX_LINKMAP_WORDS; off++)
         dummy_link_map[off] = off;
      if (dlinfo ((void*)dummy_link_map, RTLD_DI_TLS_MODID, 
                  &modid_offset) == 0) {
         assert(modid_offset >= 0 && modid_offset < MAX_LINKMAP_WORDS);
         fprintf(outputfile,
                 "lm_modid_offset 0x%zx\n", modid_offset*sizeof(size_t));
      } else {
         fprintf(stderr, 
                 "Error computing lm_modid_offset.\n"
                 "dlinfo error %s\n", dlerror());
         nr_errors++;
      }
      #undef MAX_LINKMAP_WORDS
   }
   
   if (outputfile != stdout)
      if (fclose (outputfile) != 0) {
         perror ("fclose output file failed\n");
         nr_errors++;
      }
#else
   if (verbose)
      fprintf(stderr, 
              "cannot compute lm_modid_offset.\n"
              "configure did not define HAVE_DLINFO_RTLD_DI_TLS_MODID.\n");
#endif

   if (nr_errors == 0)
      exit(0);
   else
      exit(1);
}



/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2011-2013 Philippe Waroquiers

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

#ifndef __VGDB_H
#define __VGDB_H

#include "pub_core_basics.h"
#include "pub_core_vki.h"
#include "pub_core_gdbserver.h"

#include <sys/types.h>

extern int debuglevel;
extern struct timeval dbgtv;
#define DEBUG(level, ...) (level <= debuglevel ?                        \
                           gettimeofday(&dbgtv, NULL),                  \
                           fprintf(stderr, "%ld.%6.6ld ",               \
                                   (long int)dbgtv.tv_sec,              \
                                   (long int)dbgtv.tv_usec),            \
                           fprintf(stderr, __VA_ARGS__),fflush(stderr)  \
                           : 0)

#define PDEBUG(level, ...) (level <= debuglevel ?                       \
                            fprintf(stderr, __VA_ARGS__),fflush(stderr) \
                            : 0)

#define ERROR(errno, ...) ((errno == 0 ? 0 : perror("syscall failed")), \
                           fprintf(stderr, __VA_ARGS__),                \
                           fflush(stderr))
#define XERROR(errno, ...) ((errno == 0 ? 0 : perror("syscall failed")), \
                            fprintf(stderr, __VA_ARGS__),                \
                            fflush(stderr),                              \
                            exit(1))

extern void *vmalloc(size_t size);
extern void *vrealloc(void *ptr,size_t size);

extern Bool shutting_down;

extern VgdbShared32 *shared32;
extern VgdbShared64 *shared64;


void invoker_restrictions_msg(void);

void invoker_cleanup_restore_and_detach(void *v_pid);

Bool invoker_invoke_gdbserver(pid_t pid);

void invoker_valgrind_dying(void);

#endif 


/* Target operations for the Valgrind remote server for GDB.
   Copyright (C) 2002, 2003, 2004, 2005, 2012
   Free Software Foundation, Inc.
   Philippe Waroquiers.

   Contributed by MontaVista Software.

   This file is part of GDB.
   It has been modified to integrate it in valgrind

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#ifndef TARGET_H
#define TARGET_H

#include "pub_core_basics.h"    
#include "server.h"             

        

extern void valgrind_initialize_target(void);

extern void initialize_shadow_low (Bool shadow_mode);

extern const char* valgrind_target_xml (Bool shadow_mode);



struct thread_resume
{
  
  int step;

  
  int sig;
};

extern void valgrind_resume (struct thread_resume *resume_info);

extern unsigned char valgrind_wait (char *outstatus);


extern Addr valgrind_get_ignore_break_once(void);

extern void VG_(set_watchpoint_stop_address) (Addr addr);

extern int valgrind_stopped_by_watchpoint (void);

extern CORE_ADDR valgrind_stopped_data_address (void);

extern Bool valgrind_single_stepping(void);

extern void valgrind_set_single_stepping(Bool);


extern int valgrind_thread_alive (unsigned long tid);

extern void set_desired_inferior (int use_general);

extern void valgrind_fetch_registers (int regno);

extern void valgrind_store_registers (int regno);



extern int valgrind_read_memory (CORE_ADDR memaddr,
                                 unsigned char *myaddr, int len);

extern int valgrind_write_memory (CORE_ADDR memaddr,
                                  const unsigned char *myaddr, int len);


extern int valgrind_insert_watchpoint (char type, CORE_ADDR addr, int len);
extern int valgrind_remove_watchpoint (char type, CORE_ADDR addr, int len);

extern Bool valgrind_get_tls_addr (ThreadState *tst,
                                   CORE_ADDR offset,
                                   CORE_ADDR lm,
                                   CORE_ADDR *tls_addr);




extern VexGuestArchState* get_arch (int set, ThreadState* tst);

extern void* VG_(dmemcpy) ( void *d, const void *s, SizeT sz, Bool *mod );

typedef
   enum {
      valgrind_to_gdbserver,
      gdbserver_to_valgrind} transfer_direction;

extern void  VG_(transfer) (void *valgrind,
                            void *gdbserver,
                            transfer_direction dir,
                            SizeT sz,
                            Bool *mod);


extern Bool hostvisibility;

#endif 

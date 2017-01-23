

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2004-2013 OpenWorks LLP
      info@open-works.net

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   The GNU General Public License is contained in the file COPYING.

   Neither the names of the U.S. Department of Energy nor the
   University of California nor the names of its contributors may be
   used to endorse or promote products derived from this software
   without prior written permission.
*/

#ifndef __LIBVEX_TRC_VALUES_H
#define __LIBVEX_TRC_VALUES_H



#define VEX_TRC_JMP_INVALICACHE 61  
#define VEX_TRC_JMP_FLUSHDCACHE 103 

#define VEX_TRC_JMP_NOREDIR    81  
#define VEX_TRC_JMP_SIGTRAP    85  
#define VEX_TRC_JMP_SIGSEGV    87  
#define VEX_TRC_JMP_SIGBUS     93  

#define VEX_TRC_JMP_SIGFPE_INTDIV     97  

#define VEX_TRC_JMP_SIGFPE_INTOVF     99  

#define VEX_TRC_JMP_SIGILL     101  

#define VEX_TRC_JMP_EMWARN     63  
#define VEX_TRC_JMP_EMFAIL     83  

#define VEX_TRC_JMP_CLIENTREQ  65  
#define VEX_TRC_JMP_YIELD      67  
#define VEX_TRC_JMP_NODECODE   69  
#define VEX_TRC_JMP_MAPFAIL    71  

#define VEX_TRC_JMP_SYS_SYSCALL  73 
#define VEX_TRC_JMP_SYS_INT32    75 
#define VEX_TRC_JMP_SYS_INT128   77 
#define VEX_TRC_JMP_SYS_INT129   89 
#define VEX_TRC_JMP_SYS_INT130   91 

#define VEX_TRC_JMP_SYS_SYSENTER 79 

#define VEX_TRC_JMP_BORING       95 

#endif 


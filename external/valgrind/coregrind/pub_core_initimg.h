

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2006-2013 OpenWorks LLP
      info@open-works.co.uk

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

#ifndef __PUB_CORE_INITIMG_H
#define __PUB_CORE_INITIMG_H

#include "pub_core_basics.h"      
#include "libvex.h"


typedef  struct _IICreateImageInfo    IICreateImageInfo;
typedef  struct _IIFinaliseImageInfo  IIFinaliseImageInfo;

extern 
IIFinaliseImageInfo VG_(ii_create_image)( IICreateImageInfo,
                                          const VexArchInfo* vex_archinfo );

extern 
void VG_(ii_finalise_image)( IIFinaliseImageInfo );



#if defined(VGO_linux)

struct _IICreateImageInfo {
   
   const HChar*  toolname;
   Addr    sp_at_startup;
   Addr    clstack_end; 
   
   HChar** argv;
   HChar** envp;
};

struct _IIFinaliseImageInfo {
   
   SizeT clstack_max_size;
   Addr  initial_client_SP;
   
   Addr  initial_client_IP;
   Addr  initial_client_TOC;
   UInt* client_auxv;
};


#elif defined(VGO_darwin)

struct _IICreateImageInfo {
   
   const HChar*  toolname;
   Addr    sp_at_startup;
   Addr    clstack_end; 
   
   HChar** argv;
   HChar** envp;
   Addr    entry;            
   Addr    init_ip;          
   Addr    stack_start;      
   Addr    stack_end;        
   Addr    text;             
   Bool    dynamic;          
   HChar*  executable_path;  
};

struct _IIFinaliseImageInfo {
   
   SizeT clstack_max_size;
   Addr  initial_client_SP;
   
   Addr  initial_client_IP;
};


#else
#  error "Unknown OS"
#endif


#endif   


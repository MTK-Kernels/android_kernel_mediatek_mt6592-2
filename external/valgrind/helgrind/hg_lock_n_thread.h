

/*
   This file is part of Helgrind, a Valgrind tool for detecting errors
   in threaded programs.

   Copyright (C) 2007-2013 OpenWorks Ltd
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

#ifndef __HG_LOCK_N_THREAD_H
#define __HG_LOCK_N_THREAD_H



#define Thread_MAGIC   0x504fc5e5
#define LockN_MAGIC    0x6545b557 
#define LockP_MAGIC    0x755b5456 


typedef  WordSet  WordSetID;


typedef  struct _SO  SO;

typedef  struct _Thr  Thr;
typedef  UInt         ThrID;


typedef
   struct _Thread {
      
      struct _Thread* admin;
      UInt            magic;
      Thr*            hbthr; 
      ThreadId        coretid;  
      
      WordSetID locksetA; 
      WordSetID locksetW; 
      
      
      ExeContext* created_at;
      Bool        announced;
      
      Int         errmsg_index;
   }
   Thread;

Thread* get_admin_threads ( void );


typedef
   enum {
      LK_mbRec=1001, 
      LK_nonRec,     
      LK_rdwr        
   }
   LockKind;

typedef
   struct _Lock {
      
      struct _Lock* admin_next; 
      struct _Lock* admin_prev; 
      ULong         unique; 
      UInt          magic;  
      
      
      ExeContext*   appeared_at;
      ExeContext*   acquired_at;
      
      SO*           hbso;      
      Addr          guestaddr; 
      LockKind      kind;      
      
      Bool          heldW; 
      WordBag*      heldBy; 
   }
   Lock;

#define Lock_INVALID  ((Lock*)1UL)


Bool HG_(is_sane_Thread)   ( Thread* thr );
Bool HG_(is_sane_LockP)    ( Lock* lock );
Bool HG_(is_sane_LockN)    ( Lock* lock );
Bool HG_(is_sane_LockNorP) ( Lock* lock );


#endif 


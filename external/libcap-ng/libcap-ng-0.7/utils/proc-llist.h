/*
* proc-llist.h
* Copyright (c) 2009 Red Hat Inc., Durham, North Carolina.
* All Rights Reserved.
*
* This software may be freely redistributed and/or modified under the
* terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2, or (at your option) any
* later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING. If not, write to the
* Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
* Authors:
*   Steve Grubb <sgrubb@redhat.com>
*/

#ifndef PROC_HEADER
#define PROC_HEADER

#include "config.h"


typedef struct _lnode{
  pid_t ppid;           
  pid_t pid;            
  uid_t uid;            
  char *cmd;		
  unsigned long inode;	
  char *capabilities;	
  char *bounds;		
  struct _lnode* next;	
} lnode;

typedef struct {
  lnode *head;		
  lnode *cur;		
  unsigned int cnt;	
} llist;

void list_create(llist *l);
static inline lnode *list_get_cur(llist *l) { return l->cur; }
void list_append(llist *l, lnode *node);
void list_clear(llist* l);

lnode *list_find_inode(llist *l, unsigned long i);

#endif


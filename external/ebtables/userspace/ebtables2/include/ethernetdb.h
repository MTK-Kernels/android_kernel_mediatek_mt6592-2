/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifndef	_ETHERNETDB_H
#define	_ETHERNETDB_H	1

#include <features.h>
#include <netinet/in.h>
#include <stdint.h>

#ifndef	_PATH_ETHERTYPES
#define	_PATH_ETHERTYPES	"/etc/ethertypes"
#endif				

#define __THROW

struct ethertypeent {
	char *e_name;		
	char **e_aliases;	
	int e_ethertype;	
};

extern void setethertypeent(int __stay_open) __THROW;

extern void endethertypeent(void) __THROW;

extern struct ethertypeent *getethertypeent(void) __THROW;

extern struct ethertypeent *getethertypebyname(__const char *__name)
    __THROW;

extern struct ethertypeent *getethertypebynumber(int __ethertype) __THROW;


#endif				

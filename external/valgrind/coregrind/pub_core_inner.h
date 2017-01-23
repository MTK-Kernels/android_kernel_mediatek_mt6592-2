

/*
   This file is part of Valgrind, a dynamic binary instrumentation
   framework.

   Copyright (C) 2012-2013 Philippe Waroquiers
      philippe.waroquiers@skynet.be

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

#ifndef __PUB_CORE_INNER_H
#define __PUB_CORE_INNER_H


#include "config.h" 

#if defined(ENABLE_INNER)

#define ENABLE_INNER_CLIENT_REQUEST 1

#if defined(ENABLE_INNER_CLIENT_REQUEST)
#define INNER_REQUEST(__zza)  __zza
#else
#define INNER_REQUEST(__zza)  do {} while (0)
#endif

#else

#define INNER_REQUEST(__zza)  do {} while (0)

#endif

#endif   


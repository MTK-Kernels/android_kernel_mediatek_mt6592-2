/* exif-mem.h
 *
 * Copyright (c) 2003 Lutz Mueller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA.
 */

#ifndef __EXIF_MEM_H__
#define __EXIF_MEM_H__

#include <libexif/exif-utils.h>

#ifdef __cplusplus
extern "C" {
#endif 

typedef void * (* ExifMemAllocFunc)   (ExifLong s);

typedef void * (* ExifMemReallocFunc) (void *p, ExifLong s);

typedef void   (* ExifMemFreeFunc)    (void *p);

typedef struct _ExifMem ExifMem;

ExifMem *exif_mem_new   (ExifMemAllocFunc a, ExifMemReallocFunc r,
			 ExifMemFreeFunc f);
void     exif_mem_ref   (ExifMem *);

void     exif_mem_unref (ExifMem *);

void *exif_mem_alloc   (ExifMem *m, ExifLong s);
void *exif_mem_realloc (ExifMem *m, void *p, ExifLong s);
void  exif_mem_free    (ExifMem *m, void *p);

ExifMem *exif_mem_new_default (void);

#ifdef __cplusplus
}
#endif 

#endif 

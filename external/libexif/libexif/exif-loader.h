/*
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

#ifndef __EXIF_LOADER_H__
#define __EXIF_LOADER_H__

#include <libexif/exif-data.h>
#include <libexif/exif-log.h>
#include <libexif/exif-mem.h>

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct _ExifLoader ExifLoader;

ExifLoader *exif_loader_new     (void);

ExifLoader *exif_loader_new_mem (ExifMem *mem);

void        exif_loader_ref     (ExifLoader *loader);

void        exif_loader_unref   (ExifLoader *loader);

void        exif_loader_write_file (ExifLoader *loader, const char *fname);

unsigned char exif_loader_write (ExifLoader *loader, unsigned char *buf, unsigned int sz);

void          exif_loader_reset (ExifLoader *loader);

ExifData     *exif_loader_get_data (ExifLoader *loader);

void exif_loader_get_buf (ExifLoader *loader, const unsigned char **buf,
						  unsigned int *buf_size);

void exif_loader_log (ExifLoader *loader, ExifLog *log);

#ifdef __cplusplus
}
#endif 

#endif 

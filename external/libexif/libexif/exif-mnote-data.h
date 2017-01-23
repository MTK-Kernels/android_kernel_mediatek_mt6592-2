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

#ifndef __EXIF_MNOTE_DATA_H__
#define __EXIF_MNOTE_DATA_H__

#ifdef __cplusplus
extern "C" {
#endif 

#include <libexif/exif-log.h>

typedef struct _ExifMnoteData ExifMnoteData;

void exif_mnote_data_ref   (ExifMnoteData *);
void exif_mnote_data_unref (ExifMnoteData *);

void exif_mnote_data_load (ExifMnoteData *d, const unsigned char *buf,
			   unsigned int buf_siz);

void exif_mnote_data_save (ExifMnoteData *d, unsigned char **buf,
			   unsigned int *buf_siz);

unsigned int exif_mnote_data_count           (ExifMnoteData *d);

unsigned int exif_mnote_data_get_id          (ExifMnoteData *d, unsigned int n);

const char  *exif_mnote_data_get_name        (ExifMnoteData *d, unsigned int n);

const char  *exif_mnote_data_get_title       (ExifMnoteData *d, unsigned int n);

const char  *exif_mnote_data_get_description (ExifMnoteData *d, unsigned int n);

char  *exif_mnote_data_get_value (ExifMnoteData *d, unsigned int n, char *val, unsigned int maxlen);

void exif_mnote_data_log (ExifMnoteData *, ExifLog *);

#ifdef __cplusplus
}
#endif 

#endif 

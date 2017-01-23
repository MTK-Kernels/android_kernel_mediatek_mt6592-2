/*
 * Copyright (c) 2001 Lutz Mueller <lutz@users.sourceforge.net>
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

#ifndef __EXIF_CONTENT_H__
#define __EXIF_CONTENT_H__

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct _ExifContent        ExifContent;
typedef struct _ExifContentPrivate ExifContentPrivate;

#include <libexif/exif-tag.h>
#include <libexif/exif-entry.h>
#include <libexif/exif-data.h>
#include <libexif/exif-log.h>
#include <libexif/exif-mem.h>

struct _ExifContent
{
        ExifEntry **entries;
        unsigned int count;

	
	ExifData *parent;

	ExifContentPrivate *priv;
};

ExifContent *exif_content_new     (void);
ExifContent *exif_content_new_mem (ExifMem *);
void         exif_content_ref     (ExifContent *content);
void         exif_content_unref   (ExifContent *content);
void         exif_content_free    (ExifContent *content);

void         exif_content_add_entry    (ExifContent *c, ExifEntry *entry);

void         exif_content_remove_entry (ExifContent *c, ExifEntry *e);

ExifEntry   *exif_content_get_entry    (ExifContent *content, ExifTag tag);

void         exif_content_fix          (ExifContent *c);

typedef void (* ExifContentForeachEntryFunc) (ExifEntry *, void *user_data);

void         exif_content_foreach_entry (ExifContent *content,
					 ExifContentForeachEntryFunc func,
					 void *user_data);

ExifIfd exif_content_get_ifd (ExifContent *c);

#define exif_content_get_value(c,t,v,m)					\
	(exif_content_get_entry (c,t) ?					\
	 exif_entry_get_value (exif_content_get_entry (c,t),v,m) : NULL)

void exif_content_dump  (ExifContent *content, unsigned int indent);

void exif_content_log   (ExifContent *content, ExifLog *log);

#ifdef __cplusplus
}
#endif 

#endif 

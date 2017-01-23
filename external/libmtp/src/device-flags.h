/**
 * \file device-flags.h
 * Special device flags to deal with bugs in specific devices.
 *
 * Copyright (C) 2005-2007 Richard A. Low <richard@wentnet.com>
 * Copyright (C) 2005-2007 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2006-2007 Marcus Meissner
 * Copyright (C) 2007 Ted Bullock
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * This file is supposed to be included by both libmtp and libgphoto2.
 */

#define DEVICE_FLAG_NONE 0x00000000
#define DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL 0x00000001
#define DEVICE_FLAG_UNLOAD_DRIVER 0x00000002
#define DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST 0x00000004
#define DEVICE_FLAG_NO_ZERO_READS 0x00000008
#define DEVICE_FLAG_IRIVER_OGG_ALZHEIMER 0x00000010
#define DEVICE_FLAG_ONLY_7BIT_FILENAMES 0x00000020
#define DEVICE_FLAG_NO_RELEASE_INTERFACE 0x00000040
#define DEVICE_FLAG_IGNORE_HEADER_ERRORS 0x00000080
#define DEVICE_FLAG_BROKEN_SET_OBJECT_PROPLIST 0x00000100
#define DEVICE_FLAG_OGG_IS_UNKNOWN 0x00000200
#define DEVICE_FLAG_BROKEN_SET_SAMPLE_DIMENSIONS 0x00000400
#define DEVICE_FLAG_ALWAYS_PROBE_DESCRIPTOR 0x00000800
#define DEVICE_FLAG_PLAYLIST_SPL_V1 0x00001000
#define DEVICE_FLAG_PLAYLIST_SPL_V2 0x00002000
#define DEVICE_FLAG_CANNOT_HANDLE_DATEMODIFIED 0x00004000
#define DEVICE_FLAG_BROKEN_SEND_OBJECT_PROPLIST 0x00008000
#define DEVICE_FLAG_BROKEN_BATTERY_LEVEL 0x00010000

 
#define DEVICE_FLAG_DELETE_SENDS_EVENT	0x00020000

 
#define DEVICE_FLAG_CAPTURE		0x00040000

 
#define DEVICE_FLAG_CAPTURE_PREVIEW	0x00080000

 
#define DEVICE_FLAG_NIKON_BROKEN_CAPTURE	0x00100000

 
#define DEVICE_FLAG_NO_CAPTURE_COMPLETE		0x00400000

 
#define DEVICE_FLAG_MATCH_PTP_INTERFACE		0x00800000
#define DEVICE_FLAG_FLAC_IS_UNKNOWN 0x01000000

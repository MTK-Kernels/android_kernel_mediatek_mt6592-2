/**
 * \file libmtp.c
 *
 * Copyright (C) 2005-2009 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2005-2008 Richard A. Low <richard@wentnet.com>
 * Copyright (C) 2007 Ted Bullock <tbullock@canada.com>
 * Copyright (C) 2007 Tero Saarni <tero.saarni@gmail.com>
 * Copyright (C) 2008 Florent Mertens <flomertens@gmail.com>
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
 * This file provides an interface "glue" to the underlying
 * PTP implementation from libgphoto2. It uses some local
 * code to convert from/to UTF-8 (stored in unicode.c/.h)
 * and some small utility functions, mainly for debugging
 * (stored in util.c/.h).
 *
 * The three PTP files (ptp.c, ptp.h and ptp-pack.c) are
 * plain copied from the libhphoto2 codebase.
 *
 * The files libusb-glue.c/.h are just what they say: an
 * interface to libusb for the actual, physical USB traffic.
 */
#include "config.h"
#include "libmtp.h"
#include "unicode.h"
#include "ptp.h"
#include "libusb-glue.h"
#include "device-flags.h"
#include "playlist-spl.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#ifdef _MSC_VER 
#define USE_WINDOWS_IO_H
#include <io.h>
#endif


typedef struct filemap_struct {
  char *description; 
  LIBMTP_filetype_t id; 
  uint16_t ptp_id; 
  struct filemap_struct *next;
} filemap_t;

typedef struct propertymap_struct {
  char *description; 
  LIBMTP_property_t id; 
  uint16_t ptp_id; 
  struct propertymap_struct *next;
} propertymap_t;

static filemap_t *filemap = NULL;
static propertymap_t *propertymap = NULL;

static int load_cache_on_demand = 0;
static int register_filetype(char const * const description, LIBMTP_filetype_t const id,
			     uint16_t const ptp_id);
static void init_filemap();
static int register_property(char const * const description, LIBMTP_property_t const id,
			     uint16_t const ptp_id);
static void init_propertymap();
static void add_error_to_errorstack(LIBMTP_mtpdevice_t *device,
				    LIBMTP_error_number_t errornumber,
				    char const * const error_text);
static void add_ptp_error_to_errorstack(LIBMTP_mtpdevice_t *device,
					uint16_t ptp_error,
					char const * const error_text);
static void flush_handles(LIBMTP_mtpdevice_t *device);
static void get_handles_recursively(LIBMTP_mtpdevice_t *device, 
				    PTPParams *params, 
				    uint32_t storageid,
				    uint32_t parent);
static void free_storage_list(LIBMTP_mtpdevice_t *device);
static int sort_storage_by(LIBMTP_mtpdevice_t *device, int const sortby);
static uint32_t get_writeable_storageid(LIBMTP_mtpdevice_t *device, uint64_t fitsize);
static int get_storage_freespace(LIBMTP_mtpdevice_t *device,
				 LIBMTP_devicestorage_t *storage,
				 uint64_t *freespace);
static int check_if_file_fits(LIBMTP_mtpdevice_t *device, 
			      LIBMTP_devicestorage_t *storage,
			      uint64_t const filesize);
static uint16_t map_libmtp_type_to_ptp_type(LIBMTP_filetype_t intype);
static LIBMTP_filetype_t map_ptp_type_to_libmtp_type(uint16_t intype);
static uint16_t map_libmtp_property_to_ptp_property(LIBMTP_property_t inproperty);
static LIBMTP_property_t map_ptp_property_to_libmtp_property(uint16_t intype);
static int get_device_unicode_property(LIBMTP_mtpdevice_t *device,
				       char **unicstring, uint16_t property);
static uint16_t adjust_u16(uint16_t val, PTPObjectPropDesc *opd);
static uint32_t adjust_u32(uint32_t val, PTPObjectPropDesc *opd);
static char *get_iso8601_stamp(void);
static char *get_string_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    uint16_t const attribute_id);
static uint64_t get_u64_from_object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
                                    uint16_t const attribute_id, uint64_t const value_default);
static uint32_t get_u32_from_object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
				    uint16_t const attribute_id, uint32_t const value_default);
static uint16_t get_u16_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    uint16_t const attribute_id, uint16_t const value_default);
static uint8_t get_u8_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				  uint16_t const attribute_id, uint8_t const value_default);
static int set_object_string(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			     uint16_t const attribute_id, char const * const string);
static int set_object_u32(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  uint16_t const attribute_id, uint32_t const value);
static int set_object_u16(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  uint16_t const attribute_id, uint16_t const value);
static int set_object_u8(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			 uint16_t const attribute_id, uint8_t const value);
static void get_track_metadata(LIBMTP_mtpdevice_t *device, uint16_t objectformat,
			       LIBMTP_track_t *track);
static LIBMTP_folder_t *get_subfolders_for_folder(LIBMTP_folder_t *list, uint32_t parent);
static int create_new_abstract_list(LIBMTP_mtpdevice_t *device,
				    char const * const name,
				    char const * const artist,
				    char const * const composer,
				    char const * const genre,
				    uint32_t const parenthandle,
				    uint32_t const storageid,
				    uint16_t const objectformat,
				    char const * const suffix,
				    uint32_t * const newid,
				    uint32_t const * const tracks,
				    uint32_t const no_tracks);
static int update_abstract_list(LIBMTP_mtpdevice_t *device,
				char const * const name,
				char const * const artist,
				char const * const composer,
				char const * const genre,
				uint32_t const objecthandle,
				uint16_t const objectformat,
				uint32_t const * const tracks,
				uint32_t const no_tracks);
static int send_file_object_info(LIBMTP_mtpdevice_t *device, LIBMTP_file_t *filedata);
static void add_object_to_cache(LIBMTP_mtpdevice_t *device, uint32_t object_id);
static void update_metadata_cache(LIBMTP_mtpdevice_t *device, uint32_t object_id);
static int set_object_filename(LIBMTP_mtpdevice_t *device,
		uint32_t object_id,
		uint16_t ptp_type,
                const char **newname);
                
typedef struct _MTPDataHandler {
	MTPDataGetFunc		getfunc;
	MTPDataPutFunc		putfunc;
	void			*priv;
} MTPDataHandler;

static uint16_t get_func_wrapper(PTPParams* params, void* priv, unsigned long wantlen, unsigned char *data, unsigned long *gotlen);
static uint16_t put_func_wrapper(PTPParams* params, void* priv, unsigned long sendlen, unsigned char *data, unsigned long *putlen);
                
static int has_ogg_extension(char *name) {
  char *ptype;

  if (name == NULL)
    return 0;
  ptype = strrchr(name,'.');
  if (ptype == NULL)
    return 0;
  if (!strcasecmp (ptype, ".ogg"))
    return 1;
  return 0;
}

static int has_flac_extension(char *name) {
  char *ptype;

  if (name == NULL)
    return 0;
  ptype = strrchr(name,'.');
  if (ptype == NULL)
    return 0;
  if (!strcasecmp (ptype, ".flac"))
    return 1;
  return 0;
}



static filemap_t *new_filemap_entry()
{
  filemap_t *filemap;

  filemap = (filemap_t *)malloc(sizeof(filemap_t));

  if( filemap != NULL ) {
    filemap->description = NULL;
    filemap->id = LIBMTP_FILETYPE_UNKNOWN;
    filemap->ptp_id = PTP_OFC_Undefined;
    filemap->next = NULL;
  }

  return filemap;
}

static int register_filetype(char const * const description, LIBMTP_filetype_t const id,
			     uint16_t const ptp_id)
{
  filemap_t *new = NULL, *current;

  
  current = filemap;
  while (current != NULL) {
    if(current->id == id) {
      break;
    }
    current = current->next;
  }

  
  if(current == NULL) {
    new = new_filemap_entry();
    if(new == NULL) {
      return 1;
    }

    new->id = id;
    if(description != NULL) {
      new->description = strdup(description);
    }
    new->ptp_id = ptp_id;

    
    if(filemap == NULL) {
      filemap = new;
    } else {
      current = filemap;
      while (current->next != NULL ) current=current->next;
      current->next = new;
    }
    
  } else {
    if (current->description != NULL) {
      free(current->description);
    }
    current->description = NULL;
    if(description != NULL) {
      current->description = strdup(description);
    }
    current->ptp_id = ptp_id;
  }

  return 0;
}

static void init_filemap()
{
  register_filetype("Folder", LIBMTP_FILETYPE_FOLDER, PTP_OFC_Association);
  register_filetype("MediaCard", LIBMTP_FILETYPE_MEDIACARD, PTP_OFC_MTP_MediaCard);
  register_filetype("RIFF WAVE file", LIBMTP_FILETYPE_WAV, PTP_OFC_WAV);
  register_filetype("ISO MPEG-1 Audio Layer 3", LIBMTP_FILETYPE_MP3, PTP_OFC_MP3);
  register_filetype("ISO MPEG-1 Audio Layer 2", LIBMTP_FILETYPE_MP2, PTP_OFC_MTP_MP2);
  register_filetype("Microsoft Windows Media Audio", LIBMTP_FILETYPE_WMA, PTP_OFC_MTP_WMA);
  register_filetype("Ogg container format", LIBMTP_FILETYPE_OGG, PTP_OFC_MTP_OGG);
  register_filetype("Free Lossless Audio Codec (FLAC)", LIBMTP_FILETYPE_FLAC, PTP_OFC_MTP_FLAC);
  register_filetype("Advanced Audio Coding (AAC)/MPEG-2 Part 7/MPEG-4 Part 3", LIBMTP_FILETYPE_AAC, PTP_OFC_MTP_AAC);
  register_filetype("MPEG-4 Part 14 Container Format (Audio Emphasis)", LIBMTP_FILETYPE_M4A, PTP_OFC_MTP_M4A);
  register_filetype("MPEG-4 Part 14 Container Format (Audio+Video Emphasis)", LIBMTP_FILETYPE_MP4, PTP_OFC_MTP_MP4);
  register_filetype("Audible.com Audio Codec", LIBMTP_FILETYPE_AUDIBLE, PTP_OFC_MTP_AudibleCodec);
  register_filetype("Undefined audio file", LIBMTP_FILETYPE_UNDEF_AUDIO, PTP_OFC_MTP_UndefinedAudio);
  register_filetype("Microsoft Windows Media Video", LIBMTP_FILETYPE_WMV, PTP_OFC_MTP_WMV);
  register_filetype("Audio Video Interleave", LIBMTP_FILETYPE_AVI, PTP_OFC_AVI);
  register_filetype("MPEG video stream", LIBMTP_FILETYPE_MPEG, PTP_OFC_MPEG);
  register_filetype("Microsoft Advanced Systems Format", LIBMTP_FILETYPE_ASF, PTP_OFC_ASF);
  register_filetype("Apple Quicktime container format", LIBMTP_FILETYPE_QT, PTP_OFC_QT);
  register_filetype("Undefined video file", LIBMTP_FILETYPE_UNDEF_VIDEO, PTP_OFC_MTP_UndefinedVideo);
  register_filetype("JPEG file", LIBMTP_FILETYPE_JPEG, PTP_OFC_EXIF_JPEG);
  register_filetype("JP2 file", LIBMTP_FILETYPE_JP2, PTP_OFC_JP2);
  register_filetype("JPX file", LIBMTP_FILETYPE_JPX, PTP_OFC_JPX);
  register_filetype("JFIF file", LIBMTP_FILETYPE_JFIF, PTP_OFC_JFIF);
  register_filetype("TIFF bitmap file", LIBMTP_FILETYPE_TIFF, PTP_OFC_TIFF);
  register_filetype("BMP bitmap file", LIBMTP_FILETYPE_BMP, PTP_OFC_BMP);
  register_filetype("GIF bitmap file", LIBMTP_FILETYPE_GIF, PTP_OFC_GIF);
  register_filetype("PICT bitmap file", LIBMTP_FILETYPE_PICT, PTP_OFC_PICT);
  register_filetype("Portable Network Graphics", LIBMTP_FILETYPE_PNG, PTP_OFC_PNG);
  register_filetype("Microsoft Windows Image Format", LIBMTP_FILETYPE_WINDOWSIMAGEFORMAT, PTP_OFC_MTP_WindowsImageFormat);
  register_filetype("VCalendar version 1", LIBMTP_FILETYPE_VCALENDAR1, PTP_OFC_MTP_vCalendar1);
  register_filetype("VCalendar version 2", LIBMTP_FILETYPE_VCALENDAR2, PTP_OFC_MTP_vCalendar2);
  register_filetype("VCard version 2", LIBMTP_FILETYPE_VCARD2, PTP_OFC_MTP_vCard2);
  register_filetype("VCard version 3", LIBMTP_FILETYPE_VCARD3, PTP_OFC_MTP_vCard3);
  register_filetype("Undefined Windows executable file", LIBMTP_FILETYPE_WINEXEC, PTP_OFC_MTP_UndefinedWindowsExecutable);
  register_filetype("Text file", LIBMTP_FILETYPE_TEXT, PTP_OFC_Text);
  register_filetype("HTML file", LIBMTP_FILETYPE_HTML, PTP_OFC_HTML);
  register_filetype("XML file", LIBMTP_FILETYPE_XML, PTP_OFC_MTP_XMLDocument);
  register_filetype("DOC file", LIBMTP_FILETYPE_DOC, PTP_OFC_MTP_MSWordDocument);
  register_filetype("XLS file", LIBMTP_FILETYPE_XLS, PTP_OFC_MTP_MSExcelSpreadsheetXLS);
  register_filetype("PPT file", LIBMTP_FILETYPE_PPT, PTP_OFC_MTP_MSPowerpointPresentationPPT);
  register_filetype("MHT file", LIBMTP_FILETYPE_MHT, PTP_OFC_MTP_MHTCompiledHTMLDocument);
  register_filetype("Firmware file", LIBMTP_FILETYPE_FIRMWARE, PTP_OFC_MTP_Firmware);
  register_filetype("Abstract Album file", LIBMTP_FILETYPE_ALBUM, PTP_OFC_MTP_AbstractAudioAlbum);
  register_filetype("Abstract Playlist file", LIBMTP_FILETYPE_PLAYLIST, PTP_OFC_MTP_AbstractAudioVideoPlaylist);
  register_filetype("Undefined filetype", LIBMTP_FILETYPE_UNKNOWN, PTP_OFC_Undefined);
}

static uint16_t map_libmtp_type_to_ptp_type(LIBMTP_filetype_t intype)
{
  filemap_t *current;

  current = filemap;

  while (current != NULL) {
    if(current->id == intype) {
      return current->ptp_id;
    }
    current = current->next;
  }
  
  return PTP_OFC_Undefined;
}


static LIBMTP_filetype_t map_ptp_type_to_libmtp_type(uint16_t intype)
{
  filemap_t *current;

  current = filemap;

  while (current != NULL) {
    if(current->ptp_id == intype) {
      return current->id;
    }
    current = current->next;
  }
  
  return LIBMTP_FILETYPE_UNKNOWN;
}

static propertymap_t *new_propertymap_entry()
{
  propertymap_t *propertymap;

  propertymap = (propertymap_t *)malloc(sizeof(propertymap_t));

  if( propertymap != NULL ) {
    propertymap->description = NULL;
    propertymap->id = LIBMTP_PROPERTY_UNKNOWN;
    propertymap->ptp_id = 0;
    propertymap->next = NULL;
  }

  return propertymap;
}

static int register_property(char const * const description, LIBMTP_property_t const id,
			     uint16_t const ptp_id)
{
  propertymap_t *new = NULL, *current;

  
  current = propertymap;
  while (current != NULL) {
    if(current->id == id) {
      break;
    }
    current = current->next;
  }

  
  if(current == NULL) {
    new = new_propertymap_entry();
    if(new == NULL) {
      return 1;
    }

    new->id = id;
    if(description != NULL) {
      new->description = strdup(description);
    }
    new->ptp_id = ptp_id;

    
    if(propertymap == NULL) {
      propertymap = new;
    } else {
      current = propertymap;
      while (current->next != NULL ) current=current->next;
      current->next = new;
    }
    
  } else {
    if (current->description != NULL) {
      free(current->description);
    }
    current->description = NULL;
    if(description != NULL) {
      current->description = strdup(description);
    }
    current->ptp_id = ptp_id;
  }

  return 0;
}

static void init_propertymap()
{
  register_property("Storage ID", LIBMTP_PROPERTY_StorageID, PTP_OPC_StorageID);
  register_property("Object Format", LIBMTP_PROPERTY_ObjectFormat, PTP_OPC_ObjectFormat);
  register_property("Protection Status", LIBMTP_PROPERTY_ProtectionStatus, PTP_OPC_ProtectionStatus);
  register_property("Object Size", LIBMTP_PROPERTY_ObjectSize, PTP_OPC_ObjectSize);
  register_property("Association Type", LIBMTP_PROPERTY_AssociationType, PTP_OPC_AssociationType);
  register_property("Association Desc", LIBMTP_PROPERTY_AssociationDesc, PTP_OPC_AssociationDesc);
  register_property("Object File Name", LIBMTP_PROPERTY_ObjectFileName, PTP_OPC_ObjectFileName);
  register_property("Date Created", LIBMTP_PROPERTY_DateCreated, PTP_OPC_DateCreated);
  register_property("Date Modified", LIBMTP_PROPERTY_DateModified, PTP_OPC_DateModified);
  register_property("Keywords", LIBMTP_PROPERTY_Keywords, PTP_OPC_Keywords);
  register_property("Parent Object", LIBMTP_PROPERTY_ParentObject, PTP_OPC_ParentObject);
  register_property("Allowed Folder Contents", LIBMTP_PROPERTY_AllowedFolderContents, PTP_OPC_AllowedFolderContents);
  register_property("Hidden", LIBMTP_PROPERTY_Hidden, PTP_OPC_Hidden);
  register_property("System Object", LIBMTP_PROPERTY_SystemObject, PTP_OPC_SystemObject);
  register_property("Persistant Unique Object Identifier", LIBMTP_PROPERTY_PersistantUniqueObjectIdentifier, PTP_OPC_PersistantUniqueObjectIdentifier);
  register_property("Sync ID", LIBMTP_PROPERTY_SyncID, PTP_OPC_SyncID);
  register_property("Property Bag", LIBMTP_PROPERTY_PropertyBag, PTP_OPC_PropertyBag);
  register_property("Name", LIBMTP_PROPERTY_Name, PTP_OPC_Name);
  register_property("Created By", LIBMTP_PROPERTY_CreatedBy, PTP_OPC_CreatedBy);
  register_property("Artist", LIBMTP_PROPERTY_Artist, PTP_OPC_Artist);
  register_property("Date Authored", LIBMTP_PROPERTY_DateAuthored, PTP_OPC_DateAuthored);
  register_property("Description", LIBMTP_PROPERTY_Description, PTP_OPC_Description);
  register_property("URL Reference", LIBMTP_PROPERTY_URLReference, PTP_OPC_URLReference);
  register_property("Language Locale", LIBMTP_PROPERTY_LanguageLocale, PTP_OPC_LanguageLocale);
  register_property("Copyright Information", LIBMTP_PROPERTY_CopyrightInformation, PTP_OPC_CopyrightInformation);
  register_property("Source", LIBMTP_PROPERTY_Source, PTP_OPC_Source);
  register_property("Origin Location", LIBMTP_PROPERTY_OriginLocation, PTP_OPC_OriginLocation);
  register_property("Date Added", LIBMTP_PROPERTY_DateAdded, PTP_OPC_DateAdded);
  register_property("Non Consumable", LIBMTP_PROPERTY_NonConsumable, PTP_OPC_NonConsumable);
  register_property("Corrupt Or Unplayable", LIBMTP_PROPERTY_CorruptOrUnplayable, PTP_OPC_CorruptOrUnplayable);
  register_property("Producer Serial Number", LIBMTP_PROPERTY_ProducerSerialNumber, PTP_OPC_ProducerSerialNumber);
  register_property("Representative Sample Format", LIBMTP_PROPERTY_RepresentativeSampleFormat, PTP_OPC_RepresentativeSampleFormat);
  register_property("Representative Sample Sise", LIBMTP_PROPERTY_RepresentativeSampleSize, PTP_OPC_RepresentativeSampleSize);
  register_property("Representative Sample Height", LIBMTP_PROPERTY_RepresentativeSampleHeight, PTP_OPC_RepresentativeSampleHeight);
  register_property("Representative Sample Width", LIBMTP_PROPERTY_RepresentativeSampleWidth, PTP_OPC_RepresentativeSampleWidth);
  register_property("Representative Sample Duration", LIBMTP_PROPERTY_RepresentativeSampleDuration, PTP_OPC_RepresentativeSampleDuration);
  register_property("Representative Sample Data", LIBMTP_PROPERTY_RepresentativeSampleData, PTP_OPC_RepresentativeSampleData);
  register_property("Width", LIBMTP_PROPERTY_Width, PTP_OPC_Width);
  register_property("Height", LIBMTP_PROPERTY_Height, PTP_OPC_Height);
  register_property("Duration", LIBMTP_PROPERTY_Duration, PTP_OPC_Duration);
  register_property("Rating", LIBMTP_PROPERTY_Rating, PTP_OPC_Rating);
  register_property("Track", LIBMTP_PROPERTY_Track, PTP_OPC_Track);
  register_property("Genre", LIBMTP_PROPERTY_Genre, PTP_OPC_Genre);
  register_property("Credits", LIBMTP_PROPERTY_Credits, PTP_OPC_Credits);
  register_property("Lyrics", LIBMTP_PROPERTY_Lyrics, PTP_OPC_Lyrics);
  register_property("Subscription Content ID", LIBMTP_PROPERTY_SubscriptionContentID, PTP_OPC_SubscriptionContentID);
  register_property("Produced By", LIBMTP_PROPERTY_ProducedBy, PTP_OPC_ProducedBy);
  register_property("Use Count", LIBMTP_PROPERTY_UseCount, PTP_OPC_UseCount);
  register_property("Skip Count", LIBMTP_PROPERTY_SkipCount, PTP_OPC_SkipCount);
  register_property("Last Accessed", LIBMTP_PROPERTY_LastAccessed, PTP_OPC_LastAccessed);
  register_property("Parental Rating", LIBMTP_PROPERTY_ParentalRating, PTP_OPC_ParentalRating);
  register_property("Meta Genre", LIBMTP_PROPERTY_MetaGenre, PTP_OPC_MetaGenre);
  register_property("Composer", LIBMTP_PROPERTY_Composer, PTP_OPC_Composer);
  register_property("Effective Rating", LIBMTP_PROPERTY_EffectiveRating, PTP_OPC_EffectiveRating);
  register_property("Subtitle", LIBMTP_PROPERTY_Subtitle, PTP_OPC_Subtitle);
  register_property("Original Release Date", LIBMTP_PROPERTY_OriginalReleaseDate, PTP_OPC_OriginalReleaseDate);
  register_property("Album Name", LIBMTP_PROPERTY_AlbumName, PTP_OPC_AlbumName);
  register_property("Album Artist", LIBMTP_PROPERTY_AlbumArtist, PTP_OPC_AlbumArtist);
  register_property("Mood", LIBMTP_PROPERTY_Mood, PTP_OPC_Mood);
  register_property("DRM Status", LIBMTP_PROPERTY_DRMStatus, PTP_OPC_DRMStatus);
  register_property("Sub Description", LIBMTP_PROPERTY_SubDescription, PTP_OPC_SubDescription);
  register_property("Is Cropped", LIBMTP_PROPERTY_IsCropped, PTP_OPC_IsCropped);
  register_property("Is Color Corrected", LIBMTP_PROPERTY_IsColorCorrected, PTP_OPC_IsColorCorrected);
  register_property("Image Bit Depth", LIBMTP_PROPERTY_ImageBitDepth, PTP_OPC_ImageBitDepth);
  register_property("f Number", LIBMTP_PROPERTY_Fnumber, PTP_OPC_Fnumber);
  register_property("Exposure Time", LIBMTP_PROPERTY_ExposureTime, PTP_OPC_ExposureTime);
  register_property("Exposure Index", LIBMTP_PROPERTY_ExposureIndex, PTP_OPC_ExposureIndex);
  register_property("Display Name", LIBMTP_PROPERTY_DisplayName, PTP_OPC_DisplayName);
  register_property("Body Text", LIBMTP_PROPERTY_BodyText, PTP_OPC_BodyText);
  register_property("Subject", LIBMTP_PROPERTY_Subject, PTP_OPC_Subject);
  register_property("Priority", LIBMTP_PROPERTY_Priority, PTP_OPC_Priority);
  register_property("Given Name", LIBMTP_PROPERTY_GivenName, PTP_OPC_GivenName);
  register_property("Middle Names", LIBMTP_PROPERTY_MiddleNames, PTP_OPC_MiddleNames);
  register_property("Family Name", LIBMTP_PROPERTY_FamilyName, PTP_OPC_FamilyName);
  register_property("Prefix", LIBMTP_PROPERTY_Prefix, PTP_OPC_Prefix);
  register_property("Suffix", LIBMTP_PROPERTY_Suffix, PTP_OPC_Suffix);
  register_property("Phonetic Given Name", LIBMTP_PROPERTY_PhoneticGivenName, PTP_OPC_PhoneticGivenName);
  register_property("Phonetic Family Name", LIBMTP_PROPERTY_PhoneticFamilyName, PTP_OPC_PhoneticFamilyName);
  register_property("Email: Primary", LIBMTP_PROPERTY_EmailPrimary, PTP_OPC_EmailPrimary);
  register_property("Email: Personal 1", LIBMTP_PROPERTY_EmailPersonal1, PTP_OPC_EmailPersonal1);
  register_property("Email: Personal 2", LIBMTP_PROPERTY_EmailPersonal2, PTP_OPC_EmailPersonal2);
  register_property("Email: Business 1", LIBMTP_PROPERTY_EmailBusiness1, PTP_OPC_EmailBusiness1);
  register_property("Email: Business 2", LIBMTP_PROPERTY_EmailBusiness2, PTP_OPC_EmailBusiness2);
  register_property("Email: Others", LIBMTP_PROPERTY_EmailOthers, PTP_OPC_EmailOthers);
  register_property("Phone Number: Primary", LIBMTP_PROPERTY_PhoneNumberPrimary, PTP_OPC_PhoneNumberPrimary);
  register_property("Phone Number: Personal", LIBMTP_PROPERTY_PhoneNumberPersonal, PTP_OPC_PhoneNumberPersonal);
  register_property("Phone Number: Personal 2", LIBMTP_PROPERTY_PhoneNumberPersonal2, PTP_OPC_PhoneNumberPersonal2);
  register_property("Phone Number: Business", LIBMTP_PROPERTY_PhoneNumberBusiness, PTP_OPC_PhoneNumberBusiness);
  register_property("Phone Number: Business 2", LIBMTP_PROPERTY_PhoneNumberBusiness2, PTP_OPC_PhoneNumberBusiness2);
  register_property("Phone Number: Mobile", LIBMTP_PROPERTY_PhoneNumberMobile, PTP_OPC_PhoneNumberMobile);
  register_property("Phone Number: Mobile 2", LIBMTP_PROPERTY_PhoneNumberMobile2, PTP_OPC_PhoneNumberMobile2);
  register_property("Fax Number: Primary", LIBMTP_PROPERTY_FaxNumberPrimary, PTP_OPC_FaxNumberPrimary);
  register_property("Fax Number: Personal", LIBMTP_PROPERTY_FaxNumberPersonal, PTP_OPC_FaxNumberPersonal);
  register_property("Fax Number: Business", LIBMTP_PROPERTY_FaxNumberBusiness, PTP_OPC_FaxNumberBusiness);
  register_property("Pager Number", LIBMTP_PROPERTY_PagerNumber, PTP_OPC_PagerNumber);
  register_property("Phone Number: Others", LIBMTP_PROPERTY_PhoneNumberOthers, PTP_OPC_PhoneNumberOthers);
  register_property("Primary Web Address", LIBMTP_PROPERTY_PrimaryWebAddress, PTP_OPC_PrimaryWebAddress);
  register_property("Personal Web Address", LIBMTP_PROPERTY_PersonalWebAddress, PTP_OPC_PersonalWebAddress);
  register_property("Business Web Address", LIBMTP_PROPERTY_BusinessWebAddress, PTP_OPC_BusinessWebAddress);
  register_property("Instant Messenger Address 1", LIBMTP_PROPERTY_InstantMessengerAddress, PTP_OPC_InstantMessengerAddress);
  register_property("Instant Messenger Address 2", LIBMTP_PROPERTY_InstantMessengerAddress2, PTP_OPC_InstantMessengerAddress2);
  register_property("Instant Messenger Address 3", LIBMTP_PROPERTY_InstantMessengerAddress3, PTP_OPC_InstantMessengerAddress3);
  register_property("Postal Address: Personal: Full", LIBMTP_PROPERTY_PostalAddressPersonalFull, PTP_OPC_PostalAddressPersonalFull);
  register_property("Postal Address: Personal: Line 1", LIBMTP_PROPERTY_PostalAddressPersonalFullLine1, PTP_OPC_PostalAddressPersonalFullLine1);
  register_property("Postal Address: Personal: Line 2", LIBMTP_PROPERTY_PostalAddressPersonalFullLine2, PTP_OPC_PostalAddressPersonalFullLine2);
  register_property("Postal Address: Personal: City", LIBMTP_PROPERTY_PostalAddressPersonalFullCity, PTP_OPC_PostalAddressPersonalFullCity);
  register_property("Postal Address: Personal: Region", LIBMTP_PROPERTY_PostalAddressPersonalFullRegion, PTP_OPC_PostalAddressPersonalFullRegion);
  register_property("Postal Address: Personal: Postal Code", LIBMTP_PROPERTY_PostalAddressPersonalFullPostalCode, PTP_OPC_PostalAddressPersonalFullPostalCode);
  register_property("Postal Address: Personal: Country", LIBMTP_PROPERTY_PostalAddressPersonalFullCountry, PTP_OPC_PostalAddressPersonalFullCountry);
  register_property("Postal Address: Business: Full", LIBMTP_PROPERTY_PostalAddressBusinessFull, PTP_OPC_PostalAddressBusinessFull);
  register_property("Postal Address: Business: Line 1", LIBMTP_PROPERTY_PostalAddressBusinessLine1, PTP_OPC_PostalAddressBusinessLine1);
  register_property("Postal Address: Business: Line 2", LIBMTP_PROPERTY_PostalAddressBusinessLine2, PTP_OPC_PostalAddressBusinessLine2);
  register_property("Postal Address: Business: City", LIBMTP_PROPERTY_PostalAddressBusinessCity, PTP_OPC_PostalAddressBusinessCity);
  register_property("Postal Address: Business: Region", LIBMTP_PROPERTY_PostalAddressBusinessRegion, PTP_OPC_PostalAddressBusinessRegion);
  register_property("Postal Address: Business: Postal Code", LIBMTP_PROPERTY_PostalAddressBusinessPostalCode, PTP_OPC_PostalAddressBusinessPostalCode);
  register_property("Postal Address: Business: Country", LIBMTP_PROPERTY_PostalAddressBusinessCountry, PTP_OPC_PostalAddressBusinessCountry);
  register_property("Postal Address: Other: Full", LIBMTP_PROPERTY_PostalAddressOtherFull, PTP_OPC_PostalAddressOtherFull);
  register_property("Postal Address: Other: Line 1", LIBMTP_PROPERTY_PostalAddressOtherLine1, PTP_OPC_PostalAddressOtherLine1);
  register_property("Postal Address: Other: Line 2", LIBMTP_PROPERTY_PostalAddressOtherLine2, PTP_OPC_PostalAddressOtherLine2);
  register_property("Postal Address: Other: City", LIBMTP_PROPERTY_PostalAddressOtherCity, PTP_OPC_PostalAddressOtherCity);
  register_property("Postal Address: Other: Region", LIBMTP_PROPERTY_PostalAddressOtherRegion, PTP_OPC_PostalAddressOtherRegion);
  register_property("Postal Address: Other: Postal Code", LIBMTP_PROPERTY_PostalAddressOtherPostalCode, PTP_OPC_PostalAddressOtherPostalCode);
  register_property("Postal Address: Other: Counrtry", LIBMTP_PROPERTY_PostalAddressOtherCountry, PTP_OPC_PostalAddressOtherCountry);
  register_property("Organization Name", LIBMTP_PROPERTY_OrganizationName, PTP_OPC_OrganizationName);
  register_property("Phonetic Organization Name", LIBMTP_PROPERTY_PhoneticOrganizationName, PTP_OPC_PhoneticOrganizationName);
  register_property("Role", LIBMTP_PROPERTY_Role, PTP_OPC_Role);
  register_property("Birthdate", LIBMTP_PROPERTY_Birthdate, PTP_OPC_Birthdate);
  register_property("Message To", LIBMTP_PROPERTY_MessageTo, PTP_OPC_MessageTo);
  register_property("Message CC", LIBMTP_PROPERTY_MessageCC, PTP_OPC_MessageCC);
  register_property("Message BCC", LIBMTP_PROPERTY_MessageBCC, PTP_OPC_MessageBCC);
  register_property("Message Read", LIBMTP_PROPERTY_MessageRead, PTP_OPC_MessageRead);
  register_property("Message Received Time", LIBMTP_PROPERTY_MessageReceivedTime, PTP_OPC_MessageReceivedTime);
  register_property("Message Sender", LIBMTP_PROPERTY_MessageSender, PTP_OPC_MessageSender);
  register_property("Activity Begin Time", LIBMTP_PROPERTY_ActivityBeginTime, PTP_OPC_ActivityBeginTime);
  register_property("Activity End Time", LIBMTP_PROPERTY_ActivityEndTime, PTP_OPC_ActivityEndTime);
  register_property("Activity Location", LIBMTP_PROPERTY_ActivityLocation, PTP_OPC_ActivityLocation);
  register_property("Activity Required Attendees", LIBMTP_PROPERTY_ActivityRequiredAttendees, PTP_OPC_ActivityRequiredAttendees);
  register_property("Optional Attendees", LIBMTP_PROPERTY_ActivityOptionalAttendees, PTP_OPC_ActivityOptionalAttendees);
  register_property("Activity Resources", LIBMTP_PROPERTY_ActivityResources, PTP_OPC_ActivityResources);
  register_property("Activity Accepted", LIBMTP_PROPERTY_ActivityAccepted, PTP_OPC_ActivityAccepted);
  register_property("Owner", LIBMTP_PROPERTY_Owner, PTP_OPC_Owner);
  register_property("Editor", LIBMTP_PROPERTY_Editor, PTP_OPC_Editor);
  register_property("Webmaster", LIBMTP_PROPERTY_Webmaster, PTP_OPC_Webmaster);
  register_property("URL Source", LIBMTP_PROPERTY_URLSource, PTP_OPC_URLSource);
  register_property("URL Destination", LIBMTP_PROPERTY_URLDestination, PTP_OPC_URLDestination);
  register_property("Time Bookmark", LIBMTP_PROPERTY_TimeBookmark, PTP_OPC_TimeBookmark);
  register_property("Object Bookmark", LIBMTP_PROPERTY_ObjectBookmark, PTP_OPC_ObjectBookmark);
  register_property("Byte Bookmark", LIBMTP_PROPERTY_ByteBookmark, PTP_OPC_ByteBookmark);
  register_property("Last Build Date", LIBMTP_PROPERTY_LastBuildDate, PTP_OPC_LastBuildDate);
  register_property("Time To Live", LIBMTP_PROPERTY_TimetoLive, PTP_OPC_TimetoLive);
  register_property("Media GUID", LIBMTP_PROPERTY_MediaGUID, PTP_OPC_MediaGUID);
  register_property("Total Bit Rate", LIBMTP_PROPERTY_TotalBitRate, PTP_OPC_TotalBitRate);
  register_property("Bit Rate Type", LIBMTP_PROPERTY_BitRateType, PTP_OPC_BitRateType);
  register_property("Sample Rate", LIBMTP_PROPERTY_SampleRate, PTP_OPC_SampleRate);
  register_property("Number Of Channels", LIBMTP_PROPERTY_NumberOfChannels, PTP_OPC_NumberOfChannels);
  register_property("Audio Bit Depth", LIBMTP_PROPERTY_AudioBitDepth, PTP_OPC_AudioBitDepth);
  register_property("Scan Depth", LIBMTP_PROPERTY_ScanDepth, PTP_OPC_ScanDepth);
  register_property("Audio WAVE Codec", LIBMTP_PROPERTY_AudioWAVECodec, PTP_OPC_AudioWAVECodec);
  register_property("Audio Bit Rate", LIBMTP_PROPERTY_AudioBitRate, PTP_OPC_AudioBitRate);
  register_property("Video Four CC Codec", LIBMTP_PROPERTY_VideoFourCCCodec, PTP_OPC_VideoFourCCCodec);
  register_property("Video Bit Rate", LIBMTP_PROPERTY_VideoBitRate, PTP_OPC_VideoBitRate);
  register_property("Frames Per Thousand Seconds", LIBMTP_PROPERTY_FramesPerThousandSeconds, PTP_OPC_FramesPerThousandSeconds);
  register_property("Key Frame Distance", LIBMTP_PROPERTY_KeyFrameDistance, PTP_OPC_KeyFrameDistance);
  register_property("Buffer Size", LIBMTP_PROPERTY_BufferSize, PTP_OPC_BufferSize);
  register_property("Encoding Quality", LIBMTP_PROPERTY_EncodingQuality, PTP_OPC_EncodingQuality);
  register_property("Encoding Profile", LIBMTP_PROPERTY_EncodingProfile, PTP_OPC_EncodingProfile);
  register_property("Buy flag", LIBMTP_PROPERTY_BuyFlag, PTP_OPC_BuyFlag);
  register_property("Unknown property", LIBMTP_PROPERTY_UNKNOWN, 0);
}

static uint16_t map_libmtp_property_to_ptp_property(LIBMTP_property_t inproperty)
{
  propertymap_t *current;

  current = propertymap;

  while (current != NULL) {
    if(current->id == inproperty) {
      return current->ptp_id;
    }
    current = current->next;
  }
  return 0;
}


static LIBMTP_property_t map_ptp_property_to_libmtp_property(uint16_t inproperty)
{
  propertymap_t *current;

  current = propertymap;

  while (current != NULL) {
    if(current->ptp_id == inproperty) {
      return current->id;
    }
    current = current->next;
  }
  
  return LIBMTP_PROPERTY_UNKNOWN;
}


void LIBMTP_Init(void)
{
  init_filemap();
  init_propertymap();
  return;
}


char const * LIBMTP_Get_Filetype_Description(LIBMTP_filetype_t intype)
{
  filemap_t *current;

  current = filemap;

  while (current != NULL) {
    if(current->id == intype) {
      return current->description;
    }
    current = current->next;
  }

  return "Unknown filetype";
}

char const * LIBMTP_Get_Property_Description(LIBMTP_property_t inproperty)
{
  propertymap_t *current;

  current = propertymap;

  while (current != NULL) {
    if(current->id == inproperty) {
      return current->description;
    }
    current = current->next;
  }

  return "Unknown property";
}

static uint16_t adjust_u16(uint16_t val, PTPObjectPropDesc *opd)
{
  switch (opd->FormFlag) {
  case PTP_DPFF_Range:
    if (val < opd->FORM.Range.MinimumValue.u16) {
      return opd->FORM.Range.MinimumValue.u16;
    }
    if (val > opd->FORM.Range.MaximumValue.u16) {
      return opd->FORM.Range.MaximumValue.u16;
    }
    
    if (val % opd->FORM.Range.StepSize.u16 != 0) {
      return val - (val % opd->FORM.Range.StepSize.u16);
    }
    return val;
    break;
  case PTP_DPFF_Enumeration:
    {
      int i;
      uint16_t bestfit = opd->FORM.Enum.SupportedValue[0].u16;
      
      for (i=0; i<opd->FORM.Enum.NumberOfValues; i++) {
	if (val == opd->FORM.Enum.SupportedValue[i].u16) {
	  return val;
	}
	
	if (opd->FORM.Enum.SupportedValue[i].u16 < val) {
	  bestfit = opd->FORM.Enum.SupportedValue[i].u16;
	}
      }
      
      return bestfit;
    }
  default:
    
    break;
  }
  return val;
}

static uint32_t adjust_u32(uint32_t val, PTPObjectPropDesc *opd)
{
  switch (opd->FormFlag) {
  case PTP_DPFF_Range:
    if (val < opd->FORM.Range.MinimumValue.u32) {
      return opd->FORM.Range.MinimumValue.u32;
    }
    if (val > opd->FORM.Range.MaximumValue.u32) {
      return opd->FORM.Range.MaximumValue.u32;
    }
    
    if (val % opd->FORM.Range.StepSize.u32 != 0) {
      return val - (val % opd->FORM.Range.StepSize.u32);
    }
    return val;
    break;
  case PTP_DPFF_Enumeration:
    {
      int i;
      uint32_t bestfit = opd->FORM.Enum.SupportedValue[0].u32;

      for (i=0; i<opd->FORM.Enum.NumberOfValues; i++) {
	if (val == opd->FORM.Enum.SupportedValue[i].u32) {
	  return val;
	}
	
	if (opd->FORM.Enum.SupportedValue[i].u32 < val) {
	  bestfit = opd->FORM.Enum.SupportedValue[i].u32;
	}
      }
      
      return bestfit;
    }
  default:
    
    break;
  }
  return val;
}

static char *get_iso8601_stamp(void)
{
  time_t curtime;
  struct tm *loctime;
  char tmp[64];

  curtime = time(NULL);
  loctime = localtime(&curtime);
  strftime (tmp, sizeof(tmp), "%Y%m%dT%H%M%S.0%z", loctime);
  return strdup(tmp);
}

int LIBMTP_Get_Allowed_Property_Values(LIBMTP_mtpdevice_t *device, LIBMTP_property_t const property,
            LIBMTP_filetype_t const filetype, LIBMTP_allowed_values_t *allowed_vals)
{
  PTPObjectPropDesc opd;
  uint16_t ret = 0;
  
  ret = ptp_mtp_getobjectpropdesc(device->params, map_libmtp_property_to_ptp_property(property), map_libmtp_type_to_ptp_type(filetype), &opd);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Allowed_Property_Values(): could not get property description.");
    return -1;
  }

  if (opd.FormFlag == PTP_OPFF_Enumeration) {
    int i = 0;
    
    allowed_vals->is_range = 0;
    allowed_vals->num_entries = opd.FORM.Enum.NumberOfValues;

    switch (opd.DataType)
    {
      case PTP_DTC_INT8:
        allowed_vals->i8vals = malloc(sizeof(int8_t) * opd.FORM.Enum.NumberOfValues);
        allowed_vals->datatype = LIBMTP_DATATYPE_INT8;
        break;
      case PTP_DTC_UINT8:
        allowed_vals->u8vals = malloc(sizeof(uint8_t) * opd.FORM.Enum.NumberOfValues);
        allowed_vals->datatype = LIBMTP_DATATYPE_UINT8;
        break;
      case PTP_DTC_INT16:
        allowed_vals->i16vals = malloc(sizeof(int16_t) * opd.FORM.Enum.NumberOfValues);
        allowed_vals->datatype = LIBMTP_DATATYPE_INT16;
        break;
      case PTP_DTC_UINT16:
        allowed_vals->u16vals = malloc(sizeof(uint16_t) * opd.FORM.Enum.NumberOfValues);
        allowed_vals->datatype = LIBMTP_DATATYPE_UINT16;
        break;
      case PTP_DTC_INT32:
        allowed_vals->i32vals = malloc(sizeof(int32_t) * opd.FORM.Enum.NumberOfValues);
        allowed_vals->datatype = LIBMTP_DATATYPE_INT32;
        break;
      case PTP_DTC_UINT32:
        allowed_vals->u32vals = malloc(sizeof(uint32_t) * opd.FORM.Enum.NumberOfValues);
        allowed_vals->datatype = LIBMTP_DATATYPE_UINT32;
        break;
      case PTP_DTC_INT64:
        allowed_vals->i64vals = malloc(sizeof(int64_t) * opd.FORM.Enum.NumberOfValues);
        allowed_vals->datatype = LIBMTP_DATATYPE_INT64;
        break;
      case PTP_DTC_UINT64:
        allowed_vals->u64vals = malloc(sizeof(uint64_t) * opd.FORM.Enum.NumberOfValues);
        allowed_vals->datatype = LIBMTP_DATATYPE_UINT64;
        break;
    }
    
    for (i = 0; i < opd.FORM.Enum.NumberOfValues; i++) {
      switch (opd.DataType)
      {
        case PTP_DTC_INT8:
          allowed_vals->i8vals[i] = opd.FORM.Enum.SupportedValue[i].i8;
          break;
        case PTP_DTC_UINT8:
          allowed_vals->u8vals[i] = opd.FORM.Enum.SupportedValue[i].u8;
          break;
        case PTP_DTC_INT16:
          allowed_vals->i16vals[i] = opd.FORM.Enum.SupportedValue[i].i16;
          break;
        case PTP_DTC_UINT16:
          allowed_vals->u16vals[i] = opd.FORM.Enum.SupportedValue[i].u16;
          break;
        case PTP_DTC_INT32:
          allowed_vals->i32vals[i] = opd.FORM.Enum.SupportedValue[i].i32;
          break;
        case PTP_DTC_UINT32:
          allowed_vals->u32vals[i] = opd.FORM.Enum.SupportedValue[i].u32;
          break;
        case PTP_DTC_INT64:
          allowed_vals->i64vals[i] = opd.FORM.Enum.SupportedValue[i].i64;
          break;
        case PTP_DTC_UINT64:
          allowed_vals->u64vals[i] = opd.FORM.Enum.SupportedValue[i].u64;
          break;
      }
    }
    ptp_free_objectpropdesc(&opd);
    return 0;
  } else if (opd.FormFlag == PTP_OPFF_Range) {
    allowed_vals->is_range = 1;
    
    switch (opd.DataType)
    {
      case PTP_DTC_INT8:
        allowed_vals->i8min = opd.FORM.Range.MinimumValue.i8;
        allowed_vals->i8max = opd.FORM.Range.MaximumValue.i8;
        allowed_vals->i8step = opd.FORM.Range.StepSize.i8;
        allowed_vals->datatype = LIBMTP_DATATYPE_INT8;
        break;
      case PTP_DTC_UINT8:
        allowed_vals->u8min = opd.FORM.Range.MinimumValue.u8;
        allowed_vals->u8max = opd.FORM.Range.MaximumValue.u8;
        allowed_vals->u8step = opd.FORM.Range.StepSize.u8;
        allowed_vals->datatype = LIBMTP_DATATYPE_UINT8;
        break;
      case PTP_DTC_INT16:
        allowed_vals->i16min = opd.FORM.Range.MinimumValue.i16;
        allowed_vals->i16max = opd.FORM.Range.MaximumValue.i16;
        allowed_vals->i16step = opd.FORM.Range.StepSize.i16;
        allowed_vals->datatype = LIBMTP_DATATYPE_INT16;
        break;
      case PTP_DTC_UINT16:
        allowed_vals->u16min = opd.FORM.Range.MinimumValue.u16;
        allowed_vals->u16max = opd.FORM.Range.MaximumValue.u16;
        allowed_vals->u16step = opd.FORM.Range.StepSize.u16;
        allowed_vals->datatype = LIBMTP_DATATYPE_UINT16;
        break;
      case PTP_DTC_INT32:
        allowed_vals->i32min = opd.FORM.Range.MinimumValue.i32;
        allowed_vals->i32max = opd.FORM.Range.MaximumValue.i32;
        allowed_vals->i32step = opd.FORM.Range.StepSize.i32;
        allowed_vals->datatype = LIBMTP_DATATYPE_INT32;
        break;
      case PTP_DTC_UINT32:
        allowed_vals->u32min = opd.FORM.Range.MinimumValue.u32;
        allowed_vals->u32max = opd.FORM.Range.MaximumValue.u32;
        allowed_vals->u32step = opd.FORM.Range.StepSize.u32;
        allowed_vals->datatype = LIBMTP_DATATYPE_UINT32;
        break;
      case PTP_DTC_INT64:
        allowed_vals->i64min = opd.FORM.Range.MinimumValue.i64;
        allowed_vals->i64max = opd.FORM.Range.MaximumValue.i64;
        allowed_vals->i64step = opd.FORM.Range.StepSize.i64;
        allowed_vals->datatype = LIBMTP_DATATYPE_INT64;
        break;
      case PTP_DTC_UINT64:
        allowed_vals->u64min = opd.FORM.Range.MinimumValue.u64;
        allowed_vals->u64max = opd.FORM.Range.MaximumValue.u64;
        allowed_vals->u64step = opd.FORM.Range.StepSize.u64;
        allowed_vals->datatype = LIBMTP_DATATYPE_UINT64;
        break;
    }
    return 0; 
  } else
    return -1;
}

void LIBMTP_destroy_allowed_values_t(LIBMTP_allowed_values_t *allowed_vals)
{
  if (!allowed_vals->is_range)
  {
    switch (allowed_vals->datatype)
    {
      case LIBMTP_DATATYPE_INT8:
        if (allowed_vals->i8vals)
          free(allowed_vals->i8vals);
        break;
      case LIBMTP_DATATYPE_UINT8:
        if (allowed_vals->u8vals)
          free(allowed_vals->u8vals);
        break;
      case LIBMTP_DATATYPE_INT16:
        if (allowed_vals->i16vals)
          free(allowed_vals->i16vals);
        break;
      case LIBMTP_DATATYPE_UINT16:
        if (allowed_vals->u16vals)
          free(allowed_vals->u16vals);
        break;
      case LIBMTP_DATATYPE_INT32:
        if (allowed_vals->i32vals)
          free(allowed_vals->i32vals);
        break;
      case LIBMTP_DATATYPE_UINT32:
        if (allowed_vals->u32vals)
          free(allowed_vals->u32vals);
        break;
      case LIBMTP_DATATYPE_INT64:
        if (allowed_vals->i64vals)
          free(allowed_vals->i64vals);
        break;
      case LIBMTP_DATATYPE_UINT64:
        if (allowed_vals->u64vals)
          free(allowed_vals->u64vals);
        break;
    }
  }
}

int LIBMTP_Is_Property_Supported(LIBMTP_mtpdevice_t *device, LIBMTP_property_t const property,
            LIBMTP_filetype_t const filetype)
{
  uint16_t *props = NULL;
  uint32_t propcnt = 0;
  uint16_t ret = 0;
  int i = 0;
  int supported = 0;
  uint16_t ptp_prop = map_libmtp_property_to_ptp_property(property);
  
  ret = ptp_mtp_getobjectpropssupported(device->params, map_libmtp_type_to_ptp_type(filetype), &propcnt, &props);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Is_Property_Supported(): could not get properties supported.");
    return -1;
  }

	for (i = 0; i < propcnt; i++) {
    if (props[i] == ptp_prop) {
      supported = 1;
      break;
    }
  }
  
  free(props);
  
  return supported;
}

char *LIBMTP_Get_String_From_Object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    LIBMTP_property_t const attribute_id)
{
  return get_string_from_object(device, object_id, attribute_id);
}

uint64_t LIBMTP_Get_u64_From_Object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
                                    LIBMTP_property_t const attribute_id, uint64_t const value_default)
{
  return get_u64_from_object(device, object_id, map_libmtp_property_to_ptp_property(attribute_id), value_default);
}

uint32_t LIBMTP_Get_u32_From_Object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
				    LIBMTP_property_t const attribute_id, uint32_t const value_default)
{
  return get_u32_from_object(device, object_id, map_libmtp_property_to_ptp_property(attribute_id), value_default);
}

uint16_t LIBMTP_Get_u16_From_Object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    LIBMTP_property_t const attribute_id, uint16_t const value_default)
{
  return get_u16_from_object(device, object_id, map_libmtp_property_to_ptp_property(attribute_id), value_default);
}

uint8_t LIBMTP_Get_u8_From_Object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				  LIBMTP_property_t const attribute_id, uint8_t const value_default)
{
  return get_u8_from_object(device, object_id, map_libmtp_property_to_ptp_property(attribute_id), value_default);
}

int LIBMTP_Set_Object_String(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			     LIBMTP_property_t const attribute_id, char const * const string)
{
  return set_object_string(device, object_id, map_libmtp_property_to_ptp_property(attribute_id), string);
}


int LIBMTP_Set_Object_u32(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  LIBMTP_property_t const attribute_id, uint32_t const value)
{
  return set_object_u32(device, object_id, map_libmtp_property_to_ptp_property(attribute_id), value);
}

int LIBMTP_Set_Object_u16(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  LIBMTP_property_t const attribute_id, uint16_t const value)
{
  return set_object_u16(device, object_id, map_libmtp_property_to_ptp_property(attribute_id), value);
}

int LIBMTP_Set_Object_u8(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			 LIBMTP_property_t const attribute_id, uint8_t const value)
{
  return set_object_u8(device, object_id, map_libmtp_property_to_ptp_property(attribute_id), value);
}

static char *get_string_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    uint16_t const attribute_id)
{
  PTPPropertyValue propval;
  char *retstring = NULL;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;
  MTPProperties *prop;

  if ( device == NULL || object_id == 0) {
    return NULL;
  }
  
  prop = ptp_find_object_prop_in_cache(params, object_id, attribute_id);
  if (prop) {
    if (prop->propval.str != NULL)
      return strdup(prop->propval.str);
    else
      return NULL;
  }

  ret = ptp_mtp_getobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_STR);
  if (ret == PTP_RC_OK) {
    if (propval.str != NULL) {
      retstring = (char *) strdup(propval.str);
      free(propval.str);
    }
  } else {
    add_ptp_error_to_errorstack(device, ret, "get_string_from_object(): could not get object string.");
  }

  return retstring;
}

static uint64_t get_u64_from_object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
                                    uint16_t const attribute_id, uint64_t const value_default)
{
  PTPPropertyValue propval;
  uint64_t retval = value_default;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;
  MTPProperties *prop;
  
  if ( device == NULL ) {
    return value_default;
  }
  
  prop = ptp_find_object_prop_in_cache(params, object_id, attribute_id);
  if (prop)
    return prop->propval.u64;

  ret = ptp_mtp_getobjectpropvalue(params, object_id,
                                   attribute_id,
                                   &propval,
                                   PTP_DTC_UINT64);
  if (ret == PTP_RC_OK) {
    retval = propval.u64;
  } else {
    add_ptp_error_to_errorstack(device, ret, "get_u64_from_object(): could not get unsigned 64bit integer from object.");
  }
  
  return retval;
}

static uint32_t get_u32_from_object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
				    uint16_t const attribute_id, uint32_t const value_default)
{
  PTPPropertyValue propval;
  uint32_t retval = value_default;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;
  MTPProperties *prop;

  if ( device == NULL ) {
    return value_default;
  }

  prop = ptp_find_object_prop_in_cache(params, object_id, attribute_id);
  if (prop)
    return prop->propval.u32;

  ret = ptp_mtp_getobjectpropvalue(params, object_id,
                                   attribute_id,
                                   &propval,
                                   PTP_DTC_UINT32);
  if (ret == PTP_RC_OK) {
    retval = propval.u32;
  } else {
    add_ptp_error_to_errorstack(device, ret, "get_u32_from_object(): could not get unsigned 32bit integer from object.");
  }
  return retval;
}

static uint16_t get_u16_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    uint16_t const attribute_id, uint16_t const value_default)
{
  PTPPropertyValue propval;
  uint16_t retval = value_default;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;
  MTPProperties *prop;

  if ( device == NULL ) {
    return value_default;
  }

  
  
  prop = ptp_find_object_prop_in_cache(params, object_id, attribute_id);
  if (prop)
    return prop->propval.u16;

  ret = ptp_mtp_getobjectpropvalue(params, object_id,
                                   attribute_id,
                                   &propval,
                                   PTP_DTC_UINT16);
  if (ret == PTP_RC_OK) {
    retval = propval.u16;
  } else {
    add_ptp_error_to_errorstack(device, ret, "get_u16_from_object(): could not get unsigned 16bit integer from object.");
  }

  return retval;
}

static uint8_t get_u8_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				  uint16_t const attribute_id, uint8_t const value_default)
{
  PTPPropertyValue propval;
  uint8_t retval = value_default;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;
  MTPProperties *prop;

  if ( device == NULL ) {
    return value_default;
  }

  
  
  prop = ptp_find_object_prop_in_cache(params, object_id, attribute_id);
  if (prop)
    return prop->propval.u8;

  ret = ptp_mtp_getobjectpropvalue(params, object_id,
                                   attribute_id,
                                   &propval,
                                   PTP_DTC_UINT8);
  if (ret == PTP_RC_OK) {
    retval = propval.u8;
  } else {
    add_ptp_error_to_errorstack(device, ret, "get_u8_from_object(): could not get unsigned 8bit integer from object.");
  }

  return retval;
}

static int set_object_string(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			     uint16_t const attribute_id, char const * const string)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL || string == NULL) {
    return -1;
  }
  
  if (!ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_string(): could not set object string: "
				"PTP_OC_MTP_SetObjectPropValue not supported.");
    return -1;
  }
  propval.str = (char *) string;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "set_object_string(): could not set object string.");
    return -1;
  }

  return 0;
}


static int set_object_u32(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  uint16_t const attribute_id, uint32_t const value)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL) {
    return -1;
  }

  if (!ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_u32(): could not set unsigned 32bit integer property: "
				"PTP_OC_MTP_SetObjectPropValue not supported.");
    return -1;
  }
  
  propval.u32 = value;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_UINT32);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "set_object_u32(): could not set unsigned 32bit integer property.");
    return -1;
  }

  return 0;
}

static int set_object_u16(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  uint16_t const attribute_id, uint16_t const value)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL) {
    return 1;
  }

  if (!ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_u16(): could not set unsigned 16bit integer property: "
				"PTP_OC_MTP_SetObjectPropValue not supported.");
    return -1;
  }
  propval.u16 = value;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_UINT16);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "set_object_u16(): could not set unsigned 16bit integer property.");
    return 1;
  }

  return 0;
}

static int set_object_u8(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			 uint16_t const attribute_id, uint8_t const value)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL) {
    return 1;
  }

  if (!ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_u8(): could not set unsigned 8bit integer property: "
			    "PTP_OC_MTP_SetObjectPropValue not supported.");
    return -1;
  }
  propval.u8 = value;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_UINT8);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "set_object_u8(): could not set unsigned 8bit integer property.");
    return 1;
  }

  return 0;
}

LIBMTP_mtpdevice_t *LIBMTP_Get_First_Device(void)
{
  LIBMTP_mtpdevice_t *first_device = NULL;
  LIBMTP_raw_device_t *devices;
  int numdevs;
  LIBMTP_error_number_t ret;
  
  ret = LIBMTP_Detect_Raw_Devices(&devices, &numdevs);
  if (ret != LIBMTP_ERROR_NONE) {
    return NULL;
  }

  if (devices == NULL || numdevs == 0) {
    return NULL;
  }

  first_device = LIBMTP_Open_Raw_Device(&devices[0]);
  free(devices);
  return first_device;
}

static void
#ifdef __GNUC__
__attribute__((__format__(printf,2,0)))
#endif
LIBMTP_ptp_debug(void *data, const char *format, va_list args)
{
#ifdef ENABLE_PTP_DEBUG
  vfprintf (stderr, format, args);
  fflush (stderr);
#endif
}

static void
#ifdef __GNUC__
__attribute__((__format__(printf,2,0)))
#endif
LIBMTP_ptp_error(void *data, const char *format, va_list args)
{
  
    vfprintf (stderr, format, args);
    fflush (stderr);
}

LIBMTP_mtpdevice_t *LIBMTP_Open_Raw_Device(LIBMTP_raw_device_t *rawdevice)
{
  LIBMTP_mtpdevice_t *mtp_device;
  uint8_t bs = 0;
  PTPParams *current_params;
  PTP_USB *ptp_usb;
  LIBMTP_error_number_t err;
  int i;

  
  mtp_device = (LIBMTP_mtpdevice_t *) malloc(sizeof(LIBMTP_mtpdevice_t));
  memset(mtp_device, 0, sizeof(LIBMTP_mtpdevice_t));
  
  if(mtp_device == NULL) {
    
    
    fprintf(stderr, "LIBMTP PANIC: connect_usb_devices encountered a memory "
	    "allocation error with device %d on bus %d, trying to continue",
	    rawdevice->devnum, rawdevice->bus_location);
    
    return NULL;
  }

  
  current_params = (PTPParams *) malloc(sizeof(PTPParams));
  if (current_params == NULL) {
    free(mtp_device);
    return NULL;
  }
  memset(current_params, 0, sizeof(PTPParams));
  current_params->device_flags = rawdevice->device_entry.device_flags;
  current_params->nrofobjects = 0;
  current_params->objects = NULL;
  current_params->response_packet_size = 0;
  current_params->response_packet = NULL;
  
  current_params->data = NULL;
  
  current_params->debug_func = LIBMTP_ptp_debug;
  current_params->error_func = LIBMTP_ptp_error;
  
  current_params->byteorder = PTP_DL_LE;
  current_params->cd_locale_to_ucs2 = iconv_open("UCS-2LE", "UTF-8");
  current_params->cd_ucs2_to_locale = iconv_open("UTF-8", "UCS-2LE");
    
  if(current_params->cd_locale_to_ucs2 == (iconv_t) -1 ||
     current_params->cd_ucs2_to_locale == (iconv_t) -1) {
    fprintf(stderr, "LIBMTP PANIC: Cannot open iconv() converters to/from UCS-2!\n"
	    "Too old stdlibc, glibc and libiconv?\n");
    free(current_params);
    free(mtp_device);
    return NULL;
  }
  mtp_device->params = current_params;

  
  
  err = configure_usb_device(rawdevice,
			     current_params,
			     &mtp_device->usbinfo);
  if (err != LIBMTP_ERROR_NONE) {
    free(current_params);
    free(mtp_device);
    return NULL;
  }
  ptp_usb = (PTP_USB*) mtp_device->usbinfo;
  
  ptp_usb->params = current_params;

  
  
  if (ptp_getdeviceinfo(current_params,
			&current_params->deviceinfo) != PTP_RC_OK) {
    fprintf(stderr, "LIBMTP PANIC: Unable to read device information on device "
	    "%d on bus %d, trying to continue",
	    rawdevice->devnum, rawdevice->bus_location);
    
    
    free(mtp_device->usbinfo);
    free(mtp_device->params);
    current_params = NULL;
    free(mtp_device);    
    return NULL;
  }
  
  
  for (i=0;i<current_params->deviceinfo.ImageFormats_len;i++) {
    PTPObjectPropDesc opd;
    
    if (ptp_mtp_getobjectpropdesc(current_params, 
				  PTP_OPC_ObjectSize, 
				  current_params->deviceinfo.ImageFormats[i], 
				  &opd) != PTP_RC_OK) {
      printf("LIBMTP PANIC: "
	     "could not inspect object property descriptions!\n");
    } else {
      if (opd.DataType == PTP_DTC_UINT32) {
	if (bs == 0) {
	  bs = 32;
	} else if (bs != 32) {
	  printf("LIBMTP PANIC: "
		 "different objects support different object sizes!\n");
	  bs = 0;
	  break;
	}
      } else if (opd.DataType == PTP_DTC_UINT64) {
	if (bs == 0) {
	  bs = 64;
	} else if (bs != 64) {
	  printf("LIBMTP PANIC: "
		 "different objects support different object sizes!\n");
	  bs = 0;
	  break;
	}
      } else {
	
	printf("LIBMTP PANIC: "
	       "awkward object size data type: %04x\n", opd.DataType);
	bs = 0;
	break;
      }
    }
  }
  if (bs == 0) {
    
    bs = 32;
  }
  mtp_device->object_bitsize = bs;
  
  
  mtp_device->errorstack = NULL;
  
  
  mtp_device->maximum_battery_level = 100;
  
  
  if(!FLAG_BROKEN_BATTERY_LEVEL(ptp_usb) && 
     ptp_property_issupported( current_params, PTP_DPC_BatteryLevel)) {
    PTPDevicePropDesc dpd;
    
    
    if(ptp_getdevicepropdesc(current_params,
			     PTP_DPC_BatteryLevel,
			     &dpd) != PTP_RC_OK) {
      add_error_to_errorstack(mtp_device,
			      LIBMTP_ERROR_CONNECTING,
			      "Unable to read Maximum Battery Level for this "
			      "device even though the device supposedly "
			      "supports this functionality");
    }
    
    
    
    if (dpd.FORM.Range.MaximumValue.u8 != 0) {
      mtp_device->maximum_battery_level = dpd.FORM.Range.MaximumValue.u8;
    }
    
    ptp_free_devicepropdesc(&dpd);
  }
  
  
  mtp_device->default_music_folder = 0;
  mtp_device->default_playlist_folder = 0;
  mtp_device->default_picture_folder = 0;
  mtp_device->default_video_folder = 0;
  mtp_device->default_organizer_folder = 0;
  mtp_device->default_zencast_folder = 0;
  mtp_device->default_album_folder = 0;
  mtp_device->default_text_folder = 0;
  
  
  mtp_device->storage = NULL;
  if (LIBMTP_Get_Storage(mtp_device, LIBMTP_STORAGE_SORTBY_NOTSORTED) == -1) {
    add_error_to_errorstack(mtp_device,
			    LIBMTP_ERROR_GENERAL,
			    "Get Storage information failed.");
    mtp_device->storage = NULL;
  }
  
  flush_handles(mtp_device);

  return mtp_device;
}

static LIBMTP_mtpdevice_t * create_usb_mtp_devices(LIBMTP_raw_device_t *devices, int numdevs)
{
  uint8_t i;
  LIBMTP_mtpdevice_t *mtp_device_list = NULL;
  LIBMTP_mtpdevice_t *current_device = NULL;

  for (i=0; i < numdevs; i++) {
    LIBMTP_mtpdevice_t *mtp_device;
    mtp_device = LIBMTP_Open_Raw_Device(&devices[i]);

    
    if (mtp_device == NULL)
      continue;

    
    mtp_device->next = NULL;
    if (mtp_device_list == NULL) {
      mtp_device_list = current_device = mtp_device;
    } else {
      current_device->next = mtp_device;
      current_device = mtp_device;
    }
  }
  return mtp_device_list;
}

 
uint32_t LIBMTP_Number_Devices_In_List(LIBMTP_mtpdevice_t *device_list)
{
  uint32_t numdevices = 0;
  LIBMTP_mtpdevice_t *iter;
  for(iter = device_list; iter != NULL; iter = iter->next)
    numdevices++;
  
  return numdevices;
}

LIBMTP_error_number_t LIBMTP_Get_Connected_Devices(LIBMTP_mtpdevice_t **device_list)
{
  LIBMTP_raw_device_t *devices;
  int numdevs;
  LIBMTP_error_number_t ret;
  
  ret = LIBMTP_Detect_Raw_Devices(&devices, &numdevs);
  if (ret != LIBMTP_ERROR_NONE) {
    *device_list = NULL;
    return ret;
  }

  
  if (devices == NULL || numdevs == 0) {
    *device_list = NULL;
    return LIBMTP_ERROR_NO_DEVICE_ATTACHED;
  }

  *device_list = create_usb_mtp_devices(devices, numdevs);
  free(devices);

  
  
  
  if (*device_list == NULL)
    return LIBMTP_ERROR_CONNECTING;

  return LIBMTP_ERROR_NONE;
}

void LIBMTP_Release_Device_List(LIBMTP_mtpdevice_t *device)
{
  if(device != NULL)
  {
    if(device->next != NULL)
    {
      LIBMTP_Release_Device_List(device->next);
    }
    
    LIBMTP_Release_Device(device);
  }
}

void LIBMTP_Release_Device(LIBMTP_mtpdevice_t *device)
{
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  close_device(ptp_usb, params);
  
  LIBMTP_Clear_Errorstack(device);
  
  iconv_close(params->cd_locale_to_ucs2);
  iconv_close(params->cd_ucs2_to_locale);
  free(ptp_usb);  
  ptp_free_params(params);
  free_storage_list(device);
  free(device);
}

static void add_error_to_errorstack(LIBMTP_mtpdevice_t *device,
				    LIBMTP_error_number_t errornumber,
				    char const * const error_text)
{
  LIBMTP_error_t *newerror;
  
  if (device == NULL) {
    fprintf(stderr, "LIBMTP PANIC: Trying to add error to a NULL device!\n");
    return;
  }
  newerror = (LIBMTP_error_t *) malloc(sizeof(LIBMTP_error_t));
  newerror->errornumber = errornumber;
  newerror->error_text = strdup(error_text);
  newerror->next = NULL;
  if (device->errorstack == NULL) {
    device->errorstack = newerror;
  } else {
    LIBMTP_error_t *tmp = device->errorstack;
    
    while (tmp->next != NULL) {
      tmp = tmp->next;
    }
    tmp->next = newerror;
  }
}

static void add_ptp_error_to_errorstack(LIBMTP_mtpdevice_t *device,
					uint16_t ptp_error,
					char const * const error_text)
{
  if (device == NULL) {
    fprintf(stderr, "LIBMTP PANIC: Trying to add PTP error to a NULL device!\n");
    return;
  } else {
    char outstr[256];
    snprintf(outstr, sizeof(outstr), "PTP Layer error %04x: %s", ptp_error, error_text);
    outstr[sizeof(outstr)-1] = '\0';
    add_error_to_errorstack(device, LIBMTP_ERROR_PTP_LAYER, outstr);
    add_error_to_errorstack(device, LIBMTP_ERROR_PTP_LAYER, "(Look this up in ptp.h for an explanation.)");
  }
}

LIBMTP_error_t *LIBMTP_Get_Errorstack(LIBMTP_mtpdevice_t *device)
{
  if (device == NULL) {
    fprintf(stderr, "LIBMTP PANIC: Trying to get the error stack of a NULL device!\n");
    return NULL;
  }
  return device->errorstack;
}

void LIBMTP_Clear_Errorstack(LIBMTP_mtpdevice_t *device)
{
  if (device == NULL) {
    fprintf(stderr, "LIBMTP PANIC: Trying to clear the error stack of a NULL device!\n");
  } else {
    LIBMTP_error_t *tmp = device->errorstack;
  
    while (tmp != NULL) {
      LIBMTP_error_t *tmp2;
      
      if (tmp->error_text != NULL) {
	free(tmp->error_text);
      }
      tmp2 = tmp;
      tmp = tmp->next;
      free(tmp2);
    }
    device->errorstack = NULL;
  }
}

void LIBMTP_Dump_Errorstack(LIBMTP_mtpdevice_t *device)
{
  if (device == NULL) {
    fprintf(stderr, "LIBMTP PANIC: Trying to dump the error stack of a NULL device!\n");
  } else {
    LIBMTP_error_t *tmp = device->errorstack;

    while (tmp != NULL) {
      if (tmp->error_text != NULL) {
	fprintf(stderr, "Error %d: %s\n", tmp->errornumber, tmp->error_text);
      } else {
	fprintf(stderr, "Error %d: (unknown)\n", tmp->errornumber);
      }
      tmp = tmp->next;
    }
  }
}

void LIBMTP_Set_Device_Timeout(LIBMTP_mtpdevice_t *device, int milliseconds)
{
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  set_usb_device_timeout(ptp_usb, milliseconds);
}

void LIBMTP_Get_Device_Timeout(LIBMTP_mtpdevice_t *device, int * milliseconds)
{
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  get_usb_device_timeout(ptp_usb, milliseconds);
}

static int get_all_metadata_fast(LIBMTP_mtpdevice_t *device,
				 uint32_t storage)
{
  PTPParams      *params = (PTPParams *) device->params;
  int		 cnt = 0;
  int            i, j, nrofprops;
  uint32_t	 lasthandle = 0xffffffff;
  MTPProperties  *props = NULL;
  MTPProperties  *prop;
  uint16_t       ret;
  int            oldtimeout;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  get_usb_device_timeout(ptp_usb, &oldtimeout);
  set_usb_device_timeout(ptp_usb, 60000);
  
  ret = ptp_mtp_getobjectproplist(params, 0xffffffff, &props, &nrofprops);
  set_usb_device_timeout(ptp_usb, oldtimeout);

  if (ret == PTP_RC_MTP_Specification_By_Group_Unsupported) {
    
    
    
    add_ptp_error_to_errorstack(device, ret, "get_all_metadata_fast(): "
    "cannot retrieve all metadata for an object on this device.");
    return -1;
  }
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "get_all_metadata_fast(): "
    "could not get proplist of all objects.");
    return -1;
  }
  if (props == NULL && nrofprops != 0) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL,
			    "get_all_metadata_fast(): "
			    "call to ptp_mtp_getobjectproplist() returned "
			    "inconsistent results.");
    return -1;
  }
  prop = props;
  for (i=0;i<nrofprops;i++) {
      if (lasthandle != prop->ObjectHandle) {
	cnt++;
	lasthandle = prop->ObjectHandle;
      }
      prop++;
  }
  lasthandle = 0xffffffff;
  params->objects = calloc (sizeof(PTPObject),cnt);
  prop = props;
  i = -1;
  for (j=0;j<nrofprops;j++) {
    if (lasthandle != prop->ObjectHandle) {
      if (i >= 0) {
        params->objects[i].flags |= PTPOBJECT_OBJECTINFO_LOADED;
	if (!params->objects[i].oi.Filename) {
	  
	  params->objects[i].oi.Filename = strdup("<null>");
	}
      }
      i++;
      lasthandle = prop->ObjectHandle;
      params->objects[i].oid = prop->ObjectHandle;
    }
    switch (prop->property) {
    case PTP_OPC_ParentObject:
      params->objects[i].oi.ParentObject = prop->propval.u32;
      params->objects[i].flags |= PTPOBJECT_PARENTOBJECT_LOADED;
      break;
    case PTP_OPC_ObjectFormat:
      params->objects[i].oi.ObjectFormat = prop->propval.u16;
      break;
    case PTP_OPC_ObjectSize:
      
      
      
      if (device->object_bitsize == 64) {
	params->objects[i].oi.ObjectCompressedSize = (uint32_t) prop->propval.u64;
      } else {
	params->objects[i].oi.ObjectCompressedSize = prop->propval.u32;
      }
      break;
    case PTP_OPC_StorageID:
      params->objects[i].oi.StorageID = prop->propval.u32;
      params->objects[i].flags |= PTPOBJECT_STORAGEID_LOADED;
      break;
    case PTP_OPC_ObjectFileName:
      if (prop->propval.str != NULL)
        params->objects[i].oi.Filename = strdup(prop->propval.str);
      break;
    default: {
      MTPProperties *newprops;

      
      if (params->objects[i].nrofmtpprops) {
        newprops = realloc(params->objects[i].mtpprops,(params->objects[i].nrofmtpprops+1)*sizeof(MTPProperties));
      } else {
        newprops = calloc(sizeof(MTPProperties),1);
      }
      if (!newprops) return 0; 
      params->objects[i].mtpprops = newprops;
      memcpy(&params->objects[i].mtpprops[params->objects[i].nrofmtpprops],&props[j],sizeof(props[j]));
      params->objects[i].nrofmtpprops++;
      params->objects[i].flags |= PTPOBJECT_MTPPROPLIST_LOADED;
      break;
    }
    }
    prop++;
  }
  
  params->objects[i].flags |= PTPOBJECT_OBJECTINFO_LOADED;
  params->nrofobjects = i+1;
  
  ptp_objects_sort (params);
  return 0;
}

static void get_handles_recursively(LIBMTP_mtpdevice_t *device, 
				    PTPParams *params, 
				    uint32_t storageid,
				    uint32_t parent)
{
  PTPObjectHandles currentHandles;
  int i = 0;
  uint16_t ret = ptp_getobjecthandles(params,
                                      storageid,
                                      PTP_GOH_ALL_FORMATS,
                                      parent,
                                      &currentHandles);
  
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "get_handles_recursively(): could not get object handles.");
    return;
  }
  
  if (currentHandles.Handler == NULL || currentHandles.n == 0)
    return;

  
  for (i = 0; i < currentHandles.n; i++) {    
    PTPObject *ob;
    ret = ptp_object_want(params,currentHandles.Handler[i],PTPOBJECT_OBJECTINFO_LOADED, &ob);
    if (ret == PTP_RC_OK) {
      if (ob->oi.ObjectFormat == PTP_OFC_Association)
        get_handles_recursively(device, params, storageid, currentHandles.Handler[i]);
    } else {
      add_error_to_errorstack(device,
			      LIBMTP_ERROR_CONNECTING,
			      "Found a bad handle, trying to ignore it.");
    }
  }
  free(currentHandles.Handler);
}


LIBMTP_file_t * obj2file(LIBMTP_mtpdevice_t *device, PTPObject *ob)
{
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  PTPParams *params = (PTPParams *) device->params;
  LIBMTP_file_t *file;
  PTPObject *xob;
  uint16_t ret;


  if (ob->oi.Filename == NULL)
    ob->oi.Filename = strdup("<null>");

  if (ob->oi.Keywords == NULL)
    ob->oi.Keywords = strdup("<null>");

  
  file = LIBMTP_new_file_t();

  file->parent_id = ob->oi.ParentObject;
  file->storage_id = ob->oi.StorageID;

  
  file->item_id = ob->oid;

    
  file->filetype = map_ptp_type_to_libmtp_type(ob->oi.ObjectFormat);

  
  file->modificationdate = ob->oi.ModificationDate;

  
  
  
  file->filesize = ob->oi.ObjectCompressedSize;
  if (ob->oi.Filename != NULL) {
      file->filename = strdup(ob->oi.Filename);
  }

  if (file->filetype == LIBMTP_FILETYPE_UNKNOWN) {
    if ((FLAG_IRIVER_OGG_ALZHEIMER(ptp_usb) ||
        FLAG_OGG_IS_UNKNOWN(ptp_usb)) &&
        has_ogg_extension(file->filename))

      file->filetype = LIBMTP_FILETYPE_OGG;

      if (FLAG_FLAC_IS_UNKNOWN(ptp_usb) && has_flac_extension(file->filename))
        file->filetype = LIBMTP_FILETYPE_FLAC;
    }

  ret = ptp_object_want (params, ob->oid, PTPOBJECT_MTPPROPLIST_LOADED, &xob);
  if (ob->mtpprops) {
    MTPProperties *prop = ob->mtpprops;
    int i;

    for (i=0;i<ob->nrofmtpprops;i++) {
      
      if (prop->property == PTP_OPC_ObjectSize) {
        if (device->object_bitsize == 64) {
          file->filesize = prop->propval.u64;
        } else {
          file->filesize = prop->propval.u32;
        }
        break;
      }
      prop++;
    }
  } else {
    uint16_t *props = NULL;
    uint32_t propcnt = 0;

    
    ret = ptp_mtp_getobjectpropssupported(params, ob->oi.ObjectFormat, &propcnt, &props);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "obj2file(): call to ptp_mtp_getobjectpropssupported() failed.");
      
    } else {
      int i;
      for (i=0;i<propcnt;i++) {

      switch (props[i]) {
        case PTP_OPC_ObjectSize:
          if (device->object_bitsize == 64) {
            file->filesize = get_u64_from_object(device, file->item_id, PTP_OPC_ObjectSize, 0);
          } else {
            file->filesize = get_u32_from_object(device, file->item_id, PTP_OPC_ObjectSize, 0);
          }
          break;
        default:
            break;
        }
      }
      free(props);
    }
  }

  return file;
}



static LIBMTP_file_t * get_files(LIBMTP_mtpdevice_t *device,
                        PTPParams *params,
                        uint32_t storageid,
                        uint32_t parentId
                        )
{
  int i = 0;
  LIBMTP_file_t *curfile = NULL;
  LIBMTP_file_t *retfiles = NULL;
  PTPObjectHandles currentHandles;

  uint16_t ret = ptp_getobjecthandles(params,
                                      storageid,
                                      PTP_GOH_ALL_FORMATS,
                                      parentId,
                                      &currentHandles);

  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "get_files(): could not get object handles.");
    return NULL;
  }

  if (currentHandles.Handler == NULL || currentHandles.n == 0)
    return NULL;

  for (i = 0; i < currentHandles.n; i++) {
    PTPObject *ob;
    LIBMTP_file_t *file;

    ret = ptp_object_want(params, currentHandles.Handler[i],PTPOBJECT_OBJECTINFO_LOADED, &ob);
    if (ret != PTP_RC_OK)
      return NULL;

    file = obj2file(device, ob);

    if (file == NULL)
      continue;

    
    if (curfile == NULL) {
      curfile = file;
      retfiles = file;
    } else {
      curfile->next = file;
      curfile = file;
    }
 }

  free(currentHandles.Handler);

  
  
  return retfiles;
}

void LIBMTP_Set_Load_Cache_On_Demand(int flag)
{
    load_cache_on_demand = flag;
}

LIBMTP_file_t * LIBMTP_Get_Files_And_Folders(LIBMTP_mtpdevice_t *device, uint32_t storageId, uint32_t parentId)
{
  LIBMTP_file_t *retfiles = NULL;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  int ret;
  uint32_t i;

#if 0
  
  if (ptp_operation_issupported(params,PTP_OC_MTP_GetObjPropList)
      && !FLAG_BROKEN_MTPGETOBJPROPLIST(ptp_usb)
      && !FLAG_BROKEN_MTPGETOBJPROPLIST_ALL(ptp_usb)) {
    
    ret = get_all_metadata_fast(device, PTP_GOH_ALL_STORAGE);
  }
#endif


  retfiles = get_files(device, params, storageId, parentId);

  return retfiles;
}

static void flush_handles(LIBMTP_mtpdevice_t *device)
{
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  int ret;
  uint32_t i;

  if (load_cache_on_demand) {
    return;
  }

  if (params->objects != NULL) {
    for (i=0;i<params->nrofobjects;i++)
      ptp_free_object (&params->objects[i]);
    free(params->objects);
    params->objects = NULL;
    params->nrofobjects = 0;
  }

  if (ptp_operation_issupported(params,PTP_OC_MTP_GetObjPropList)
      && !FLAG_BROKEN_MTPGETOBJPROPLIST(ptp_usb)
      && !FLAG_BROKEN_MTPGETOBJPROPLIST_ALL(ptp_usb)) {
    
    ret = get_all_metadata_fast(device, PTP_GOH_ALL_STORAGE);
  }

  
  
  if (params->nrofobjects == 0) {
    
    if (device->storage == NULL) {
      get_handles_recursively(device, params,
			      PTP_GOH_ALL_STORAGE,
			      PTP_GOH_ROOT_PARENT);
    } else {
      
      LIBMTP_devicestorage_t *storage = device->storage;
      while(storage != NULL) {
	get_handles_recursively(device, params,
				storage->id,
				PTP_GOH_ROOT_PARENT);
	storage = storage->next;
      }
    }
  }
  
  for(i = 0; i < params->nrofobjects; i++) {
    PTPObject *ob, *xob;
 
    ob = &params->objects[i];
    ret = ptp_object_want(params,params->objects[i].oid,PTPOBJECT_OBJECTINFO_LOADED, &xob);
    if (ret != PTP_RC_OK) {
	fprintf(stderr,"broken! %x not found\n", params->objects[i].oid);
    }
    if (ob->oi.Filename == NULL)
      ob->oi.Filename = strdup("<null>");
    if (ob->oi.Keywords == NULL)
      ob->oi.Keywords = strdup("<null>");

    
    if(ob->oi.ObjectFormat != PTP_OFC_Association)
      continue;
    
    if (ob->oi.ParentObject != 0x00000000U)
      continue;
    
    if (device->storage != NULL && ob->oi.StorageID != device->storage->id)
      continue;

    
    
    if (!strcasecmp(ob->oi.Filename, "My Music") ||
	!strcasecmp(ob->oi.Filename, "Music")) {
      device->default_music_folder = ob->oid;
    }
    else if (!strcasecmp(ob->oi.Filename, "My Playlists") ||
	     !strcasecmp(ob->oi.Filename, "Playlists")) {
      device->default_playlist_folder = ob->oid;
    }
    else if (!strcasecmp(ob->oi.Filename, "My Pictures") ||
	     !strcasecmp(ob->oi.Filename, "Pictures")) {
      device->default_picture_folder = ob->oid;
    }
    else if (!strcasecmp(ob->oi.Filename, "My Video") ||
	     !strcasecmp(ob->oi.Filename, "Video")) {
	device->default_video_folder = ob->oid;
    }
    else if (!strcasecmp(ob->oi.Filename, "My Organizer")) {
      device->default_organizer_folder = ob->oid;
    }
    else if (!strcasecmp(ob->oi.Filename, "ZENcast") ||
	     !strcasecmp(ob->oi.Filename, "Datacasts")) {
      device->default_zencast_folder = ob->oid;
    }
    else if (!strcasecmp(ob->oi.Filename, "My Albums") ||
	     !strcasecmp(ob->oi.Filename, "Albums")) {
      device->default_album_folder = ob->oid;
    }
    else if (!strcasecmp(ob->oi.Filename, "Text") ||
	     !strcasecmp(ob->oi.Filename, "Texts")) {
      device->default_text_folder = ob->oid;
    }
  }
}

static void free_storage_list(LIBMTP_mtpdevice_t *device)
{
  LIBMTP_devicestorage_t *storage;
  LIBMTP_devicestorage_t *tmp;

  storage = device->storage;
  while(storage != NULL) {  
    if (storage->StorageDescription != NULL) {
      free(storage->StorageDescription);
    }
    if (storage->VolumeIdentifier != NULL) {
      free(storage->VolumeIdentifier);
    }
    tmp = storage;
    storage = storage->next;
    free(tmp);
  }
  device->storage = NULL;

  return;
}

static int sort_storage_by(LIBMTP_mtpdevice_t *device,int const sortby)
{
  LIBMTP_devicestorage_t *oldhead, *ptr1, *ptr2, *newlist;

  if (device->storage == NULL)
    return -1;
  if (sortby == LIBMTP_STORAGE_SORTBY_NOTSORTED) 
    return 0;

  oldhead = ptr1 = ptr2 = device->storage;

  newlist = NULL;

  while(oldhead != NULL) {
    ptr1 = ptr2 = oldhead;
    while(ptr1 != NULL) {

      if (sortby == LIBMTP_STORAGE_SORTBY_FREESPACE && ptr1->FreeSpaceInBytes > ptr2->FreeSpaceInBytes) 
        ptr2 = ptr1;
      if (sortby == LIBMTP_STORAGE_SORTBY_MAXSPACE && ptr1->FreeSpaceInBytes > ptr2->FreeSpaceInBytes) 
        ptr2 = ptr1;

      ptr1 = ptr1->next;
    }

    
    if(ptr2->prev != NULL) {
      ptr1 = ptr2->prev;
      ptr1->next = ptr2->next; 
    } else {
      oldhead = ptr2->next;
      if(oldhead != NULL)
        oldhead->prev = NULL;
    }

    
    ptr1 = ptr2->next;
    if(ptr1 != NULL) {
      ptr1->prev = ptr2->prev;
    } else {
      ptr1 = ptr2->prev;
      if(ptr1 != NULL)
        ptr1->next = NULL;
    }
  
    if(newlist == NULL) {
      newlist = ptr2;
      newlist->prev = NULL;
    } else {
      ptr2->prev = newlist;
      newlist->next = ptr2;
      newlist = newlist->next;
    }
  }
 
  if (newlist != NULL) {
    newlist->next = NULL;
    while(newlist->prev != NULL) 
      newlist = newlist->prev;
    device->storage = newlist;
  }

  return 0;
}

static uint32_t get_writeable_storageid(LIBMTP_mtpdevice_t *device, uint64_t fitsize)
{
  LIBMTP_devicestorage_t *storage;
  uint32_t store = 0x00000000; 
  int subcall_ret;

  
  storage = device->storage;
  if (storage == NULL) {
    
    store = 0x00000000U;
  } else {
    while(storage != NULL) {
      
      if (storage->StorageType == PTP_ST_FixedROM || storage->StorageType == PTP_ST_RemovableROM) {
	storage = storage->next;
	continue;
      }
      
      
      if ((storage->id & 0x0000FFFFU) == 0x00000000U) {
	storage = storage->next;
	continue;
      }
      
      if (storage->AccessCapability == PTP_AC_ReadOnly || storage->AccessCapability == PTP_AC_ReadOnly_with_Object_Deletion) {
	storage = storage->next;
	continue;
      }
      
      subcall_ret = check_if_file_fits(device, storage, fitsize);
      if (subcall_ret != 0) {
	storage = storage->next;
      } else {
	
	break;
      }
    }
    if (storage == NULL) {
      add_error_to_errorstack(device, LIBMTP_ERROR_STORAGE_FULL, "LIBMTP_Send_File_From_File_Descriptor(): " 
			      "all device storage is full or corrupt.");
      return -1;
    }
    store = storage->id;
  }

  return store;
}

static int get_storage_freespace(LIBMTP_mtpdevice_t *device, 
				 LIBMTP_devicestorage_t *storage,
				 uint64_t *freespace)
{
  PTPParams *params = (PTPParams *) device->params;

  
  
  if (ptp_operation_issupported(params,PTP_OC_GetStorageInfo)) {
    PTPStorageInfo storageInfo;
    uint16_t ret;
    
    ret = ptp_getstorageinfo(params, storage->id, &storageInfo);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "get_first_storage_freespace(): could not get storage info.");
      return -1;
    }
    if (storage->StorageDescription != NULL) {
      free(storage->StorageDescription);
    }
    if (storage->VolumeIdentifier != NULL) {
      free(storage->VolumeIdentifier);
    }
    storage->StorageType = storageInfo.StorageType;
    storage->FilesystemType = storageInfo.FilesystemType;
    storage->AccessCapability = storageInfo.AccessCapability;
    storage->MaxCapacity = storageInfo.MaxCapability;
    storage->FreeSpaceInBytes = storageInfo.FreeSpaceInBytes;
    storage->FreeSpaceInObjects = storageInfo.FreeSpaceInImages;
    storage->StorageDescription = storageInfo.StorageDescription;
    storage->VolumeIdentifier = storageInfo.VolumeLabel;
  }
  if(storage->FreeSpaceInBytes == (uint64_t) -1)
    return -1;
  *freespace = storage->FreeSpaceInBytes;
  return 0;
}

void LIBMTP_Dump_Device_Info(LIBMTP_mtpdevice_t *device)
{
  int i;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  LIBMTP_devicestorage_t *storage = device->storage;

  printf("USB low-level info:\n");
  dump_usbinfo(ptp_usb);
  
  printf("Device info:\n");
  printf("   Manufacturer: %s\n", params->deviceinfo.Manufacturer);
  printf("   Model: %s\n", params->deviceinfo.Model);
  printf("   Device version: %s\n", params->deviceinfo.DeviceVersion);
  printf("   Serial number: %s\n", params->deviceinfo.SerialNumber);
  printf("   Vendor extension ID: 0x%08x\n", params->deviceinfo.VendorExtensionID);
  printf("   Vendor extension description: %s\n", params->deviceinfo.VendorExtensionDesc);
  printf("   Detected object size: %d bits\n", device->object_bitsize);
  printf("Supported operations:\n");
  for (i=0;i<params->deviceinfo.OperationsSupported_len;i++) {
    char txt[256];

    (void) ptp_render_opcode (params, params->deviceinfo.OperationsSupported[i], sizeof(txt), txt);
    printf("   %04x: %s\n", params->deviceinfo.OperationsSupported[i], txt);
  }
  printf("Events supported:\n");
  if (params->deviceinfo.EventsSupported_len == 0) {
    printf("   None.\n");
  } else {
    for (i=0;i<params->deviceinfo.EventsSupported_len;i++) {
      printf("   0x%04x\n", params->deviceinfo.EventsSupported[i]);
    }
  }
  printf("Device Properties Supported:\n");
  for (i=0;i<params->deviceinfo.DevicePropertiesSupported_len;i++) {
    char const *propdesc = ptp_get_property_description(params, params->deviceinfo.DevicePropertiesSupported[i]);

    if (propdesc != NULL) {
      printf("   0x%04x: %s\n", params->deviceinfo.DevicePropertiesSupported[i], propdesc);
    } else {
      uint16_t prop = params->deviceinfo.DevicePropertiesSupported[i];
      printf("   0x%04x: Unknown property\n", prop);
    }
  }

  if (ptp_operation_issupported(params,PTP_OC_MTP_GetObjectPropsSupported)) {
    printf("Playable File (Object) Types and Object Properties Supported:\n");
    for (i=0;i<params->deviceinfo.ImageFormats_len;i++) {
      char txt[256];
      uint16_t ret;
      uint16_t *props = NULL;
      uint32_t propcnt = 0;
      int j;

      (void) ptp_render_ofc (params, params->deviceinfo.ImageFormats[i], sizeof(txt), txt);
      printf("   %04x: %s\n", params->deviceinfo.ImageFormats[i], txt);

      ret = ptp_mtp_getobjectpropssupported (params, params->deviceinfo.ImageFormats[i], &propcnt, &props);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "LIBMTP_Dump_Device_Info(): error on query for object properties.");
      } else {
	for (j=0;j<propcnt;j++) {
	  PTPObjectPropDesc opd;
	  int k;
	  
	  printf("      %04x: %s", props[j], LIBMTP_Get_Property_Description(map_ptp_property_to_libmtp_property(props[j])));
	  
	  ret = ptp_mtp_getobjectpropdesc(params, props[j], params->deviceinfo.ImageFormats[i], &opd);
	  if (ret != PTP_RC_OK) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Dump_Device_Info(): "
				    "could not get property description.");
	    break;
	  }

	  if (opd.DataType == PTP_DTC_STR) {
	    printf(" STRING data type");
	    switch (opd.FormFlag) {
	    case PTP_OPFF_DateTime:
	      printf(" DATETIME FORM");
	      break;
	    case PTP_OPFF_RegularExpression:
	      printf(" REGULAR EXPRESSION FORM");
	      break;
	    case PTP_OPFF_LongString:
	      printf(" LONG STRING FORM");
	      break;
	    default:
	      break;
	    }
	  } else {
	    if (opd.DataType & PTP_DTC_ARRAY_MASK) {
	      printf(" array of");
	    }

	    switch (opd.DataType & (~PTP_DTC_ARRAY_MASK)) {

	    case PTP_DTC_UNDEF:
	      printf(" UNDEFINED data type");
	      break;
	      
	    case PTP_DTC_INT8:
	      printf(" INT8 data type");
	      switch (opd.FormFlag) {
	      case PTP_OPFF_Range:
		printf(" range: MIN %d, MAX %d, STEP %d",
		       opd.FORM.Range.MinimumValue.i8,
		       opd.FORM.Range.MaximumValue.i8,
		       opd.FORM.Range.StepSize.i8);
		break;
	      case PTP_OPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].i8);
		}
		break;
	      case PTP_OPFF_ByteArray:
		printf(" byte array: ");
		break;
	      default:
		printf(" ANY 8BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_UINT8:
	      printf(" UINT8 data type");
	      switch (opd.FormFlag) {
	      case PTP_OPFF_Range:
		printf(" range: MIN %d, MAX %d, STEP %d",
		       opd.FORM.Range.MinimumValue.u8,
		       opd.FORM.Range.MaximumValue.u8,
		       opd.FORM.Range.StepSize.u8);
		break;
	      case PTP_OPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].u8);
		}
		break;
	      case PTP_OPFF_ByteArray:
		printf(" byte array: ");
		break;
	      default:
		printf(" ANY 8BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_INT16:
	      printf(" INT16 data type");
	      switch (opd.FormFlag) {
	      case PTP_OPFF_Range:
	      printf(" range: MIN %d, MAX %d, STEP %d",
		     opd.FORM.Range.MinimumValue.i16,
		     opd.FORM.Range.MaximumValue.i16,
		     opd.FORM.Range.StepSize.i16);
	      break;
	      case PTP_OPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].i16);
		}
		break;
	      default:
		printf(" ANY 16BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_UINT16:
	      printf(" UINT16 data type");
	      switch (opd.FormFlag) {
	      case PTP_OPFF_Range:
		printf(" range: MIN %d, MAX %d, STEP %d",
		       opd.FORM.Range.MinimumValue.u16,
		       opd.FORM.Range.MaximumValue.u16,
		       opd.FORM.Range.StepSize.u16);
		break;
	      case PTP_OPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].u16);
		}
		break;
	      default:
		printf(" ANY 16BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_INT32:
	      printf(" INT32 data type");
	      switch (opd.FormFlag) {
	      case PTP_OPFF_Range:
		printf(" range: MIN %d, MAX %d, STEP %d",
		       opd.FORM.Range.MinimumValue.i32,
		       opd.FORM.Range.MaximumValue.i32,
		       opd.FORM.Range.StepSize.i32);
		break;
	      case PTP_OPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].i32);
		}
		break;
	      default:
		printf(" ANY 32BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_UINT32:
	      printf(" UINT32 data type");
	      switch (opd.FormFlag) {
	      case PTP_OPFF_Range:
		printf(" range: MIN %d, MAX %d, STEP %d",
		       opd.FORM.Range.MinimumValue.u32,
		       opd.FORM.Range.MaximumValue.u32,
		       opd.FORM.Range.StepSize.u32);
		break;
	      case PTP_OPFF_Enumeration:
		
		if (params->deviceinfo.ImageFormats[i] == PTP_OPC_VideoFourCCCodec) {
		  printf(" enumeration of u32 casted FOURCC: ");
		  for (k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		    if (opd.FORM.Enum.SupportedValue[k].u32 == 0) {
		      printf("ANY, ");
		    } else {
		      char fourcc[6];
		      fourcc[0] = (opd.FORM.Enum.SupportedValue[k].u32 >> 24) & 0xFFU;
		      fourcc[1] = (opd.FORM.Enum.SupportedValue[k].u32 >> 16) & 0xFFU;
		      fourcc[2] = (opd.FORM.Enum.SupportedValue[k].u32 >> 8) & 0xFFU;
		      fourcc[3] = opd.FORM.Enum.SupportedValue[k].u32 & 0xFFU;
		      fourcc[4] = '\n';
		      fourcc[5] = '\0';
		      printf("\"%s\", ", fourcc);
		    }
		  }
		} else {
		  printf(" enumeration: ");
		  for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		    printf("%d, ", opd.FORM.Enum.SupportedValue[k].u32);
		  }
		}
		break;
	      default:
		printf(" ANY 32BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_INT64:
	      printf(" INT64 data type");
	      break;
	      
	    case PTP_DTC_UINT64:
	      printf(" UINT64 data type");
	      break;
	      
	    case PTP_DTC_INT128:
	      printf(" INT128 data type");
	      break;
	      
	    case PTP_DTC_UINT128:
	      printf(" UINT128 data type");
	      break;
	      
	    default:
	      printf(" UNKNOWN data type");
	      break;
	    }
	  }
	  if (opd.GetSet) {
	    printf(" GET/SET");
	  } else {
	    printf(" READ ONLY");
	  }
	  printf("\n");
	  ptp_free_objectpropdesc(&opd);
	}
	free(props);
      }
    }
  }

  if(storage != NULL && ptp_operation_issupported(params,PTP_OC_GetStorageInfo)) {
    printf("Storage Devices:\n");
    while(storage != NULL) {
      printf("   StorageID: 0x%08x\n",storage->id);
      printf("      StorageType: 0x%04x ",storage->StorageType);
      switch (storage->StorageType) {
      case PTP_ST_Undefined:
	printf("(undefined)\n");
	break;
      case PTP_ST_FixedROM:
	printf("fixed ROM storage\n");
	break;
      case PTP_ST_RemovableROM:
	printf("removable ROM storage\n");
	break;
      case PTP_ST_FixedRAM:
	printf("fixed RAM storage\n");
	break;
      case PTP_ST_RemovableRAM:
	printf("removable RAM storage\n");
	break;
      default:
	printf("UNKNOWN storage\n");
	break;
      }
      printf("      FilesystemType: 0x%04x ",storage->FilesystemType);
      switch(storage->FilesystemType) {
      case PTP_FST_Undefined:
	printf("(undefined)\n");
	break;
      case PTP_FST_GenericFlat:
	printf("generic flat filesystem\n");
	break;
      case PTP_FST_GenericHierarchical:
	printf("generic hierarchical\n");
	break;
      case PTP_FST_DCF:
	printf("DCF\n");
	break;
      default:
	printf("UNKNONWN filesystem type\n");
	break;
      }
      printf("      AccessCapability: 0x%04x ",storage->AccessCapability);
      switch(storage->AccessCapability) {
      case PTP_AC_ReadWrite:
	printf("read/write\n");
	break;
      case PTP_AC_ReadOnly:
	printf("read only\n");
	break;
      case PTP_AC_ReadOnly_with_Object_Deletion:
	printf("read only + object deletion\n");
	break;
      default:
	printf("UNKNOWN access capability\n");
	break;
      }
      printf("      MaxCapacity: %llu\n", (long long unsigned int) storage->MaxCapacity);
      printf("      FreeSpaceInBytes: %llu\n", (long long unsigned int) storage->FreeSpaceInBytes);
      printf("      FreeSpaceInObjects: %llu\n", (long long unsigned int) storage->FreeSpaceInObjects);
      printf("      StorageDescription: %s\n",storage->StorageDescription);
      printf("      VolumeIdentifier: %s\n",storage->VolumeIdentifier);
      storage = storage->next;
    }
  }

  printf("Special directories:\n");
  printf("   Default music folder: 0x%08x\n", device->default_music_folder);
  printf("   Default playlist folder: 0x%08x\n", device->default_playlist_folder);
  printf("   Default picture folder: 0x%08x\n", device->default_picture_folder);
  printf("   Default video folder: 0x%08x\n", device->default_video_folder);
  printf("   Default organizer folder: 0x%08x\n", device->default_organizer_folder);
  printf("   Default zencast folder: 0x%08x\n", device->default_zencast_folder);
  printf("   Default album folder: 0x%08x\n", device->default_album_folder);
  printf("   Default text folder: 0x%08x\n", device->default_text_folder);
}

int LIBMTP_Reset_Device(LIBMTP_mtpdevice_t *device)
{
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (!ptp_operation_issupported(params,PTP_OC_ResetDevice)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, 
			    "LIBMTP_Reset_Device(): device does not support resetting.");
    return -1;
  }
  ret = ptp_resetdevice(params);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "Error resetting.");
    return -1;
  }
  return 0;
}

char *LIBMTP_Get_Manufacturername(LIBMTP_mtpdevice_t *device)
{
  char *retmanuf = NULL;
  PTPParams *params = (PTPParams *) device->params;

  if (params->deviceinfo.Manufacturer != NULL) {
    retmanuf = strdup(params->deviceinfo.Manufacturer);
  }
  return retmanuf;
}

char *LIBMTP_Get_Modelname(LIBMTP_mtpdevice_t *device)
{
  char *retmodel = NULL;
  PTPParams *params = (PTPParams *) device->params;

  if (params->deviceinfo.Model != NULL) {
    retmodel = strdup(params->deviceinfo.Model);
  }
  return retmodel;
}

char *LIBMTP_Get_Serialnumber(LIBMTP_mtpdevice_t *device)
{
  char *retnumber = NULL;
  PTPParams *params = (PTPParams *) device->params;

  if (params->deviceinfo.SerialNumber != NULL) {
    retnumber = strdup(params->deviceinfo.SerialNumber);
  }
  return retnumber;
}

char *LIBMTP_Get_Deviceversion(LIBMTP_mtpdevice_t *device)
{
  char *retversion = NULL;
  PTPParams *params = (PTPParams *) device->params;

  if (params->deviceinfo.DeviceVersion != NULL) {
    retversion = strdup(params->deviceinfo.DeviceVersion);
  }
  return retversion;
}


char *LIBMTP_Get_Friendlyname(LIBMTP_mtpdevice_t *device)
{
  PTPPropertyValue propval;
  char *retstring = NULL;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (!ptp_property_issupported(params, PTP_DPC_MTP_DeviceFriendlyName)) {
    return NULL;
  }

  ret = ptp_getdevicepropvalue(params,
			       PTP_DPC_MTP_DeviceFriendlyName,
			       &propval,
			       PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "Error getting friendlyname.");
    return NULL;
  }
  if (propval.str != NULL) {
    retstring = strdup(propval.str);
    free(propval.str);
  }
  return retstring;
}

int LIBMTP_Set_Friendlyname(LIBMTP_mtpdevice_t *device,
			 char const * const friendlyname)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (!ptp_property_issupported(params, PTP_DPC_MTP_DeviceFriendlyName)) {
    return -1;
  }
  propval.str = (char *) friendlyname;
  ret = ptp_setdevicepropvalue(params,
			       PTP_DPC_MTP_DeviceFriendlyName,
			       &propval,
			       PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "Error setting friendlyname.");
    return -1;
  }
  return 0;
}

char *LIBMTP_Get_Syncpartner(LIBMTP_mtpdevice_t *device)
{
  PTPPropertyValue propval;
  char *retstring = NULL;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (!ptp_property_issupported(params, PTP_DPC_MTP_SynchronizationPartner)) {
    return NULL;
  }

  ret = ptp_getdevicepropvalue(params,
			       PTP_DPC_MTP_SynchronizationPartner,
			       &propval,
			       PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "Error getting syncpartner.");
    return NULL;
  }
  if (propval.str != NULL) {
    retstring = strdup(propval.str);
    free(propval.str);
  }
  return retstring;
}


int LIBMTP_Set_Syncpartner(LIBMTP_mtpdevice_t *device,
			 char const * const syncpartner)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (!ptp_property_issupported(params, PTP_DPC_MTP_SynchronizationPartner)) {
    return -1;
  }
  propval.str = (char *) syncpartner;
  ret = ptp_setdevicepropvalue(params,
			       PTP_DPC_MTP_SynchronizationPartner,
			       &propval,
			       PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "Error setting syncpartner.");
    return -1;
  }
  return 0;
}

static int check_if_file_fits(LIBMTP_mtpdevice_t *device,
			      LIBMTP_devicestorage_t *storage,
			      uint64_t const filesize) {
  PTPParams *params = (PTPParams *) device->params;
  uint64_t freebytes;
  int ret;

  
  if (!ptp_operation_issupported(params,PTP_OC_GetStorageInfo)) {
    return 0;
  }
  
  ret = get_storage_freespace(device, storage, &freebytes);
  if (ret != 0) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, 
			    "check_if_file_fits(): error checking free storage.");
    return -1;
  } else {
    
    if (filesize > freebytes) {
      return -1;
    }
  }
  return 0;
}


int LIBMTP_Get_Batterylevel(LIBMTP_mtpdevice_t *device,
			    uint8_t * const maximum_level,
			    uint8_t * const current_level)
{
  PTPPropertyValue propval;
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  *maximum_level = 0;
  *current_level = 0;

  if (FLAG_BROKEN_BATTERY_LEVEL(ptp_usb) ||
      !ptp_property_issupported(params, PTP_DPC_BatteryLevel)) {
    return -1;
  }

  ret = ptp_getdevicepropvalue(params, PTP_DPC_BatteryLevel, &propval, PTP_DTC_UINT8);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Batterylevel(): could not get device property value.");
    return -1;
  }

  *maximum_level = device->maximum_battery_level;
  *current_level = propval.u8;

  return 0;
}


int LIBMTP_Format_Storage(LIBMTP_mtpdevice_t *device, LIBMTP_devicestorage_t *storage)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;

  if (!ptp_operation_issupported(params,PTP_OC_FormatStore)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, 
			    "LIBMTP_Format_Storage(): device does not support formatting storage.");
    return -1;
  }
  ret = ptp_formatstore(params, storage->id);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Format_Storage(): failed to format storage.");
    return -1;
  }
  return 0;
}

static int get_device_unicode_property(LIBMTP_mtpdevice_t *device,
				       char **unicstring, uint16_t property)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t *tmp;
  uint16_t ret;
  int i;

  if (!ptp_property_issupported(params, property)) {
    return -1;
  }

  
  ret = ptp_getdevicepropvalue(params,
			       property,
			       &propval,
			       PTP_DTC_AUINT16);
  if (ret != PTP_RC_OK) {
    
    *unicstring = NULL;
    add_ptp_error_to_errorstack(device, ret, "get_device_unicode_property(): failed to get unicode property.");
    return -1;
  }

  
  
  tmp = malloc((propval.a.count + 1)*sizeof(uint16_t));
  for (i = 0; i < propval.a.count; i++) {
    tmp[i] = propval.a.v[i].u16;
    
  }
  tmp[propval.a.count] = 0x0000U;
  free(propval.a.v);

  *unicstring = utf16_to_utf8(device, tmp);

  free(tmp);

  return 0;
}

int LIBMTP_Get_Secure_Time(LIBMTP_mtpdevice_t *device, char ** const sectime)
{
  return get_device_unicode_property(device, sectime, PTP_DPC_MTP_SecureTime);
}

int LIBMTP_Get_Device_Certificate(LIBMTP_mtpdevice_t *device, char ** const devcert)
{
  return get_device_unicode_property(device, devcert, PTP_DPC_MTP_DeviceCertificate);
}

int LIBMTP_Get_Supported_Filetypes(LIBMTP_mtpdevice_t *device, uint16_t ** const filetypes,
				  uint16_t * const length)
{
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint16_t *localtypes;
  uint16_t localtypelen;
  uint32_t i;

  
  localtypes = (uint16_t *) malloc(params->deviceinfo.ImageFormats_len * sizeof(uint16_t));
  localtypelen = 0;

  for (i=0;i<params->deviceinfo.ImageFormats_len;i++) {
    uint16_t localtype = map_ptp_type_to_libmtp_type(params->deviceinfo.ImageFormats[i]);
    if (localtype != LIBMTP_FILETYPE_UNKNOWN) {
      localtypes[localtypelen] = localtype;
      localtypelen++;
    }
  }
  
  if (FLAG_OGG_IS_UNKNOWN(ptp_usb)) {
    localtypes = (uint16_t *) realloc(localtypes, (params->deviceinfo.ImageFormats_len+1) * sizeof(uint16_t));
    localtypes[localtypelen] = LIBMTP_FILETYPE_OGG;
    localtypelen++;
  }
  
  if (FLAG_FLAC_IS_UNKNOWN(ptp_usb)) {
    localtypes = (uint16_t *) realloc(localtypes, (params->deviceinfo.ImageFormats_len+1) * sizeof(uint16_t));
    localtypes[localtypelen] = LIBMTP_FILETYPE_FLAC;
    localtypelen++;
  }

  *filetypes = localtypes;
  *length = localtypelen;

  return 0;
}

int LIBMTP_Get_Storage(LIBMTP_mtpdevice_t *device, int const sortby)
{
  uint32_t i = 0;
  PTPStorageInfo storageInfo;
  PTPParams *params = (PTPParams *) device->params;
  PTPStorageIDs storageIDs;
  LIBMTP_devicestorage_t *storage = NULL;
  LIBMTP_devicestorage_t *storageprev = NULL;

  if (device->storage != NULL)
    free_storage_list(device);

  
  
  if (ptp_getstorageids (params, &storageIDs) != PTP_RC_OK) 
    return -1;
  if (storageIDs.n < 1) 
    return -1;

  if (!ptp_operation_issupported(params,PTP_OC_GetStorageInfo)) {
    for (i = 0; i < storageIDs.n; i++) {

      storage = (LIBMTP_devicestorage_t *) malloc(sizeof(LIBMTP_devicestorage_t));
      storage->prev = storageprev;
      if (storageprev != NULL)
        storageprev->next = storage;
      if (device->storage == NULL) 
        device->storage = storage;

      storage->id = storageIDs.Storage[i];
      storage->StorageType = PTP_ST_Undefined;
      storage->FilesystemType = PTP_FST_Undefined;
      storage->AccessCapability = PTP_AC_ReadWrite;
      storage->MaxCapacity = (uint64_t) -1;
      storage->FreeSpaceInBytes = (uint64_t) -1;
      storage->FreeSpaceInObjects = (uint64_t) -1;
      storage->StorageDescription = strdup("Unknown storage");
      storage->VolumeIdentifier = strdup("Unknown volume");
      storage->next = NULL;

      storageprev = storage;
    }
    free(storageIDs.Storage);
    return 1;
  } else {
    for (i = 0; i < storageIDs.n; i++) {
      uint16_t ret;
      ret = ptp_getstorageinfo(params, storageIDs.Storage[i], &storageInfo);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Storage(): Could not get storage info.");
	if (device->storage != NULL) {
          free_storage_list(device);
	}
	return -1;
      }

      storage = (LIBMTP_devicestorage_t *) malloc(sizeof(LIBMTP_devicestorage_t));
      storage->prev = storageprev;
      if (storageprev != NULL)
        storageprev->next = storage;
      if (device->storage == NULL)
        device->storage = storage;

      storage->id = storageIDs.Storage[i];
      storage->StorageType = storageInfo.StorageType;
      storage->FilesystemType = storageInfo.FilesystemType;
      storage->AccessCapability = storageInfo.AccessCapability;
      storage->MaxCapacity = storageInfo.MaxCapability;
      storage->FreeSpaceInBytes = storageInfo.FreeSpaceInBytes;
      storage->FreeSpaceInObjects = storageInfo.FreeSpaceInImages;
      storage->StorageDescription = storageInfo.StorageDescription;
      storage->VolumeIdentifier = storageInfo.VolumeLabel;
      storage->next = NULL;

      storageprev = storage;
    }

    if (storage != NULL)
      storage->next = NULL;

    sort_storage_by(device,sortby);
    free(storageIDs.Storage);
    return 0;
  }
}

LIBMTP_file_t *LIBMTP_new_file_t(void)
{
  LIBMTP_file_t *new = (LIBMTP_file_t *) malloc(sizeof(LIBMTP_file_t));
  if (new == NULL) {
    return NULL;
  }
  new->filename = NULL;
  new->item_id = 0;
  new->parent_id = 0;
  new->storage_id = 0;
  new->filesize = 0;
  new->modificationdate = 0;
  new->filetype = LIBMTP_FILETYPE_UNKNOWN;
  new->next = NULL;
  return new;
}

void LIBMTP_destroy_file_t(LIBMTP_file_t *file)
{
  if (file == NULL) {
    return;
  }
  if (file->filename != NULL)
    free(file->filename);
  free(file);
  return;
}

LIBMTP_file_t *LIBMTP_Get_Filelisting(LIBMTP_mtpdevice_t *device)
{
  printf("WARNING: LIBMTP_Get_Filelisting() is deprecated.\n");
  printf("WARNING: please update your code to use LIBMTP_Get_Filelisting_With_Callback()\n");
  return LIBMTP_Get_Filelisting_With_Callback(device, NULL, NULL);
}

LIBMTP_file_t *LIBMTP_Get_Filelisting_With_Callback(LIBMTP_mtpdevice_t *device,
                                                    LIBMTP_progressfunc_t const callback,
                                                    void const * const data)
{
  uint32_t i = 0;
  LIBMTP_file_t *retfiles = NULL;
  LIBMTP_file_t *curfile = NULL;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint16_t ret;

  
  if (params->nrofobjects == 0) {
    flush_handles(device);
  }

  for (i = 0; i < params->nrofobjects; i++) {
    LIBMTP_file_t *file;
    PTPObject *ob, *xob;

    if (callback != NULL)
      callback(i, params->nrofobjects, data);

    ob = &params->objects[i];

    if (ob->oi.ObjectFormat == PTP_OFC_Association) {
      
      
      continue;
    }

    
    file = LIBMTP_new_file_t();

    file->parent_id = ob->oi.ParentObject;
    file->storage_id = ob->oi.StorageID;

    
    file->item_id = ob->oid;

    
    file->filetype = map_ptp_type_to_libmtp_type(ob->oi.ObjectFormat);
    
    
    file->modificationdate = ob->oi.ModificationDate;

    
    
    
    file->filesize = ob->oi.ObjectCompressedSize;
    if (ob->oi.Filename != NULL) {
      file->filename = strdup(ob->oi.Filename);
    }

    if (file->filetype == LIBMTP_FILETYPE_UNKNOWN) {
      if ((FLAG_IRIVER_OGG_ALZHEIMER(ptp_usb) ||
	   FLAG_OGG_IS_UNKNOWN(ptp_usb)) &&
	  has_ogg_extension(file->filename))
	file->filetype = LIBMTP_FILETYPE_OGG;
      if (FLAG_FLAC_IS_UNKNOWN(ptp_usb) &&
	  has_flac_extension(file->filename))
	file->filetype = LIBMTP_FILETYPE_FLAC;
    }

    ret = ptp_object_want (params, ob->oid, PTPOBJECT_MTPPROPLIST_LOADED, &xob);
    if (ob->mtpprops) {
      MTPProperties *prop = ob->mtpprops;
      int i;

      for (i=0;i<ob->nrofmtpprops;i++) {
	
	if (prop->property == PTP_OPC_ObjectSize) {
	  if (device->object_bitsize == 64) {
	    file->filesize = prop->propval.u64;
	  } else {
	    file->filesize = prop->propval.u32;
	  }
	  break;
	}
	prop++;
      }
    } else {
      uint16_t *props = NULL;
      uint32_t propcnt = 0;
      
      
      ret = ptp_mtp_getobjectpropssupported(params, ob->oi.ObjectFormat, &propcnt, &props);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Filelisting_With_Callback(): call to ptp_mtp_getobjectpropssupported() failed.");
	
      } else {
        int i;
	for (i=0;i<propcnt;i++) {
	  switch (props[i]) {
	  case PTP_OPC_ObjectSize:
	    if (device->object_bitsize == 64) {
	      file->filesize = get_u64_from_object(device, file->item_id, PTP_OPC_ObjectSize, 0);
	    } else {
	      file->filesize = get_u32_from_object(device, file->item_id, PTP_OPC_ObjectSize, 0);
	    }
	    break;
	  default:
	    break;
	  }
	}
	free(props);
      }
    }
    
    
    if (retfiles == NULL) {
      retfiles = file;
      curfile = file;
    } else {
      curfile->next = file;
      curfile = file;
    }

    
    
    
  } 
  return retfiles;
}

LIBMTP_file_t *LIBMTP_Get_Filemetadata(LIBMTP_mtpdevice_t *device, uint32_t const fileid)
{
  uint32_t i = 0;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;
  PTPObject *ob;
  LIBMTP_file_t *file;

  
  if (params->nrofobjects == 0) {
    flush_handles(device);
  }

  ret = ptp_object_want (params, fileid, PTPOBJECT_OBJECTINFO_LOADED|PTPOBJECT_MTPPROPLIST_LOADED, &ob);
  if (ret != PTP_RC_OK)
    return NULL;

  
  file = LIBMTP_new_file_t();
  
  file->parent_id = ob->oi.ParentObject;
  file->storage_id = ob->oi.StorageID;

  
  file->filetype = map_ptp_type_to_libmtp_type(ob->oi.ObjectFormat);

  
  
  
  file->filesize = ob->oi.ObjectCompressedSize;
  if (ob->oi.Filename != NULL) {
    file->filename = strdup(ob->oi.Filename);
  }

  
  file->item_id = fileid;

  if (ob->mtpprops) {
    MTPProperties *prop = ob->mtpprops;
    
    for (i=0;i<ob->nrofmtpprops;i++,prop++) {
      
      if (prop->property == PTP_OPC_ObjectSize) {
	
	
	if (device->object_bitsize == 64) {
	  file->filesize = prop->propval.u64;
	} else {
	  file->filesize = prop->propval.u32;
	}
	break;
      }
    }
  } else {
    uint16_t *props = NULL;
    uint32_t propcnt = 0;
    
    
    ret = ptp_mtp_getobjectpropssupported(params, map_libmtp_type_to_ptp_type(file->filetype), &propcnt, &props);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Filemetadata(): call to ptp_mtp_getobjectpropssupported() failed.");
      
    } else {
      for (i=0;i<propcnt;i++) {
	switch (props[i]) {
	case PTP_OPC_ObjectSize:
	  if (device->object_bitsize == 64) {
	    file->filesize = get_u64_from_object(device, file->item_id, PTP_OPC_ObjectSize, 0);
	  } else {
	    file->filesize = get_u32_from_object(device, file->item_id, PTP_OPC_ObjectSize, 0);
	  }
	  break;
	default:
	  break;
	}
      }
      free(props);
    }
  }
  
  return file;
}

LIBMTP_track_t *LIBMTP_new_track_t(void)
{
  LIBMTP_track_t *new = (LIBMTP_track_t *) malloc(sizeof(LIBMTP_track_t));
  if (new == NULL) {
    return NULL;
  }
  new->item_id = 0;
  new->parent_id = 0;
  new->storage_id = 0;
  new->title = NULL;
  new->artist = NULL;
  new->composer = NULL;
  new->album = NULL;
  new->genre = NULL;
  new->date = NULL;
  new->filename = NULL;
  new->duration = 0;
  new->tracknumber = 0;
  new->filesize = 0;
  new->filetype = LIBMTP_FILETYPE_UNKNOWN;
  new->samplerate = 0;
  new->nochannels = 0;
  new->wavecodec = 0;
  new->bitrate = 0;
  new->bitratetype = 0;
  new->rating = 0;
  new->usecount = 0;
  new->modificationdate = 0;
  new->next = NULL;
  return new;
}

void LIBMTP_destroy_track_t(LIBMTP_track_t *track)
{
  if (track == NULL) {
    return;
  }
  if (track->title != NULL)
    free(track->title);
  if (track->artist != NULL)
    free(track->artist);
  if (track->composer != NULL)
    free(track->composer);
  if (track->album != NULL)
    free(track->album);
  if (track->genre != NULL)
    free(track->genre);
  if (track->date != NULL)
    free(track->date);
  if (track->filename != NULL)
    free(track->filename);
  free(track);
  return;
}

static void pick_property_to_track_metadata(LIBMTP_mtpdevice_t *device, MTPProperties *prop, LIBMTP_track_t *track)
{
  switch (prop->property) {
  case PTP_OPC_Name:
    if (prop->propval.str != NULL)
      track->title = strdup(prop->propval.str);
    else
      track->title = NULL;
    break;
  case PTP_OPC_Artist:
    if (prop->propval.str != NULL)
      track->artist = strdup(prop->propval.str);
    else
      track->artist = NULL;
    break;
  case PTP_OPC_Composer:
    if (prop->propval.str != NULL)
      track->composer = strdup(prop->propval.str);
    else
      track->composer = NULL;
    break;
  case PTP_OPC_Duration:
    track->duration = prop->propval.u32;
    break;
  case PTP_OPC_Track:
    track->tracknumber = prop->propval.u16;
    break;
  case PTP_OPC_Genre:
    if (prop->propval.str != NULL)
      track->genre = strdup(prop->propval.str);
    else
      track->genre = NULL;
    break;
  case PTP_OPC_AlbumName:
    if (prop->propval.str != NULL)
      track->album = strdup(prop->propval.str);
    else
      track->album = NULL;
    break;
  case PTP_OPC_OriginalReleaseDate:
    if (prop->propval.str != NULL)
      track->date = strdup(prop->propval.str);
    else
      track->date = NULL;
    break;
    
  case PTP_OPC_SampleRate:
    track->samplerate = prop->propval.u32;
    break;
  case PTP_OPC_NumberOfChannels:
    track->nochannels = prop->propval.u16;
    break;
  case PTP_OPC_AudioWAVECodec:
    track->wavecodec = prop->propval.u32;
    break;
  case PTP_OPC_AudioBitRate:
    track->bitrate = prop->propval.u32;
    break;
  case PTP_OPC_BitRateType:
    track->bitratetype = prop->propval.u16;
    break;
  case PTP_OPC_Rating:
    track->rating = prop->propval.u16;
    break;
  case PTP_OPC_UseCount:
    track->usecount = prop->propval.u32;
    break;
  case PTP_OPC_ObjectSize:
    if (device->object_bitsize == 64) {
      track->filesize = prop->propval.u64;
    } else {
      track->filesize = prop->propval.u32;
    }
    break;
  default:
    break;
  }
}

static void get_track_metadata(LIBMTP_mtpdevice_t *device, uint16_t objectformat,
			       LIBMTP_track_t *track)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  uint32_t i;
  MTPProperties *prop;
  PTPObject *ob;

  ret = ptp_object_want(params, track->item_id, PTPOBJECT_MTPPROPLIST_LOADED, &ob);
  if (ob->mtpprops) {
    prop = ob->mtpprops;
    for (i=0;i<ob->nrofmtpprops;i++,prop++)
      pick_property_to_track_metadata(device, prop, track);
  } else {
    uint16_t *props = NULL;
    uint32_t propcnt = 0;

    
    ret = ptp_mtp_getobjectpropssupported(params, map_libmtp_type_to_ptp_type(track->filetype), &propcnt, &props);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "get_track_metadata(): call to ptp_mtp_getobjectpropssupported() failed.");
      
      return;
    } else {
      for (i=0;i<propcnt;i++) {
	switch (props[i]) {
	case PTP_OPC_Name:
	  track->title = get_string_from_object(device, track->item_id, PTP_OPC_Name);
	  break;
	case PTP_OPC_Artist:
	  track->artist = get_string_from_object(device, track->item_id, PTP_OPC_Artist);
	  break;
	case PTP_OPC_Composer:
	  track->composer = get_string_from_object(device, track->item_id, PTP_OPC_Composer);
	  break;
	case PTP_OPC_Duration:
	  track->duration = get_u32_from_object(device, track->item_id, PTP_OPC_Duration, 0);
	  break;
	case PTP_OPC_Track:
	  track->tracknumber = get_u16_from_object(device, track->item_id, PTP_OPC_Track, 0);
	  break;
	case PTP_OPC_Genre:
	  track->genre = get_string_from_object(device, track->item_id, PTP_OPC_Genre);
	  break;
	case PTP_OPC_AlbumName:
	  track->album = get_string_from_object(device, track->item_id, PTP_OPC_AlbumName);
	  break;
	case PTP_OPC_OriginalReleaseDate:
	  track->date = get_string_from_object(device, track->item_id, PTP_OPC_OriginalReleaseDate);
	  break;
	  
	case PTP_OPC_SampleRate:
	  track->samplerate = get_u32_from_object(device, track->item_id, PTP_OPC_SampleRate, 0);
	  break;
	case PTP_OPC_NumberOfChannels:
	  track->nochannels = get_u16_from_object(device, track->item_id, PTP_OPC_NumberOfChannels, 0);
	  break;
	case PTP_OPC_AudioWAVECodec:
	  track->wavecodec = get_u32_from_object(device, track->item_id, PTP_OPC_AudioWAVECodec, 0);
	  break;
	case PTP_OPC_AudioBitRate:
	  track->bitrate = get_u32_from_object(device, track->item_id, PTP_OPC_AudioBitRate, 0);
	  break;
	case PTP_OPC_BitRateType:
	  track->bitratetype = get_u16_from_object(device, track->item_id, PTP_OPC_BitRateType, 0);
	  break;
	case PTP_OPC_Rating:
	  track->rating = get_u16_from_object(device, track->item_id, PTP_OPC_Rating, 0);
	  break;
	case PTP_OPC_UseCount:
	  track->usecount = get_u32_from_object(device, track->item_id, PTP_OPC_UseCount, 0);
	  break;
	case PTP_OPC_ObjectSize:
	  if (device->object_bitsize == 64) {
	    track->filesize = get_u64_from_object(device, track->item_id, PTP_OPC_ObjectSize, 0);
	  } else {
	    track->filesize = (uint64_t) get_u32_from_object(device, track->item_id, PTP_OPC_ObjectSize, 0);
	  }
	  break;
	}
      }
      free(props);
    }
  }
}

LIBMTP_track_t *LIBMTP_Get_Tracklisting(LIBMTP_mtpdevice_t *device)
{
  printf("WARNING: LIBMTP_Get_Tracklisting() is deprecated.\n");
  printf("WARNING: please update your code to use LIBMTP_Get_Tracklisting_With_Callback()\n");
  return LIBMTP_Get_Tracklisting_With_Callback(device, NULL, NULL);
}

LIBMTP_track_t *LIBMTP_Get_Tracklisting_With_Callback(LIBMTP_mtpdevice_t *device,
                                                      LIBMTP_progressfunc_t const callback,
                                                      void const * const data)
{
  uint32_t i = 0;
  LIBMTP_track_t *retracks = NULL;
  LIBMTP_track_t *curtrack = NULL;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  
  if (params->nrofobjects == 0) {
    flush_handles(device);
  }

  for (i = 0; i < params->nrofobjects; i++) {
    LIBMTP_track_t *track;
    PTPObject *ob;
    LIBMTP_filetype_t mtptype;

    if (callback != NULL)
      callback(i, params->nrofobjects, data);

    ob = &params->objects[i];
    mtptype = map_ptp_type_to_libmtp_type(ob->oi.ObjectFormat);

    
    
    
    
    if (!LIBMTP_FILETYPE_IS_TRACK(mtptype) &&
	
	(ob->oi.ObjectFormat != PTP_OFC_Undefined || 
	 (!FLAG_IRIVER_OGG_ALZHEIMER(ptp_usb) &&
	  !FLAG_OGG_IS_UNKNOWN(ptp_usb) &&
	  !FLAG_FLAC_IS_UNKNOWN(ptp_usb)))
	) {
      
      continue;
    }

    
    track = LIBMTP_new_track_t();
      
    
    track->item_id = ob->oid;
    track->parent_id = ob->oi.ParentObject;
    track->storage_id = ob->oi.StorageID;
    track->modificationdate = ob->oi.ModificationDate;

    track->filetype = mtptype;

    
    track->filesize = ob->oi.ObjectCompressedSize;
    if (ob->oi.Filename != NULL) {
      track->filename = strdup(ob->oi.Filename);
    }

    get_track_metadata(device, ob->oi.ObjectFormat, track);

    if (track->filetype == LIBMTP_FILETYPE_UNKNOWN &&
	track->filename != NULL) {
      if ((FLAG_IRIVER_OGG_ALZHEIMER(ptp_usb) ||
	   FLAG_OGG_IS_UNKNOWN(ptp_usb)) &&
	  has_ogg_extension(track->filename))
	track->filetype = LIBMTP_FILETYPE_OGG;
      else if (FLAG_FLAC_IS_UNKNOWN(ptp_usb) &&
	       has_flac_extension(track->filename))
	track->filetype = LIBMTP_FILETYPE_FLAC;
      else {
	
	LIBMTP_destroy_track_t(track);
	continue;
      }
    }
    
    
    if (retracks == NULL) {
      retracks = track;
      curtrack = track;
    } else {
      curtrack->next = track;
      curtrack = track;
    }
    
    
    
    
    
  } 
  return retracks;
}

LIBMTP_track_t *LIBMTP_Get_Trackmetadata(LIBMTP_mtpdevice_t *device, uint32_t const trackid)
{
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  PTPObject *ob;
  LIBMTP_track_t *track;
  LIBMTP_filetype_t mtptype;
  uint16_t ret;

  
  if (params->nrofobjects == 0)
    flush_handles(device);

  ret = ptp_object_want (params, trackid, PTPOBJECT_OBJECTINFO_LOADED, &ob);
  if (ret != PTP_RC_OK)
    return NULL;

  mtptype = map_ptp_type_to_libmtp_type(ob->oi.ObjectFormat);

  
  if (!LIBMTP_FILETYPE_IS_TRACK(mtptype) &&
      (ob->oi.ObjectFormat != PTP_OFC_Undefined || 
       (!FLAG_IRIVER_OGG_ALZHEIMER(ptp_usb) &&
	!FLAG_OGG_IS_UNKNOWN(ptp_usb) &&
	!FLAG_FLAC_IS_UNKNOWN(ptp_usb)))
      ) {
    
    return NULL;
  }

  
  track = LIBMTP_new_track_t();
  
  
  track->item_id = ob->oid;
  track->parent_id = ob->oi.ParentObject;
  track->storage_id = ob->oi.StorageID;
  track->modificationdate = ob->oi.ModificationDate;

  track->filetype = mtptype;

  
  track->filesize = ob->oi.ObjectCompressedSize;
  if (ob->oi.Filename != NULL) {
    track->filename = strdup(ob->oi.Filename);
  }

  if (track->filetype == LIBMTP_FILETYPE_UNKNOWN &&
      track->filename != NULL) {
    if ((FLAG_IRIVER_OGG_ALZHEIMER(ptp_usb) ||
	 FLAG_OGG_IS_UNKNOWN(ptp_usb)) &&
	has_ogg_extension(track->filename))
      track->filetype = LIBMTP_FILETYPE_OGG;
    else if (FLAG_FLAC_IS_UNKNOWN(ptp_usb) &&
	     has_flac_extension(track->filename))
      track->filetype = LIBMTP_FILETYPE_FLAC;
    else {
      
      LIBMTP_destroy_track_t(track);
      return NULL;
    }
  }
  get_track_metadata(device, ob->oi.ObjectFormat, track);
  return track;
}

static uint16_t get_func_wrapper(PTPParams* params, void* priv, unsigned long wantlen, unsigned char *data, unsigned long *gotlen)
{
  MTPDataHandler *handler = (MTPDataHandler *)priv;
  uint16_t ret;
  uint32_t local_gotlen = 0;
  ret = handler->getfunc(params, handler->priv, wantlen, data, &local_gotlen);
  *gotlen = local_gotlen;
  switch (ret)
  {
    case LIBMTP_HANDLER_RETURN_OK:
      return PTP_RC_OK;
    case LIBMTP_HANDLER_RETURN_ERROR:
      return PTP_ERROR_IO;
    case LIBMTP_HANDLER_RETURN_CANCEL:
      return PTP_ERROR_CANCEL;
    default:
      return PTP_ERROR_IO;
  }
}

static uint16_t put_func_wrapper(PTPParams* params, void* priv, unsigned long sendlen, unsigned char *data, unsigned long *putlen)
{
  MTPDataHandler *handler = (MTPDataHandler *)priv;
  uint16_t ret;
  uint32_t local_putlen = 0;
  ret = handler->putfunc(params, handler->priv, sendlen, data, &local_putlen);
  *putlen = local_putlen;
  switch (ret)
  {
    case LIBMTP_HANDLER_RETURN_OK:
      return PTP_RC_OK;
    case LIBMTP_HANDLER_RETURN_ERROR:
      return PTP_ERROR_IO;
    case LIBMTP_HANDLER_RETURN_CANCEL:
      return PTP_ERROR_CANCEL;
    default:
      return PTP_ERROR_IO;
  }
}

int LIBMTP_Get_File_To_File(LIBMTP_mtpdevice_t *device, uint32_t const id,
			 char const * const path, LIBMTP_progressfunc_t const callback,
			 void const * const data)
{
  int fd = -1;
  struct utimbuf mtime;
  int ret;

  
  if (path == NULL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_File_To_File(): Bad arguments, path was NULL.");
    return -1;
  }

  
#ifdef __WIN32__
#ifdef USE_WINDOWS_IO_H
  if ( (fd = _open(path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY,_S_IREAD)) == -1 ) {
#else
  if ( (fd = open(path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY,S_IRWXU)) == -1 ) {
#endif
#else
  if ( (fd = open(path, O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRGRP)) == -1) {
#endif
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_File_To_File(): Could not create file.");
    return -1;
  }

  ret = LIBMTP_Get_File_To_File_Descriptor(device, id, fd, callback, data, &mtime);

  
  close(fd);

  
  if (ret == -1) {
    unlink(path);
  } else {
      utime(path, &mtime);
  }
  return ret;
}

int LIBMTP_Get_File_To_File_Descriptor(LIBMTP_mtpdevice_t *device,
					uint32_t const id,
					int const fd,
					LIBMTP_progressfunc_t const callback,
					void const * const data,
                    struct utimbuf * mtime)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  PTPObject *ob;

  ret = ptp_object_want (params, id, PTPOBJECT_OBJECTINFO_LOADED, &ob);
  if (ret != PTP_RC_OK) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_File_To_File_Descriptor(): Could not get object info.");
    return -1;
  }
  if (ob->oi.ObjectFormat == PTP_OFC_Association) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_File_To_File_Descriptor(): Bad object format.");
    return -1;
  }

  if (mtime != NULL) {
    mtime->actime = ob->oi.CaptureDate;
    mtime->modtime = ob->oi.ModificationDate;
  }

  
  ptp_usb->callback_active = 1;
  ptp_usb->current_transfer_total = ob->oi.ObjectCompressedSize+
    PTP_USB_BULK_HDR_LEN+sizeof(uint32_t); 
  ptp_usb->current_transfer_complete = 0;
  ptp_usb->current_transfer_callback = callback;
  ptp_usb->current_transfer_callback_data = data;

  ret = ptp_getobject_tofd(params, id, fd);

  ptp_usb->callback_active = 0;
  ptp_usb->current_transfer_callback = NULL;
  ptp_usb->current_transfer_callback_data = NULL;

  if (ret == PTP_ERROR_CANCEL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_CANCELLED, "LIBMTP_Get_File_From_File_Descriptor(): Cancelled transfer.");
    return -1;
  }
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_File_To_File_Descriptor(): Could not get file from device.");
    return -1;
  }

  return 0;
}

int LIBMTP_Get_File_To_Handler(LIBMTP_mtpdevice_t *device,
					uint32_t const id,
					MTPDataPutFunc put_func,
          void * priv,
					LIBMTP_progressfunc_t const callback,
					void const * const data)
{
  PTPObject *ob;
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  ret = ptp_object_want (params, id, PTPOBJECT_OBJECTINFO_LOADED, &ob);
  if (ret != PTP_RC_OK) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_File_To_File_Descriptor(): Could not get object info.");
    return -1;
  }
  if (ob->oi.ObjectFormat == PTP_OFC_Association) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_File_To_File_Descriptor(): Bad object format.");
    return -1;
  }

  
  ptp_usb->callback_active = 1;
  ptp_usb->current_transfer_total = ob->oi.ObjectCompressedSize+
    PTP_USB_BULK_HDR_LEN+sizeof(uint32_t); 
  ptp_usb->current_transfer_complete = 0;
  ptp_usb->current_transfer_callback = callback;
  ptp_usb->current_transfer_callback_data = data;

  MTPDataHandler mtp_handler;
  mtp_handler.getfunc = NULL;
  mtp_handler.putfunc = put_func;
  mtp_handler.priv = priv;

  PTPDataHandler handler;
  handler.getfunc = NULL;
  handler.putfunc = put_func_wrapper;
  handler.priv = &mtp_handler;

  ret = ptp_getobject_to_handler(params, id, &handler);

  ptp_usb->callback_active = 0;
  ptp_usb->current_transfer_callback = NULL;
  ptp_usb->current_transfer_callback_data = NULL;

  if (ret == PTP_ERROR_CANCEL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_CANCELLED, "LIBMTP_Get_File_From_File_Descriptor(): Cancelled transfer.");
    return -1;
  }
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_File_To_File_Descriptor(): Could not get file from device.");
    return -1;
  }

  return 0;
}


int LIBMTP_Get_Track_To_File(LIBMTP_mtpdevice_t *device, uint32_t const id,
			 char const * const path, LIBMTP_progressfunc_t const callback,
			 void const * const data)
{
  
  return LIBMTP_Get_File_To_File(device, id, path, callback, data);
}

int LIBMTP_Get_Track_To_File_Descriptor(LIBMTP_mtpdevice_t *device,
					uint32_t const id,
					int const fd,
					LIBMTP_progressfunc_t const callback,
					void const * const data,
                    struct utimbuf * mtime)
{
  
  return LIBMTP_Get_File_To_File_Descriptor(device, id, fd, callback, data, mtime);
}

int LIBMTP_Get_Track_To_Handler(LIBMTP_mtpdevice_t *device,
					uint32_t const id,
					MTPDataPutFunc put_func,
          void * priv,
					LIBMTP_progressfunc_t const callback,
					void const * const data)
{
  
  return LIBMTP_Get_File_To_Handler(device, id, put_func, priv, callback, data);
}

/**
 * This function sends a track from a local file to an
 * MTP device. A filename and a set of metadata must be
 * given as input.
 * @param device a pointer to the device to send the track to.
 * @param path the filename of a local file which will be sent.
 * @param metadata a track metadata set to be written along with the file.
 *        After this call the field <code>metadata-&gt;item_id</code>
 *        will contain the new track ID. Other fields such
 *        as the <code>metadata-&gt;filename</code>, <code>metadata-&gt;parent_id</code>
 *        or <code>metadata-&gt;storage_id</code> may also change during this 
 *        operation due to device restrictions, so do not rely on the
 *        contents of this struct to be preserved in any way.
 *        <ul>
 *        <li><code>metadata-&gt;parent_id</code> should be set to the parent 
 *        (e.g. folder) to store this track in. Since some 
 *        devices are a bit picky about where files
 *        are placed, a default folder will be chosen if libmtp
 *        has detected one for the current filetype and this
 *        parameter is set to 0. If this is 0 and no default folder
 *        can be found, the file will be stored in the root folder.
 *        <li><code>metadata-&gt;storage_id</code> should be set to the
 *        desired storage (e.g. memory card or whatever your device
 *        presents) to store this track in. Setting this to 0 will store
 *        the track on the primary storage.
 *        </ul>
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Send_Track_From_File_Descriptor()
 * @see LIBMTP_Send_File_From_File()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Send_Track_From_File(LIBMTP_mtpdevice_t *device,
			 char const * const path, LIBMTP_track_t * const metadata,
                         LIBMTP_progressfunc_t const callback,
			 void const * const data)
{
  int fd;
  int ret;

  
  if (path == NULL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_Track_From_File(): Bad arguments, path was NULL.");
    return -1;
  }

  
#ifdef __WIN32__
#ifdef USE_WINDOWS_IO_H
  if ( (fd = _open(path, O_RDONLY|O_BINARY) == -1) ) {
#else
  if ( (fd = open(path, O_RDONLY|O_BINARY) == -1) ) {
#endif
#else
  if ( (fd = open(path, O_RDONLY)) == -1) {
#endif
    printf("LIBMTP_Send_Track_From_File(): Could not open source file \"%s\"\n", path);
    return -1;
  }

  ret = LIBMTP_Send_Track_From_File_Descriptor(device, fd, metadata, callback, data);

  
#ifdef USE_WINDOWS_IO_H
  _close(fd);
#else
  close(fd);
#endif

  return ret;
}

/**
 * This function sends a track from a file descriptor to an
 * MTP device. A filename and a set of metadata must be
 * given as input.
 * @param device a pointer to the device to send the track to.
 * @param fd the filedescriptor for a local file which will be sent.
 * @param metadata a track metadata set to be written along with the file.
 *        After this call the field <code>metadata-&gt;item_id</code>
 *        will contain the new track ID. Other fields such
 *        as the <code>metadata-&gt;filename</code>, <code>metadata-&gt;parent_id</code>
 *        or <code>metadata-&gt;storage_id</code> may also change during this 
 *        operation due to device restrictions, so do not rely on the
 *        contents of this struct to be preserved in any way.
 *        <ul>
 *        <li><code>metadata-&gt;parent_id</code> should be set to the parent 
 *        (e.g. folder) to store this track in. Since some 
 *        devices are a bit picky about where files
 *        are placed, a default folder will be chosen if libmtp
 *        has detected one for the current filetype and this
 *        parameter is set to 0. If this is 0 and no default folder
 *        can be found, the file will be stored in the root folder.
 *        <li><code>metadata-&gt;storage_id</code> should be set to the
 *        desired storage (e.g. memory card or whatever your device
 *        presents) to store this track in. Setting this to 0 will store
 *        the track on the primary storage.
 *        </ul>
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Send_Track_From_File()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Send_Track_From_File_Descriptor(LIBMTP_mtpdevice_t *device,
			 int const fd, LIBMTP_track_t * const metadata,
                         LIBMTP_progressfunc_t const callback,
			 void const * const data)
{
  int subcall_ret;
  LIBMTP_file_t filedata;

  
  if (!LIBMTP_FILETYPE_IS_TRACK(metadata->filetype)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, 
			    "LIBMTP_Send_Track_From_File_Descriptor(): "
			    "I don't think this is actually a track, strange filetype...");
  }

  
  filedata.item_id = metadata->item_id;
  filedata.parent_id = metadata->parent_id;
  filedata.storage_id = metadata->storage_id;
  filedata.filename = metadata->filename;
  filedata.filesize = metadata->filesize;
  filedata.filetype = metadata->filetype;
  filedata.next = NULL;

  subcall_ret = LIBMTP_Send_File_From_File_Descriptor(device,
						      fd, 
						      &filedata,
						      callback,
						      data);

  if (subcall_ret != 0) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, 
			    "LIBMTP_Send_Track_From_File_Descriptor(): "
			    "subcall to LIBMTP_Send_File_From_File_Descriptor failed.");
    
    
    return -1;
  }
  
  
  metadata->item_id = filedata.item_id;
  metadata->parent_id = filedata.parent_id;
  metadata->storage_id = filedata.storage_id;

  
  subcall_ret = LIBMTP_Update_Track_Metadata(device, metadata);
  if (subcall_ret != 0) {
    
    
    
    return -1;
  }

  
  

  return 0;
}

/**
 * This function sends a track from a handler function to an
 * MTP device. A filename and a set of metadata must be
 * given as input.
 * @param device a pointer to the device to send the track to.
 * @param get_func the function to call when we have data.
 * @param priv the user-defined pointer that is passed to
 *             <code>get_func</code>.
 * @param metadata a track metadata set to be written along with the file.
 *        After this call the field <code>metadata-&gt;item_id</code>
 *        will contain the new track ID. Other fields such
 *        as the <code>metadata-&gt;filename</code>, <code>metadata-&gt;parent_id</code>
 *        or <code>metadata-&gt;storage_id</code> may also change during this 
 *        operation due to device restrictions, so do not rely on the
 *        contents of this struct to be preserved in any way.
 *        <ul>
 *        <li><code>metadata-&gt;parent_id</code> should be set to the parent 
 *        (e.g. folder) to store this track in. Since some 
 *        devices are a bit picky about where files
 *        are placed, a default folder will be chosen if libmtp
 *        has detected one for the current filetype and this
 *        parameter is set to 0. If this is 0 and no default folder
 *        can be found, the file will be stored in the root folder.
 *        <li><code>metadata-&gt;storage_id</code> should be set to the
 *        desired storage (e.g. memory card or whatever your device
 *        presents) to store this track in. Setting this to 0 will store
 *        the track on the primary storage.
 *        </ul>
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Send_Track_From_File()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Send_Track_From_Handler(LIBMTP_mtpdevice_t *device,
			 MTPDataGetFunc get_func, void * priv, LIBMTP_track_t * const metadata,
                         LIBMTP_progressfunc_t const callback,
			 void const * const data)
{
  int subcall_ret;
  LIBMTP_file_t filedata;

  
  if (!LIBMTP_FILETYPE_IS_TRACK(metadata->filetype)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, 
			    "LIBMTP_Send_Track_From_Handler(): "
			    "I don't think this is actually a track, strange filetype...");
  }

  
  filedata.item_id = metadata->item_id;
  filedata.parent_id = metadata->parent_id;
  filedata.storage_id = metadata->storage_id;
  filedata.filename = metadata->filename;
  filedata.filesize = metadata->filesize;
  filedata.filetype = metadata->filetype;
  filedata.next = NULL;

  subcall_ret = LIBMTP_Send_File_From_Handler(device,
						      get_func,
                  priv, 
						      &filedata,
						      callback,
						      data);

  if (subcall_ret != 0) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, 
			    "LIBMTP_Send_Track_From_Handler(): "
			    "subcall to LIBMTP_Send_File_From_Handler failed.");
    
    
    return -1;
  }

  
  metadata->item_id = filedata.item_id;
  metadata->parent_id = filedata.parent_id;
  metadata->storage_id = filedata.storage_id;

  
  subcall_ret = LIBMTP_Update_Track_Metadata(device, metadata);
  if (subcall_ret != 0) {
    
    
    
    return -1;
  }

  
  

  return 0;
}

/**
 * This function sends a local file to an MTP device.
 * A filename and a set of metadata must be
 * given as input.
 * @param device a pointer to the device to send the track to.
 * @param path the filename of a local file which will be sent.
 * @param filedata a file metadata set to be written along with the file.
 *        After this call the field <code>filedata-&gt;item_id</code>
 *        will contain the new file ID. Other fields such
 *        as the <code>filedata-&gt;filename</code>, <code>filedata-&gt;parent_id</code>
 *        or <code>filedata-&gt;storage_id</code> may also change during this
 *        operation due to device restrictions, so do not rely on the
 *        contents of this struct to be preserved in any way.
 *        <ul>
 *        <li><code>filedata-&gt;parent_id</code> should be set to the parent
 *        (e.g. folder) to store this file in. If this is 0,
 *        the file will be stored in the root folder.
 *        <li><code>filedata-&gt;storage_id</code> should be set to the
 *        desired storage (e.g. memory card or whatever your device
 *        presents) to store this file in. Setting this to 0 will store
 *        the file on the primary storage.
 *        </ul>
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Send_File_From_File_Descriptor()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Send_File_From_File(LIBMTP_mtpdevice_t *device,
			       char const * const path, LIBMTP_file_t * const filedata,
			       LIBMTP_progressfunc_t const callback,
			       void const * const data)
{
  int fd;
  int ret;

  
  if (path == NULL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_File_From_File(): Bad arguments, path was NULL.");
    return -1;
  }

  
#ifdef __WIN32__
#ifdef USE_WINDOWS_IO_H
  if ( (fd = _open(path, O_RDONLY|O_BINARY) == -1) ) {
#else
  if ( (fd = open(path, O_RDONLY|O_BINARY) == -1) ) {
#endif
#else
  if ( (fd = open(path, O_RDONLY)) == -1) {
#endif
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_File_From_File(): Could not open source file.");
    return -1;
  }

  ret = LIBMTP_Send_File_From_File_Descriptor(device, fd, filedata, callback, data);

  
#ifdef USE_WINDOWS_IO_H
  _close(fd);
#else
  close(fd);
#endif

  return ret;
}

/**
 * This function sends a generic file from a file descriptor to an
 * MTP device. A filename and a set of metadata must be
 * given as input.
 *
 * This can potentially be used for sending in a stream of unknown
 * length. Send music files with
 * <code>LIBMTP_Send_Track_From_File_Descriptor()</code>
 *
 * @param device a pointer to the device to send the file to.
 * @param fd the filedescriptor for a local file which will be sent.
 * @param filedata a file metadata set to be written along with the file.
 *        After this call the field <code>filedata-&gt;item_id</code>
 *        will contain the new file ID. Other fields such
 *        as the <code>filedata-&gt;filename</code>, <code>filedata-&gt;parent_id</code>
 *        or <code>filedata-&gt;storage_id</code> may also change during this
 *        operation due to device restrictions, so do not rely on the
 *        contents of this struct to be preserved in any way.
 *        <ul>
 *        <li><code>filedata-&gt;parent_id</code> should be set to the parent
 *        (e.g. folder) to store this file in. If this is 0,
 *        the file will be stored in the root folder.
 *        <li><code>filedata-&gt;storage_id</code> should be set to the
 *        desired storage (e.g. memory card or whatever your device
 *        presents) to store this file in. Setting this to 0 will store
 *        the file on the primary storage.
 *        </ul>
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Send_File_From_File()
 * @see LIBMTP_Send_Track_From_File_Descriptor()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Send_File_From_File_Descriptor(LIBMTP_mtpdevice_t *device,
			 int const fd, LIBMTP_file_t * const filedata,
                         LIBMTP_progressfunc_t const callback,
			 void const * const data)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  LIBMTP_file_t *newfilemeta;

  if (send_file_object_info(device, filedata))
  {
    
    return -1;
  }

  
  ptp_usb->callback_active = 1;
  
  
  ptp_usb->current_transfer_total = filedata->filesize+PTP_USB_BULK_HDR_LEN*2;
  ptp_usb->current_transfer_complete = 0;
  ptp_usb->current_transfer_callback = callback;
  ptp_usb->current_transfer_callback_data = data;

  ret = ptp_sendobject_fromfd(params, fd, filedata->filesize);

  ptp_usb->callback_active = 0;
  ptp_usb->current_transfer_callback = NULL;
  ptp_usb->current_transfer_callback_data = NULL;

  if (ret == PTP_ERROR_CANCEL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_CANCELLED, "LIBMTP_Send_File_From_File_Descriptor(): Cancelled transfer.");
    return -1;
  }
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_File_From_File_Descriptor(): "
				"Could not send object.");
    return -1;
  }

  add_object_to_cache(device, filedata->item_id);

  newfilemeta = LIBMTP_Get_Filemetadata(device, filedata->item_id);
  if (newfilemeta != NULL) {
    filedata->parent_id = newfilemeta->parent_id;
    filedata->storage_id = newfilemeta->storage_id;
    LIBMTP_destroy_file_t(newfilemeta);
  } else {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL,
			    "LIBMTP_Send_File_From_File_Descriptor(): "
			    "Could not retrieve updated metadata.");
    return -1;
  }

  return 0;
}

/**
 * This function sends a generic file from a handler function to an
 * MTP device. A filename and a set of metadata must be
 * given as input.
 *
 * This can potentially be used for sending in a stream of unknown
 * length. Send music files with
 * <code>LIBMTP_Send_Track_From_Handler()</code>
 *
 * @param device a pointer to the device to send the file to.
 * @param get_func the function to call to get data to write
 * @param priv a user-defined pointer that is passed along to
 *        <code>get_func</code>. If not used, this is set to NULL.
 * @param filedata a file metadata set to be written along with the file.
 *        After this call the field <code>filedata-&gt;item_id</code>
 *        will contain the new file ID. Other fields such
 *        as the <code>filedata-&gt;filename</code>, <code>filedata-&gt;parent_id</code>
 *        or <code>filedata-&gt;storage_id</code> may also change during this
 *        operation due to device restrictions, so do not rely on the
 *        contents of this struct to be preserved in any way.
 *        <ul>
 *        <li><code>filedata-&gt;parent_id</code> should be set to the parent
 *        (e.g. folder) to store this file in. If this is 0,
 *        the file will be stored in the root folder.
 *        <li><code>filedata-&gt;storage_id</code> should be set to the
 *        desired storage (e.g. memory card or whatever your device
 *        presents) to store this file in. Setting this to 0 will store
 *        the file on the primary storage.
 *        </ul>
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Send_File_From_File()
 * @see LIBMTP_Send_Track_From_File_Descriptor()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Send_File_From_Handler(LIBMTP_mtpdevice_t *device,
			 MTPDataGetFunc get_func, void * priv, LIBMTP_file_t * const filedata,
       LIBMTP_progressfunc_t const callback, void const * const data)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  LIBMTP_file_t *newfilemeta;

  if (send_file_object_info(device, filedata))
  {
    
    return -1;
  }

  
  ptp_usb->callback_active = 1;
  
  
  ptp_usb->current_transfer_total = filedata->filesize+PTP_USB_BULK_HDR_LEN*2;
  ptp_usb->current_transfer_complete = 0;
  ptp_usb->current_transfer_callback = callback;
  ptp_usb->current_transfer_callback_data = data;

  MTPDataHandler mtp_handler;
  mtp_handler.getfunc = get_func;
  mtp_handler.putfunc = NULL;
  mtp_handler.priv = priv;

  PTPDataHandler handler;
  handler.getfunc = get_func_wrapper;
  handler.putfunc = NULL;
  handler.priv = &mtp_handler;

  ret = ptp_sendobject_from_handler(params, &handler, filedata->filesize);

  ptp_usb->callback_active = 0;
  ptp_usb->current_transfer_callback = NULL;
  ptp_usb->current_transfer_callback_data = NULL;

  if (ret == PTP_ERROR_CANCEL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_CANCELLED, "LIBMTP_Send_File_From_Handler(): Cancelled transfer.");
    return -1;
  }
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_File_From_Handler(): "
				"Could not send object.");
    return -1;
  }

  add_object_to_cache(device, filedata->item_id);

  newfilemeta = LIBMTP_Get_Filemetadata(device, filedata->item_id);
  if (newfilemeta != NULL) {
    filedata->parent_id = newfilemeta->parent_id;
    filedata->storage_id = newfilemeta->storage_id;
    LIBMTP_destroy_file_t(newfilemeta);
  } else {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL,
			    "LIBMTP_Send_File_From_Handler(): "
			    "Could not retrieve updated metadata.");
    return -1;
  }

  return 0;
}

/**
 * This function sends the file object info, ready for sendobject
 * @param device a pointer to the device to send the file to.
 * @param filedata a file metadata set to be written along with the file.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 */
static int send_file_object_info(LIBMTP_mtpdevice_t *device, LIBMTP_file_t *filedata)
{
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint32_t store;

#ifdef _AFT_BUILD
  int use_primary_storage = 0;
#else
  int use_primary_storage = 1;
#endif

  uint16_t of = map_libmtp_type_to_ptp_type(filedata->filetype);
  LIBMTP_devicestorage_t *storage;
  uint32_t localph = filedata->parent_id;
  uint16_t ret;
  int i;

  if (filedata->storage_id != 0) {
    store = filedata->storage_id;
  } else {
    store = get_writeable_storageid(device, filedata->filesize);
  }
  
  storage = device->storage;
  if (storage != NULL && store != storage->id) {
    use_primary_storage = 0;
  }


  if (localph == 0 && use_primary_storage) {
    if (LIBMTP_FILETYPE_IS_AUDIO(filedata->filetype)) {
      localph = device->default_music_folder;
    } else if (LIBMTP_FILETYPE_IS_VIDEO(filedata->filetype)) {
      localph = device->default_video_folder;
    } else if (of == PTP_OFC_EXIF_JPEG ||
	       of == PTP_OFC_JP2 ||
	       of == PTP_OFC_JPX ||
	       of == PTP_OFC_JFIF ||
	       of == PTP_OFC_TIFF ||
	       of == PTP_OFC_TIFF_IT ||
	       of == PTP_OFC_BMP ||
	       of == PTP_OFC_GIF ||
	       of == PTP_OFC_PICT ||
	       of == PTP_OFC_PNG ||
	       of == PTP_OFC_MTP_WindowsImageFormat) {
      localph = device->default_picture_folder;
    } else if (of == PTP_OFC_MTP_vCalendar1 ||
	       of == PTP_OFC_MTP_vCalendar2 ||
	       of == PTP_OFC_MTP_UndefinedContact ||
	       of == PTP_OFC_MTP_vCard2 ||
	       of == PTP_OFC_MTP_vCard3 ||
	       of == PTP_OFC_MTP_UndefinedCalendarItem) {
      localph = device->default_organizer_folder;
    } else if (of == PTP_OFC_Text) {
      localph = device->default_text_folder;
    }
  }

  
  if (localph == 0) {
    localph = 0xFFFFFFFFU; 
  }

  
  
  if (FLAG_OGG_IS_UNKNOWN(ptp_usb) && of == PTP_OFC_MTP_OGG) {
    of = PTP_OFC_Undefined;
  }
  if (FLAG_FLAC_IS_UNKNOWN(ptp_usb) && of == PTP_OFC_MTP_FLAC) {
    of = PTP_OFC_Undefined;
  }

  if (ptp_operation_issupported(params, PTP_OC_MTP_SendObjectPropList) &&
      !FLAG_BROKEN_SEND_OBJECT_PROPLIST(ptp_usb)) {
    MTPProperties *props = NULL;
    int nrofprops = 0;
    MTPProperties *prop = NULL;
    uint16_t *properties = NULL;
    uint32_t propcnt = 0;

    
    filedata->item_id = 0x00000000U;

    ret = ptp_mtp_getobjectpropssupported(params, of, &propcnt, &properties);

    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;

      ret = ptp_mtp_getobjectpropdesc(params, properties[i], of, &opd);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "send_file_object_info(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (properties[i]) {
	case PTP_OPC_ObjectFileName:
	  prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	  prop->ObjectHandle = filedata->item_id;
	  prop->property = PTP_OPC_ObjectFileName;
	  prop->datatype = PTP_DTC_STR;
	  if (filedata->filename != NULL) {
	    prop->propval.str = strdup(filedata->filename);
	    if (FLAG_ONLY_7BIT_FILENAMES(ptp_usb)) {
	      strip_7bit_from_utf8(prop->propval.str);
	    }
	  }
	  break;
	case PTP_OPC_ProtectionStatus:
	  prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	  prop->ObjectHandle = filedata->item_id;
	  prop->property = PTP_OPC_ProtectionStatus;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = 0x0000U; 
	  break;
	case PTP_OPC_NonConsumable:
	  prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	  prop->ObjectHandle = filedata->item_id;
	  prop->property = PTP_OPC_NonConsumable;
	  prop->datatype = PTP_DTC_UINT8;
	  prop->propval.u8 = 0x00; 
	  break;
	case PTP_OPC_Name:
	  prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	  prop->ObjectHandle = filedata->item_id;
	  prop->property = PTP_OPC_Name;
	  prop->datatype = PTP_DTC_STR;
	  if (filedata->filename != NULL)
	    prop->propval.str = strdup(filedata->filename);
	  break;
	case PTP_OPC_DateModified:
	  
	  if (!FLAG_CANNOT_HANDLE_DATEMODIFIED(ptp_usb)) {
	    prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	    prop->ObjectHandle = filedata->item_id;
	    prop->property = PTP_OPC_DateModified;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = get_iso8601_stamp();
	    filedata->modificationdate = time(NULL);
	  }
	  break;
	}
      }
      ptp_free_objectpropdesc(&opd);
    }
    free(properties);

    ret = ptp_mtp_sendobjectproplist(params, &store, &localph, &filedata->item_id,
				     of, filedata->filesize, props, nrofprops);

    
    ptp_destroy_object_prop_list(props, nrofprops);

    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "send_file_object_info():"
				  "Could not send object property list.");
      if (ret == PTP_RC_AccessDenied) {
	add_ptp_error_to_errorstack(device, ret, "ACCESS DENIED.");
      }
      return -1;
    }
  } else if (ptp_operation_issupported(params,PTP_OC_SendObjectInfo)) {
    PTPObjectInfo new_file;

    memset(&new_file, 0, sizeof(PTPObjectInfo));

    new_file.Filename = filedata->filename;
    if (FLAG_ONLY_7BIT_FILENAMES(ptp_usb)) {
      strip_7bit_from_utf8(new_file.Filename);
    }
    
    new_file.ObjectCompressedSize = (uint32_t) filedata->filesize;
    new_file.ObjectFormat = of;
    new_file.StorageID = store;
    new_file.ParentObject = localph;
    new_file.ModificationDate = time(NULL);
    

    ret = ptp_sendobjectinfo(params, &store, &localph, &filedata->item_id, &new_file);

    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "send_file_object_info(): "
				  "Could not send object info.");
      if (ret == PTP_RC_AccessDenied) {
	add_ptp_error_to_errorstack(device, ret, "ACCESS DENIED.");
      }
      return -1;
    }
    
    
  }

  
  filedata->parent_id = localph;

  return 0;
}

/**
 * This function updates the MTP track object metadata on a
 * single file identified by an object ID.
 * @param device a pointer to the device to update the track
 *        metadata on.
 * @param metadata a track metadata set to be written to the file.
 *        notice that the <code>track_id</code> field of the
 *        metadata structure must be correct so that the
 *        function can update the right file. If some properties
 *        of this metadata are set to NULL (strings) or 0
 *        (numerical values) they will be discarded and the
 *        track will not be tagged with these blank values.
 * @return 0 on success, any other value means failure. If some
 *        or all of the properties fail to update we will still
 *        return success. On some devices (notably iRiver T30)
 *        properties that exist cannot be updated.
 */
int LIBMTP_Update_Track_Metadata(LIBMTP_mtpdevice_t *device,
				 LIBMTP_track_t const * const metadata)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint32_t i;
  uint16_t *properties = NULL;
  uint32_t propcnt = 0;

  
  
  ret = ptp_mtp_getobjectpropssupported(params, map_libmtp_type_to_ptp_type(metadata->filetype), &propcnt, &properties);
  if (ret != PTP_RC_OK) {
    
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
			    "could not retrieve supported object properties.");
    return -1;
  }
  if (ptp_operation_issupported(params, PTP_OC_MTP_SetObjPropList) &&
      !FLAG_BROKEN_SET_OBJECT_PROPLIST(ptp_usb)) {
    MTPProperties *props = NULL;
    MTPProperties *prop = NULL;
    int nrofprops = 0;

    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;

      ret = ptp_mtp_getobjectpropdesc(params, properties[i], map_libmtp_type_to_ptp_type(metadata->filetype), &opd);
      if (ret != PTP_RC_OK) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (properties[i]) {
	case PTP_OPC_Name:
	  if (metadata->title == NULL)
	    break;
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_Name;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->title);
	  break;
	case PTP_OPC_AlbumName:
	  if (metadata->album == NULL)
	    break;
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_AlbumName;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->album);
	  break;
	case PTP_OPC_Artist:
	  if (metadata->artist == NULL)
	    break;
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_Artist;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->artist);
	  break;
	case PTP_OPC_Composer:
	  if (metadata->composer == NULL)
	    break;
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_Composer;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->composer);
	  break;
	case PTP_OPC_Genre:
	  if (metadata->genre == NULL)
	    break;
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_Genre;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->genre);
	  break;
	case PTP_OPC_Duration:
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_Duration;
	  prop->datatype = PTP_DTC_UINT32;
	  prop->propval.u32 = adjust_u32(metadata->duration, &opd);
	  break;
	case PTP_OPC_Track:
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_Track;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = adjust_u16(metadata->tracknumber, &opd);
	  break;
	case PTP_OPC_OriginalReleaseDate:
	  if (metadata->date == NULL)
	    break;
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_OriginalReleaseDate;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->date);
	  break;
	case PTP_OPC_SampleRate:
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_SampleRate;
	  prop->datatype = PTP_DTC_UINT32;
	  prop->propval.u32 = adjust_u32(metadata->samplerate, &opd);
	  break;
	case PTP_OPC_NumberOfChannels:
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_NumberOfChannels;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = adjust_u16(metadata->nochannels, &opd);
	  break;
	case PTP_OPC_AudioWAVECodec:
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_AudioWAVECodec;
	  prop->datatype = PTP_DTC_UINT32;
	  prop->propval.u32 = adjust_u32(metadata->wavecodec, &opd);
	  break;
	case PTP_OPC_AudioBitRate:
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_AudioBitRate;
	  prop->datatype = PTP_DTC_UINT32;
	  prop->propval.u32 = adjust_u32(metadata->bitrate, &opd);
	  break;
	case PTP_OPC_BitRateType:
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_BitRateType;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = adjust_u16(metadata->bitratetype, &opd);
	  break;
	case PTP_OPC_Rating:
	  
	  if (metadata->rating == 0)
	    break;
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_Rating;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = adjust_u16(metadata->rating, &opd);
	  break;
	case PTP_OPC_UseCount:
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_UseCount;
	  prop->datatype = PTP_DTC_UINT32;
	  prop->propval.u32 = adjust_u32(metadata->usecount, &opd);
	  break;
	case PTP_OPC_DateModified:
	  if (!FLAG_CANNOT_HANDLE_DATEMODIFIED(ptp_usb)) {
	    
	    prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	    prop->ObjectHandle = metadata->item_id;
	    prop->property = PTP_OPC_DateModified;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = get_iso8601_stamp();
	  }
	  break;
	default:
	  break;
	}
      }
      ptp_free_objectpropdesc(&opd);
    }

    
    

    ret = ptp_mtp_setobjectproplist(params, props, nrofprops);

    ptp_destroy_object_prop_list(props, nrofprops);

    if (ret != PTP_RC_OK) {
      
      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
			      "could not set object property list.");
      free(properties);
      return -1;
    }

  } else if (ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;

      ret = ptp_mtp_getobjectpropdesc(params, properties[i], map_libmtp_type_to_ptp_type(metadata->filetype), &opd);
      if (ret != PTP_RC_OK) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (properties[i]) {
	case PTP_OPC_Name:
	  
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_Name, metadata->title);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track title.");
	  }
	  break;
	case PTP_OPC_AlbumName:
	  
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_AlbumName, metadata->album);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track album name.");
	  }
	  break;
	case PTP_OPC_Artist:
	  
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_Artist, metadata->artist);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track artist name.");
	  }
	  break;
	case PTP_OPC_Composer:
	  
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_Composer, metadata->composer);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track composer name.");
	  }
	  break;
	case PTP_OPC_Genre:
	  
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_Genre, metadata->genre);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track genre name.");
	  }
	  break;
	case PTP_OPC_Duration:
	  
	  if (metadata->duration != 0) {
	    ret = set_object_u32(device, metadata->item_id, PTP_OPC_Duration, adjust_u32(metadata->duration, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set track duration.");
	    }
	  }
	  break;
	case PTP_OPC_Track:
	  
	  if (metadata->tracknumber != 0) {
	    ret = set_object_u16(device, metadata->item_id, PTP_OPC_Track, adjust_u16(metadata->tracknumber, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set track tracknumber.");
	    }
	  }
	  break;
	case PTP_OPC_OriginalReleaseDate:
	  
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_OriginalReleaseDate, metadata->date);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track release date.");
	  }
	  break;
	  
	case PTP_OPC_SampleRate:
	  
	  if (metadata->samplerate != 0) {
	    ret = set_object_u32(device, metadata->item_id, PTP_OPC_SampleRate, adjust_u32(metadata->samplerate, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set samplerate.");
	    }
	  }
	  break;
	case PTP_OPC_NumberOfChannels:
	  
	  if (metadata->nochannels != 0) {
	    ret = set_object_u16(device, metadata->item_id, PTP_OPC_NumberOfChannels, adjust_u16(metadata->nochannels, &opd));
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set number of channels.");
	  }
	}
	  break;
	case PTP_OPC_AudioWAVECodec:
	  
	  if (metadata->wavecodec != 0) {
	    ret = set_object_u32(device, metadata->item_id, PTP_OPC_AudioWAVECodec, adjust_u32(metadata->wavecodec, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set WAVE codec.");
	    }
	  }
	  break;
	case PTP_OPC_AudioBitRate:
	  
	  if (metadata->bitrate != 0) {
	    ret = set_object_u32(device, metadata->item_id, PTP_OPC_AudioBitRate, adjust_u32(metadata->bitrate, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set bitrate.");
	  }
	  }
	  break;
	case PTP_OPC_BitRateType:
	  
	  if (metadata->bitratetype != 0) {
	    ret = set_object_u16(device, metadata->item_id, PTP_OPC_BitRateType, adjust_u16(metadata->bitratetype, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set bitratetype.");
	    }
	  }
	  break;
	case PTP_OPC_Rating:
	  
	  
	  if (metadata->rating != 0) {
	    ret = set_object_u16(device, metadata->item_id, PTP_OPC_Rating, adjust_u16(metadata->rating, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set user rating.");
	    }
	  }
	  break;
	case PTP_OPC_UseCount:
	  
	  ret = set_object_u32(device, metadata->item_id, PTP_OPC_UseCount, adjust_u32(metadata->usecount, &opd));
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				  "could not set use count.");
	  }
	  break;
	case PTP_OPC_DateModified:
	  if (!FLAG_CANNOT_HANDLE_DATEMODIFIED(ptp_usb)) {
	    
	    char *tmpstamp = get_iso8601_stamp();
	    ret = set_object_string(device, metadata->item_id, PTP_OPC_DateModified, tmpstamp);
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set modification date.");
	    }
	    free(tmpstamp);
	  }
	  break;

	  
	  
	default:
	  break;
	}
      }
      ptp_free_objectpropdesc(&opd);
    }
  } else {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
                            "Your device doesn't seem to support any known way of setting metadata.");
    free(properties);
    return -1;
  }

  
  update_metadata_cache(device, metadata->item_id);

  free(properties);

  return 0;
}

int LIBMTP_Delete_Object(LIBMTP_mtpdevice_t *device,
			 uint32_t object_id)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;

  ret = ptp_deleteobject(params, object_id, 0);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Delete_Object(): could not delete object.");
    return -1;
  }

  return 0;
}

static int set_object_filename(LIBMTP_mtpdevice_t *device,
			       uint32_t object_id, uint16_t ptp_type,
			       const char **newname_ptr)
{
  PTPParams             *params = (PTPParams *) device->params;
  PTP_USB               *ptp_usb = (PTP_USB*) device->usbinfo;
  PTPObjectPropDesc     opd;
  uint16_t              ret;
  char                  *newname;

  
  ret = ptp_mtp_getobjectpropdesc(params, PTP_OPC_ObjectFileName, ptp_type, &opd);
  if (ret != PTP_RC_OK) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_filename(): "
			    "could not get property description.");
    return -1;
  }

  if (!opd.GetSet) {
    ptp_free_objectpropdesc(&opd);
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_filename(): "
            " property is not settable.");
    
    
    return -1;
  }

  newname = strdup(*newname_ptr);

  if (FLAG_ONLY_7BIT_FILENAMES(ptp_usb)) {
    strip_7bit_from_utf8(newname);
  }

  if (ptp_operation_issupported(params, PTP_OC_MTP_SetObjPropList) &&
      !FLAG_BROKEN_SET_OBJECT_PROPLIST(ptp_usb)) {
    MTPProperties *props = NULL;
    MTPProperties *prop = NULL;
    int nrofprops = 0;

    prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
    prop->ObjectHandle = object_id;
    prop->property = PTP_OPC_ObjectFileName;
    prop->datatype = PTP_DTC_STR;
    prop->propval.str = newname;

    ret = ptp_mtp_setobjectproplist(params, props, nrofprops);

    ptp_destroy_object_prop_list(props, nrofprops);

    if (ret != PTP_RC_OK) {
        add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_filename(): "
              " could not set object property list.");
        ptp_free_objectpropdesc(&opd);
        return -1;
    }
  } else if (ptp_operation_issupported(params, PTP_OC_MTP_SetObjectPropValue)) {
    ret = set_object_string(device, object_id, PTP_OPC_ObjectFileName, newname);
    if (ret != 0) {
      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_filename(): "
              " could not set object filename.");
      ptp_free_objectpropdesc(&opd);
      return -1;
    }
  } else {
    free(newname);
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_filename(): "
              " your device doesn't seem to support any known way of setting metadata.");
    ptp_free_objectpropdesc(&opd);
    return -1;
  }

  ptp_free_objectpropdesc(&opd);

  
  update_metadata_cache(device, object_id);

  return 0;
}

int LIBMTP_Set_File_Name(LIBMTP_mtpdevice_t *device,
                   LIBMTP_file_t *file, const char *newname)
{
  int         ret;

  ret = set_object_filename(device, file->item_id,
			    map_libmtp_type_to_ptp_type(file->filetype),
			    &newname);

  if (ret != 0) {
    return ret;
  }

  free(file->filename);
  file->filename = strdup(newname);
  return ret;
}

int LIBMTP_Set_Folder_Name(LIBMTP_mtpdevice_t *device,
                   LIBMTP_folder_t *folder, const char* newname)
{
  int ret;

  ret = set_object_filename(device, folder->folder_id,
			    PTP_OFC_Association,
			    &newname);

  if (ret != 0) {
    return ret;
    }

  free(folder->name);
  folder->name = strdup(newname);
  return ret;
}

int LIBMTP_Set_Track_Name(LIBMTP_mtpdevice_t *device,
                   LIBMTP_track_t *track, const char* newname)
{
  int         ret;

  ret = set_object_filename(device, track->item_id,
			    map_libmtp_type_to_ptp_type(track->filetype),
			    &newname);

  if (ret != 0) {
    return ret;
  }

  free(track->filename);
  track->filename = strdup(newname);
  return ret;
}

int LIBMTP_Set_Playlist_Name(LIBMTP_mtpdevice_t *device,
                   LIBMTP_playlist_t *playlist, const char* newname)
{
  int ret;

  ret = set_object_filename(device, playlist->playlist_id,
			    PTP_OFC_MTP_AbstractAudioVideoPlaylist,
			    &newname);

  if (ret != 0) {
    return ret;
  }

  free(playlist->name);
  playlist->name = strdup(newname);
  return ret;
}

int LIBMTP_Set_Album_Name(LIBMTP_mtpdevice_t *device,
                   LIBMTP_album_t *album, const char* newname)
{
  int ret;

  ret = set_object_filename(device, album->album_id,
			    PTP_OFC_MTP_AbstractAudioAlbum,
			    &newname);

  if (ret != 0) {
    return ret;
  }

  free(album->name);
  album->name = strdup(newname);
  return ret;
}

int LIBMTP_Set_Object_Filename(LIBMTP_mtpdevice_t *device,
                   uint32_t object_id, char* newname)
{
  int             ret;
  LIBMTP_file_t   *file;

  file = LIBMTP_Get_Filemetadata(device, object_id);

  if (file == NULL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Set_Object_Filename(): "
			    "could not get file metadata for target object.");
    return -1;
  }

  ret = set_object_filename(device, object_id, map_libmtp_type_to_ptp_type(file->filetype), (const char **) &newname);

  free(file);

  return ret;
}

int LIBMTP_Track_Exists(LIBMTP_mtpdevice_t *device,
           uint32_t const id)
{
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;
  PTPObject *ob;

  ret = ptp_object_want (params, id, 0, &ob);
  if (ret == PTP_RC_OK)
      return -1;
  return 0;
}

LIBMTP_folder_t *LIBMTP_new_folder_t(void)
{
  LIBMTP_folder_t *new = (LIBMTP_folder_t *) malloc(sizeof(LIBMTP_folder_t));
  if (new == NULL) {
    return NULL;
  }
  new->folder_id = 0;
  new->parent_id = 0;
  new->storage_id = 0;
  new->name = NULL;
  new->sibling = NULL;
  new->child = NULL;
  return new;
}

void LIBMTP_destroy_folder_t(LIBMTP_folder_t *folder)
{

  if(folder == NULL) {
     return;
  }

  
  if(folder->child != NULL) {
     LIBMTP_destroy_folder_t(folder->child);
  }

  if(folder->sibling != NULL) {
    LIBMTP_destroy_folder_t(folder->sibling);
  }

  if(folder->name != NULL) {
    free(folder->name);
  }

  free(folder);
}

LIBMTP_folder_t *LIBMTP_Find_Folder(LIBMTP_folder_t *folderlist, uint32_t id)
{
  LIBMTP_folder_t *ret = NULL;

  if(folderlist == NULL) {
    return NULL;
  }

  if(folderlist->folder_id == id) {
    return folderlist;
  }

  if(folderlist->sibling) {
    ret = LIBMTP_Find_Folder(folderlist->sibling, id);
  }

  if(folderlist->child && ret == NULL) {
    ret = LIBMTP_Find_Folder(folderlist->child, id);
  }

  return ret;
}

static LIBMTP_folder_t *get_subfolders_for_folder(LIBMTP_folder_t *list, uint32_t parent)
{
  LIBMTP_folder_t *retfolders = NULL, *children, *iter, *curr;

  iter = list->sibling;
  while(iter != list) {
    if (iter->parent_id != parent) {
      iter = iter->sibling;
      continue;
    }

    children = get_subfolders_for_folder(list, iter->folder_id);

    curr = iter;
    iter = iter->sibling;

    
    curr->child->sibling = curr->sibling;
    curr->sibling->child = curr->child;

    
    curr->child = children;

    
    curr->sibling = retfolders;
    retfolders = curr;
  }

  return retfolders;
}

LIBMTP_folder_t *LIBMTP_Get_Folder_List(LIBMTP_mtpdevice_t *device)
{
  PTPParams *params = (PTPParams *) device->params;
  LIBMTP_folder_t head, *rv;
  int i;

  
  if (params->nrofobjects == 0) {
    flush_handles(device);
  }

  head.sibling = &head;
  head.child = &head;
  for (i = 0; i < params->nrofobjects; i++) {
    LIBMTP_folder_t *folder;
    PTPObject *ob;

    ob = &params->objects[i];
    if (ob->oi.ObjectFormat != PTP_OFC_Association) {
      continue;
    }
    if (ob->oi.AssociationDesc != 0x00000000U) {
      printf("MTP extended association type 0x%08x encountered\n", ob->oi.AssociationDesc);
    }

    
    folder = LIBMTP_new_folder_t();
    if (folder == NULL) {
      
      return NULL;
    }
    folder->folder_id = ob->oid;
    folder->parent_id = ob->oi.ParentObject;
    folder->storage_id = ob->oi.StorageID;
    folder->name = (ob->oi.Filename) ? (char *)strdup(ob->oi.Filename) : NULL;

    
    folder->sibling = head.sibling;
    folder->child = &head;
    head.sibling->child = folder;
    head.sibling = folder;
  }

  
  rv = get_subfolders_for_folder(&head, 0x00000000);

  
  while(head.sibling != &head) {
    LIBMTP_folder_t *curr = head.sibling;

    printf("Orphan folder with ID: 0x%08x name: \"%s\" encountered.\n",
	   curr->folder_id,
	   curr->name);
    curr->sibling->child = curr->child;
    curr->child->sibling = curr->sibling;
    curr->child = NULL;
    curr->sibling = NULL;
    LIBMTP_destroy_folder_t(curr);
  }

  return rv;
}

uint32_t LIBMTP_Create_Folder(LIBMTP_mtpdevice_t *device, char *name,
			      uint32_t parent_id, uint32_t storage_id)
{
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint32_t parenthandle = 0;
  uint32_t store;
  PTPObjectInfo new_folder;
  uint16_t ret;
  uint32_t new_id = 0;

  if (storage_id == 0) {
    
    store = get_writeable_storageid(device, 512);
  } else {
    store = storage_id;
  }

  if (parent_id == 0) {
    parent_id = 0xFFFFFFFFU; 
  }

  parenthandle = parent_id;

  memset(&new_folder, 0, sizeof(new_folder));
  new_folder.Filename = name;
  if (FLAG_ONLY_7BIT_FILENAMES(ptp_usb)) {
    strip_7bit_from_utf8(new_folder.Filename);
  }
  new_folder.ObjectCompressedSize = 1;
  new_folder.ObjectFormat = PTP_OFC_Association;
  new_folder.ProtectionStatus = PTP_PS_NoProtection;
  new_folder.AssociationType = PTP_AT_GenericFolder;
  new_folder.ParentObject = parent_id;
  new_folder.StorageID = store;

  
  
  ret = ptp_sendobjectinfo(params, &store, &parenthandle, &new_id, &new_folder);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Create_Folder: Could not send object info.");
    if (ret == PTP_RC_AccessDenied) {
      add_ptp_error_to_errorstack(device, ret, "ACCESS DENIED.");
    }
    return 0;
  }
  
  

  add_object_to_cache(device, new_id);

  return new_id;
}

LIBMTP_playlist_t *LIBMTP_new_playlist_t(void)
{
  LIBMTP_playlist_t *new = (LIBMTP_playlist_t *) malloc(sizeof(LIBMTP_playlist_t));
  if (new == NULL) {
    return NULL;
  }
  new->playlist_id = 0;
  new->parent_id = 0;
  new->storage_id = 0;
  new->name = NULL;
  new->tracks = NULL;
  new->no_tracks = 0;
  new->next = NULL;
  return new;
}

void LIBMTP_destroy_playlist_t(LIBMTP_playlist_t *playlist)
{
  if (playlist == NULL) {
    return;
  }
  if (playlist->name != NULL)
    free(playlist->name);
  if (playlist->tracks != NULL)
    free(playlist->tracks);
  free(playlist);
  return;
}

LIBMTP_playlist_t *LIBMTP_Get_Playlist_List(LIBMTP_mtpdevice_t *device)
{
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  const int REQ_SPL = FLAG_PLAYLIST_SPL(ptp_usb);
  PTPParams *params = (PTPParams *) device->params;
  LIBMTP_playlist_t *retlists = NULL;
  LIBMTP_playlist_t *curlist = NULL;
  uint32_t i;

  
  if (params->nrofobjects == 0) {
    flush_handles(device);
  }

  for (i = 0; i < params->nrofobjects; i++) {
    LIBMTP_playlist_t *pl;
    PTPObject *ob;
    uint16_t ret;

    ob = &params->objects[i];

    

    
    
    if ( REQ_SPL && is_spl_playlist(&ob->oi) ) {
      
      pl = LIBMTP_new_playlist_t();
      spl_to_playlist_t(device, &ob->oi, ob->oid, pl);
    }
    else if ( ob->oi.ObjectFormat != PTP_OFC_MTP_AbstractAudioVideoPlaylist ) {
      continue;
    }
    else {
      
      pl = LIBMTP_new_playlist_t();

      
      pl->name = get_string_from_object(device, ob->oid, PTP_OPC_Name);
      if (pl->name == NULL) {
	pl->name = strdup(ob->oi.Filename);
      }
      pl->playlist_id = ob->oid;
      pl->parent_id = ob->oi.ParentObject;
      pl->storage_id = ob->oi.StorageID;

      
      ret = ptp_mtp_getobjectreferences(params, pl->playlist_id, &pl->tracks, &pl->no_tracks);
      if (ret != PTP_RC_OK) {
        add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Playlist_List(): "
				    "could not get object references.");
        pl->tracks = NULL;
        pl->no_tracks = 0;
      }
    }

    
    if (retlists == NULL) {
      retlists = pl;
      curlist = pl;
    } else {
      curlist->next = pl;
      curlist = pl;
    }

    
  }
  return retlists;
}


LIBMTP_playlist_t *LIBMTP_Get_Playlist(LIBMTP_mtpdevice_t *device, uint32_t const plid)
{
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  const int REQ_SPL = FLAG_PLAYLIST_SPL(ptp_usb);
  PTPParams *params = (PTPParams *) device->params;
  PTPObject *ob;
  LIBMTP_playlist_t *pl;
  uint16_t ret;

  
  if (params->nrofobjects == 0) {
    flush_handles(device);
  }

  ret = ptp_object_want (params, plid, PTPOBJECT_OBJECTINFO_LOADED, &ob);
  if (ret != PTP_RC_OK)
    return NULL;

  
  
  if ( REQ_SPL && is_spl_playlist(&ob->oi) ) {
    
    pl = LIBMTP_new_playlist_t();
    spl_to_playlist_t(device, &ob->oi, ob->oid, pl);
    return pl;
  }

  
  else if ( ob->oi.ObjectFormat != PTP_OFC_MTP_AbstractAudioVideoPlaylist ) {
    return NULL;
  }

  
  pl = LIBMTP_new_playlist_t();

  pl->name = get_string_from_object(device, ob->oid, PTP_OPC_Name);
  if (pl->name == NULL) {
    pl->name = strdup(ob->oi.Filename);
  }
  pl->playlist_id = ob->oid;
  pl->parent_id = ob->oi.ParentObject;
  pl->storage_id = ob->oi.StorageID;

  
  ret = ptp_mtp_getobjectreferences(params, pl->playlist_id, &pl->tracks, &pl->no_tracks);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Playlist(): Could not get object references.");
    pl->tracks = NULL;
    pl->no_tracks = 0;
  }

  return pl;
}

static int create_new_abstract_list(LIBMTP_mtpdevice_t *device,
				    char const * const name,
				    char const * const artist,
				    char const * const composer,
				    char const * const genre,
				    uint32_t const parenthandle,
				    uint32_t const storageid,
				    uint16_t const objectformat,
				    char const * const suffix,
				    uint32_t * const newid,
				    uint32_t const * const tracks,
				    uint32_t const no_tracks)

{
  int i;
  int supported = 0;
  uint16_t ret;
  uint16_t *properties = NULL;
  uint32_t propcnt = 0;
  uint32_t store;
  uint32_t localph = parenthandle;
  uint8_t nonconsumable = 0x00U; 
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  char fname[256];
  uint8_t data[2];

  if (storageid == 0) {
    
    store = get_writeable_storageid(device, 512);
  } else {
    store = storageid;
  }

  
  for ( i=0; i < params->deviceinfo.ImageFormats_len; i++ ) {
    if (params->deviceinfo.ImageFormats[i] == objectformat) {
      supported = 1;
      break;
    }
  }
  if (!supported) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): player does not support this abstract type.");
    printf("Unsupported abstract list type: %04x\n", objectformat);
    return -1;
  }

  
  fname[0] = '\0';
  if (strlen(name) > strlen(suffix)) {
    char const * const suff = &name[strlen(name)-strlen(suffix)];
    if (!strcmp(suff, suffix)) {
      
      strncpy(fname, name, sizeof(fname));
    }
  }
  
  if (fname[0] == '\0') {
    strncpy(fname, name, sizeof(fname)-strlen(suffix)-1);
    strcat(fname, suffix);
    fname[sizeof(fname)-1] = '\0';
  }

  if (ptp_operation_issupported(params, PTP_OC_MTP_SendObjectPropList) &&
      !FLAG_BROKEN_SEND_OBJECT_PROPLIST(ptp_usb)) {
    MTPProperties *props = NULL;
    MTPProperties *prop = NULL;
    int nrofprops = 0;

    *newid = 0x00000000U;

    ret = ptp_mtp_getobjectpropssupported(params, objectformat, &propcnt, &properties);

    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;

      ret = ptp_mtp_getobjectpropdesc(params, properties[i], objectformat, &opd);
      if (ret != PTP_RC_OK) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (properties[i]) {
	case PTP_OPC_ObjectFileName:
	  prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	  prop->ObjectHandle = *newid;
	  prop->property = PTP_OPC_ObjectFileName;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(fname);
	  if (FLAG_ONLY_7BIT_FILENAMES(ptp_usb)) {
	    strip_7bit_from_utf8(prop->propval.str);
	  }
	  break;
	case PTP_OPC_ProtectionStatus:
	  prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	  prop->ObjectHandle = *newid;
	  prop->property = PTP_OPC_ProtectionStatus;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = 0x0000U; 
	  break;
	case PTP_OPC_NonConsumable:
	  prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	  prop->ObjectHandle = *newid;
	  prop->property = PTP_OPC_NonConsumable;
	  prop->datatype = PTP_DTC_UINT8;
	  prop->propval.u8 = nonconsumable;
	  break;
	case PTP_OPC_Name:
	  if (name != NULL) {
	    prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	    prop->ObjectHandle = *newid;
	    prop->property = PTP_OPC_Name;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(name);
	  }
	  break;
	case PTP_OPC_AlbumArtist:
	  if (artist != NULL) {
	    prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	    prop->ObjectHandle = *newid;
	    prop->property = PTP_OPC_AlbumArtist;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(artist);
	  }
	  break;
	case PTP_OPC_Artist:
	  if (artist != NULL) {
	    prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	    prop->ObjectHandle = *newid;
	    prop->property = PTP_OPC_Artist;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(artist);
	  }
	  break;
	case PTP_OPC_Composer:
	  if (composer != NULL) {
	    prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	    prop->ObjectHandle = *newid;
	    prop->property = PTP_OPC_Composer;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(composer);
	  }
	  break;
	case PTP_OPC_Genre:
	  if (genre != NULL) {
	    prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	    prop->ObjectHandle = *newid;
	    prop->property = PTP_OPC_Genre;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(genre);
	  }
	  break;
 	case PTP_OPC_DateModified:
	  
	  if (!FLAG_CANNOT_HANDLE_DATEMODIFIED(ptp_usb)) {
	    prop = ptp_get_new_object_prop_entry(&props,&nrofprops);
	    prop->ObjectHandle = *newid;
	    prop->property = PTP_OPC_DateModified;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = get_iso8601_stamp();
	  }
	  break;
	}
      }
      ptp_free_objectpropdesc(&opd);
    }
    free(properties);

    ret = ptp_mtp_sendobjectproplist(params, &store, &localph, newid,
				     objectformat, 0, props, nrofprops);

    
    ptp_destroy_object_prop_list(props, nrofprops);

    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "create_new_abstract_list(): Could not send object property list.");
      if (ret == PTP_RC_AccessDenied) {
	add_ptp_error_to_errorstack(device, ret, "ACCESS DENIED.");
      }
      return -1;
    }

    
    ret = ptp_sendobject(params, NULL, 0);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "create_new_abstract_list(): Could not send blank object data.");
      return -1;
    }

  } else if (ptp_operation_issupported(params,PTP_OC_SendObjectInfo)) {
    PTPObjectInfo new_object;

    new_object.Filename = fname;
    if (FLAG_ONLY_7BIT_FILENAMES(ptp_usb)) {
      strip_7bit_from_utf8(new_object.Filename);
    }
    new_object.ObjectCompressedSize = 1;
    new_object.ObjectFormat = objectformat;

    
    ret = ptp_sendobjectinfo(params, &store, &localph, newid, &new_object);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "create_new_abstract_list(): Could not send object info (the playlist itself).");
      if (ret == PTP_RC_AccessDenied) {
	add_ptp_error_to_errorstack(device, ret, "ACCESS DENIED.");
      }
      return -1;
    }
    
    
    data[0] = '\0';
    data[1] = '\0';
    ret = ptp_sendobject(params, data, 1);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "create_new_abstract_list(): Could not send blank object data.");
      return -1;
    }

    
    ret = ptp_mtp_getobjectpropssupported(params, objectformat, &propcnt, &properties);

    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;

      ret = ptp_mtp_getobjectpropdesc(params, properties[i], objectformat, &opd);
      if (ret != PTP_RC_OK) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (properties[i]) {
	case PTP_OPC_Name:
	  if (name != NULL) {
	    ret = set_object_string(device, *newid, PTP_OPC_Name, name);
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): could not set entity name.");
	      return -1;
	    }
	  }
	  break;
	case PTP_OPC_AlbumArtist:
	  if (artist != NULL) {
	    ret = set_object_string(device, *newid, PTP_OPC_AlbumArtist, artist);
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): could not set entity album artist.");
	      return -1;
	    }
	  }
	  break;
	case PTP_OPC_Artist:
	  if (artist != NULL) {
	    ret = set_object_string(device, *newid, PTP_OPC_Artist, artist);
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): could not set entity artist.");
	      return -1;
	    }
	  }
	  break;
	case PTP_OPC_Composer:
	  if (composer != NULL) {
	    ret = set_object_string(device, *newid, PTP_OPC_Composer, composer);
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): could not set entity composer.");
	      return -1;
	    }
	  }
	  break;
	case PTP_OPC_Genre:
	  if (genre != NULL) {
	    ret = set_object_string(device, *newid, PTP_OPC_Genre, genre);
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): could not set entity genre.");
	      return -1;
	    }
	  }
	  break;
 	case PTP_OPC_DateModified:
	  if (!FLAG_CANNOT_HANDLE_DATEMODIFIED(ptp_usb)) {
	    ret = set_object_string(device, *newid, PTP_OPC_DateModified, get_iso8601_stamp());
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): could not set date modified.");
	      return -1;
	    }
	  }
	  break;
	}
      }
      ptp_free_objectpropdesc(&opd);
    }
    free(properties);
  }

  if (no_tracks > 0) {
    
    ret = ptp_mtp_setobjectreferences (params, *newid, (uint32_t *) tracks, no_tracks);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "create_new_abstract_list(): could not add tracks as object references.");
      return -1;
    }
  }

  add_object_to_cache(device, *newid);

  return 0;
}

static int update_abstract_list(LIBMTP_mtpdevice_t *device,
				char const * const name,
				char const * const artist,
				char const * const composer,
				char const * const genre,
				uint32_t const objecthandle,
				uint16_t const objectformat,
				uint32_t const * const tracks,
				uint32_t const no_tracks)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint16_t *properties = NULL;
  uint32_t propcnt = 0;
  int i;

  
  
  ret = ptp_mtp_getobjectpropssupported(params, objectformat, &propcnt, &properties);
  if (ret != PTP_RC_OK) {
    
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "update_abstract_list(): "
			    "could not retrieve supported object properties.");
    return -1;
  }
  if (ptp_operation_issupported(params,PTP_OC_MTP_SetObjPropList) &&
      !FLAG_BROKEN_SET_OBJECT_PROPLIST(ptp_usb)) {
    MTPProperties *props = NULL;
    MTPProperties *prop = NULL;
    int nrofprops = 0;

    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;

      ret = ptp_mtp_getobjectpropdesc(params, properties[i], objectformat, &opd);
      if (ret != PTP_RC_OK) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "update_abstract_list(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (properties[i]) {
	case PTP_OPC_Name:
	  prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	  prop->ObjectHandle = objecthandle;
	  prop->property = PTP_OPC_Name;
	  prop->datatype = PTP_DTC_STR;
	  if (name != NULL)
	    prop->propval.str = strdup(name);
	  break;
	case PTP_OPC_AlbumArtist:
	  if (artist != NULL) {
	    prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	    prop->ObjectHandle = objecthandle;
	    prop->property = PTP_OPC_AlbumArtist;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(artist);
	  }
	  break;
	case PTP_OPC_Artist:
	  if (artist != NULL) {
	    prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	    prop->ObjectHandle = objecthandle;
	    prop->property = PTP_OPC_Artist;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(artist);
	  }
	  break;
	case PTP_OPC_Composer:
	  if (composer != NULL) {
	    prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	    prop->ObjectHandle = objecthandle;
	    prop->property = PTP_OPC_Composer;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(composer);
	  }
	  break;
	case PTP_OPC_Genre:
	  if (genre != NULL) {
	    prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	    prop->ObjectHandle = objecthandle;
	    prop->property = PTP_OPC_Genre;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(genre);
	  }
	  break;
 	case PTP_OPC_DateModified:
	  if (!FLAG_CANNOT_HANDLE_DATEMODIFIED(ptp_usb)) {
	    
	    prop = ptp_get_new_object_prop_entry(&props, &nrofprops);
	    prop->ObjectHandle = objecthandle;
	    prop->property = PTP_OPC_DateModified;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = get_iso8601_stamp();
	  }
	  break;
	default:
	  break;
	}
      }
      ptp_free_objectpropdesc(&opd);
    }

    
    if (props != NULL) {
      ret = ptp_mtp_setobjectproplist(params, props, nrofprops);

      ptp_destroy_object_prop_list(props, nrofprops);

      if (ret != PTP_RC_OK) {
        
        add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "update_abstract_list(): "
                                "could not set object property list.");
        free(properties);
        return -1;
      }
    }

  } else if (ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    for (i=0;i<propcnt;i++) {
      switch (properties[i]) {
      case PTP_OPC_Name:
	
	ret = set_object_string(device, objecthandle, PTP_OPC_Name, name);
	if (ret != 0) {
	  add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "update_abstract_list(): "
				  "could not set title.");
	}
	break;
      case PTP_OPC_AlbumArtist:
	
	ret = set_object_string(device, objecthandle, PTP_OPC_AlbumArtist, artist);
	if (ret != 0) {
	  add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "update_abstract_list(): "
				  "could not set album artist name.");
	}
	break;
      case PTP_OPC_Artist:
	
	ret = set_object_string(device, objecthandle, PTP_OPC_Artist, artist);
	if (ret != 0) {
	  add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "update_abstract_list(): "
				  "could not set artist name.");
	}
      case PTP_OPC_Composer:
	
	ret = set_object_string(device, objecthandle, PTP_OPC_Composer, composer);
	if (ret != 0) {
	  add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "update_abstract_list(): "
				  "could not set composer name.");
	}
	break;
      case PTP_OPC_Genre:
	
	ret = set_object_string(device, objecthandle, PTP_OPC_Genre, genre);
	if (ret != 0) {
	  add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "update_abstract_list(): "
				  "could not set genre.");
	}
	break;
      case PTP_OPC_DateModified:
	
	if (!FLAG_CANNOT_HANDLE_DATEMODIFIED(ptp_usb)) {
	  char *tmpdate = get_iso8601_stamp();
	  ret = set_object_string(device, objecthandle, PTP_OPC_DateModified, tmpdate);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "update_abstract_list(): "
				    "could not set modification date.");
	  }
	  free(tmpdate);
	}
      default:
	break;
      }
    }
  } else {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "update_abstract_list(): "
                            "Your device doesn't seem to support any known way of setting metadata.");
    free(properties);
    return -1;
  }

  
  ret = ptp_mtp_setobjectreferences (params, objecthandle, (uint32_t *) tracks, no_tracks);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "update_abstract_list(): could not add tracks as object references.");
    free(properties);
    return -1;
  }

  free(properties);

  update_metadata_cache(device, objecthandle);

  return 0;
}


int LIBMTP_Create_New_Playlist(LIBMTP_mtpdevice_t *device,
			       LIBMTP_playlist_t * const metadata)
{
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint32_t localph = metadata->parent_id;

  
  if (localph == 0) {
    if (device->default_playlist_folder != 0)
      localph = device->default_playlist_folder;
    else
      localph = device->default_music_folder;
  }
  metadata->parent_id = localph;

  
  if(FLAG_PLAYLIST_SPL(ptp_usb)) {
    return playlist_t_to_spl(device, metadata);
  }

  
  return create_new_abstract_list(device,
				  metadata->name,
				  NULL,
				  NULL,
				  NULL,
				  localph,
				  metadata->storage_id,
				  PTP_OFC_MTP_AbstractAudioVideoPlaylist,
				  get_playlist_extension(ptp_usb),
				  &metadata->playlist_id,
				  metadata->tracks,
				  metadata->no_tracks);
}

int LIBMTP_Update_Playlist(LIBMTP_mtpdevice_t *device,
			   LIBMTP_playlist_t * const metadata)
{

  
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  if(FLAG_PLAYLIST_SPL(ptp_usb)) {
    return update_spl_playlist(device, metadata);
  }

  return update_abstract_list(device,
			      metadata->name,
			      NULL,
			      NULL,
			      NULL,
			      metadata->playlist_id,
			      PTP_OFC_MTP_AbstractAudioVideoPlaylist,
			      metadata->tracks,
			      metadata->no_tracks);
}

LIBMTP_album_t *LIBMTP_new_album_t(void)
{
  LIBMTP_album_t *new = (LIBMTP_album_t *) malloc(sizeof(LIBMTP_album_t));
  if (new == NULL) {
    return NULL;
  }
  new->album_id = 0;
  new->parent_id = 0;
  new->storage_id = 0;
  new->name = NULL;
  new->artist = NULL;
  new->composer = NULL;
  new->genre = NULL;
  new->tracks = NULL;
  new->no_tracks = 0;
  new->next = NULL;
  return new;
}

void LIBMTP_destroy_album_t(LIBMTP_album_t *album)
{
  if (album == NULL) {
    return;
  }
  if (album->name != NULL)
    free(album->name);
  if (album->artist != NULL)
    free(album->artist);
  if (album->composer != NULL)
    free(album->composer);
  if (album->genre != NULL)
    free(album->genre);
  if (album->tracks != NULL)
    free(album->tracks);
  free(album);
  return;
}

static void pick_property_to_album_metadata(LIBMTP_mtpdevice_t *device,
					    MTPProperties *prop, LIBMTP_album_t *alb)
{
  switch (prop->property) {
  case PTP_OPC_Name:
    if (prop->propval.str != NULL)
      alb->name = strdup(prop->propval.str);
    else
      alb->name = NULL;
    break;
  case PTP_OPC_AlbumArtist:
    if (prop->propval.str != NULL) {
      
      if (alb->artist != NULL)
	free(alb->artist);
      alb->artist = strdup(prop->propval.str);
    } else
      alb->artist = NULL;
    break;
  case PTP_OPC_Artist:
    if (prop->propval.str != NULL) {
      
      if (alb->artist == NULL)
	alb->artist = strdup(prop->propval.str);
    } else
      alb->artist = NULL;
    break;
  case PTP_OPC_Composer:
    if (prop->propval.str != NULL)
      alb->composer = strdup(prop->propval.str);
    else
      alb->composer = NULL;
    break;
  case PTP_OPC_Genre:
    if (prop->propval.str != NULL)
      alb->genre = strdup(prop->propval.str);
    else
      alb->genre = NULL;
    break;
  }
}

static void get_album_metadata(LIBMTP_mtpdevice_t *device,
			       LIBMTP_album_t *alb)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  uint32_t i;
  MTPProperties *prop;
  PTPObject *ob;

  ret = ptp_object_want(params, alb->album_id, PTPOBJECT_MTPPROPLIST_LOADED, &ob);
  if (ob->mtpprops) {
    prop = ob->mtpprops;
    for (i=0;i<ob->nrofmtpprops;i++,prop++)
      pick_property_to_album_metadata(device, prop, alb);
  } else {
    uint16_t *props = NULL;
    uint32_t propcnt = 0;

    
    ret = ptp_mtp_getobjectpropssupported(params, PTP_OFC_MTP_AbstractAudioAlbum, &propcnt, &props);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "get_album_metadata(): call to ptp_mtp_getobjectpropssupported() failed.");
      
      return;
    } else {
      for (i=0;i<propcnt;i++) {
	switch (props[i]) {
	case PTP_OPC_Name:
	  alb->name = get_string_from_object(device, ob->oid, PTP_OPC_Name);
	  break;
	case PTP_OPC_AlbumArtist:
	  if (alb->artist != NULL)
	    free(alb->artist);
	  alb->artist = get_string_from_object(device, ob->oid, PTP_OPC_AlbumArtist);
	  break;
	case PTP_OPC_Artist:
	  if (alb->artist == NULL)
	    alb->artist = get_string_from_object(device, ob->oid, PTP_OPC_Artist);
	  break;
	case PTP_OPC_Composer:
	  alb->composer = get_string_from_object(device, ob->oid, PTP_OPC_Composer);
	  break;
	case PTP_OPC_Genre:
	  alb->genre = get_string_from_object(device, ob->oid, PTP_OPC_Genre);
	  break;
	default:
	  break;
	}
      }
      free(props);
    }
  }
}

LIBMTP_album_t *LIBMTP_Get_Album_List(LIBMTP_mtpdevice_t *device)
{
  PTPParams *params = (PTPParams *) device->params;
  LIBMTP_album_t *retalbums = NULL;
  LIBMTP_album_t *curalbum = NULL;
  uint32_t i;

  
  if (params->nrofobjects == 0)
    flush_handles(device);

  for (i = 0; i < params->nrofobjects; i++) {
    LIBMTP_album_t *alb;
    PTPObject *ob;
    uint16_t ret;

    ob = &params->objects[i];

    
    if ( ob->oi.ObjectFormat != PTP_OFC_MTP_AbstractAudioAlbum )
      continue;

    
    alb = LIBMTP_new_album_t();
    alb->album_id = ob->oid;
    alb->parent_id = ob->oi.ParentObject;
    alb->storage_id = ob->oi.StorageID;

    
    get_album_metadata(device, alb);

    
    ret = ptp_mtp_getobjectreferences(params, alb->album_id, &alb->tracks, &alb->no_tracks);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Album_List(): Could not get object references.");
      alb->tracks = NULL;
      alb->no_tracks = 0;
    }

    
    if (retalbums == NULL) {
      retalbums = alb;
      curalbum = alb;
    } else {
      curalbum->next = alb;
      curalbum = alb;
    }

  }
  return retalbums;
}

LIBMTP_album_t *LIBMTP_Get_Album(LIBMTP_mtpdevice_t *device, uint32_t const albid)
{
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;
  PTPObject *ob;
  LIBMTP_album_t *alb;

  
  if (params->nrofobjects == 0)
    flush_handles(device);

  ret = ptp_object_want(params, albid, PTPOBJECT_OBJECTINFO_LOADED, &ob);
  if (ret != PTP_RC_OK)
    return NULL;

  
  if (ob->oi.ObjectFormat != PTP_OFC_MTP_AbstractAudioAlbum)
    return NULL;

  
  alb = LIBMTP_new_album_t();
  alb->album_id = ob->oid;
  alb->parent_id = ob->oi.ParentObject;
  alb->storage_id = ob->oi.StorageID;

  
  get_album_metadata(device, alb);

  
  ret = ptp_mtp_getobjectreferences(params, alb->album_id, &alb->tracks, &alb->no_tracks);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Album: Could not get object references.");
    alb->tracks = NULL;
    alb->no_tracks = 0;
  }

  return alb;
}

int LIBMTP_Create_New_Album(LIBMTP_mtpdevice_t *device,
			    LIBMTP_album_t * const metadata)
{
  uint32_t localph = metadata->parent_id;

  
  if (localph == 0) {
    if (device->default_album_folder != 0)
      localph = device->default_album_folder;
    else
      localph = device->default_music_folder;
  }
  metadata->parent_id = localph;

  
  return create_new_abstract_list(device,
				  metadata->name,
				  metadata->artist,
				  metadata->composer,
				  metadata->genre,
				  localph,
				  metadata->storage_id,
				  PTP_OFC_MTP_AbstractAudioAlbum,
				  ".alb",
				  &metadata->album_id,
				  metadata->tracks,
				  metadata->no_tracks);
}

LIBMTP_filesampledata_t *LIBMTP_new_filesampledata_t(void)
{
  LIBMTP_filesampledata_t *new = (LIBMTP_filesampledata_t *) malloc(sizeof(LIBMTP_filesampledata_t));
  if (new == NULL) {
    return NULL;
  }
  new->height=0;
  new->width = 0;
  new->data = NULL;
  new->duration = 0;
  new->size = 0;
  return new;
}

void LIBMTP_destroy_filesampledata_t(LIBMTP_filesampledata_t * sample)
{
  if (sample == NULL) {
    return;
  }
  if (sample->data != NULL) {
    free(sample->data);
  }
  free(sample);
}

int LIBMTP_Get_Representative_Sample_Format(LIBMTP_mtpdevice_t *device,
					    LIBMTP_filetype_t const filetype,
					    LIBMTP_filesampledata_t ** sample)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t *props = NULL;
  uint32_t propcnt = 0;
  int i;
  
  int support_data = 0;
  int support_format = 0;
  int support_height = 0;
  int support_width = 0;
  int support_duration = 0;
  int support_size = 0;

  PTPObjectPropDesc opd_height;
  PTPObjectPropDesc opd_width;
  PTPObjectPropDesc opd_format;
  PTPObjectPropDesc opd_duration;
  PTPObjectPropDesc opd_size;
  
  
  *sample = NULL;
  
  ret = ptp_mtp_getobjectpropssupported(params, map_libmtp_type_to_ptp_type(filetype), &propcnt, &props);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Representative_Sample_Format(): could not get object properties.");
    return -1;
  }
  for (i = 0; i < propcnt; i++) {
    switch(props[i]) {
    case PTP_OPC_RepresentativeSampleData:
      support_data = 1;
      break;
    case PTP_OPC_RepresentativeSampleFormat:
      support_format = 1;
      break;
    case PTP_OPC_RepresentativeSampleSize:
      support_size = 1;
      break;
    case PTP_OPC_RepresentativeSampleHeight:
      support_height = 1;
      break;
    case PTP_OPC_RepresentativeSampleWidth:
      support_width = 1;
      break;
    case PTP_OPC_RepresentativeSampleDuration:
      support_duration = 1;
      break;
    default:
      break;
    }
  }
  free(props);
    
  if (support_data && support_format && support_height && support_width && !support_duration) {
    
    LIBMTP_filesampledata_t *retsam = LIBMTP_new_filesampledata_t();
    ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleFormat, map_libmtp_type_to_ptp_type(filetype), &opd_format);    
    retsam->filetype = map_ptp_type_to_libmtp_type(opd_format.FORM.Enum.SupportedValue[0].u16);
    ptp_free_objectpropdesc(&opd_format);
    
    ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleWidth, map_libmtp_type_to_ptp_type(filetype), &opd_width);        
    retsam->width = opd_width.FORM.Range.MaximumValue.u32;
    ptp_free_objectpropdesc(&opd_width);
    
    ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleHeight, map_libmtp_type_to_ptp_type(filetype), &opd_height);    						
    retsam->height = opd_height.FORM.Range.MaximumValue.u32;
    ptp_free_objectpropdesc(&opd_height);
    
    if (support_size) {
      ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleSize, map_libmtp_type_to_ptp_type(filetype), &opd_size);
      retsam->size = opd_size.FORM.Range.MaximumValue.u32;
      ptp_free_objectpropdesc(&opd_size);
    }
    *sample = retsam;
  } else if (support_data && support_format && !support_height && !support_width && support_duration) {
    
    LIBMTP_filesampledata_t *retsam = LIBMTP_new_filesampledata_t();
    ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleFormat, map_libmtp_type_to_ptp_type(filetype), &opd_format);    
    retsam->filetype = map_ptp_type_to_libmtp_type(opd_format.FORM.Enum.SupportedValue[0].u16);
    ptp_free_objectpropdesc(&opd_format);
    
    ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleDuration, map_libmtp_type_to_ptp_type(filetype), &opd_duration);    
    retsam->duration = opd_duration.FORM.Range.MaximumValue.u32;
    ptp_free_objectpropdesc(&opd_duration);
    
    if (support_size) {
      ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleSize, map_libmtp_type_to_ptp_type(filetype), &opd_size);
      retsam->size = opd_size.FORM.Range.MaximumValue.u32;
      ptp_free_objectpropdesc(&opd_size);
    }
    *sample = retsam;
  }
  return 0;
}

int LIBMTP_Send_Representative_Sample(LIBMTP_mtpdevice_t *device,
                          uint32_t const id,
                          LIBMTP_filesampledata_t *sampledata)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  PTPPropertyValue propval;
  PTPObject *ob;
  uint32_t i;
  uint16_t *props = NULL;
  uint32_t propcnt = 0;
  int supported = 0;

  
  ret = ptp_object_want (params, id, PTPOBJECT_OBJECTINFO_LOADED, &ob);
  if (ret != PTP_RC_OK) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_Representative_Sample(): could not get object info.");
    return -1;
  }

  
  ret = ptp_mtp_getobjectpropssupported(params, ob->oi.ObjectFormat, &propcnt, &props);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_Representative_Sample(): could not get object properties.");
    return -1;
  }

  for (i = 0; i < propcnt; i++) {
    if (props[i] == PTP_OPC_RepresentativeSampleData) {
      supported = 1;
      break;
    }
  }
  if (!supported) {
    free(props);
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_Representative_Sample(): object type doesn't support RepresentativeSampleData.");
    return -1;
  }
  free(props);
  
  
  propval.a.count = sampledata->size;
  propval.a.v = malloc(sizeof(PTPPropertyValue) * sampledata->size);
  for (i = 0; i < sampledata->size; i++) {
    propval.a.v[i].u8 = sampledata->data[i];
  }
  
  ret = ptp_mtp_setobjectpropvalue(params,id,PTP_OPC_RepresentativeSampleData,
				   &propval,PTP_DTC_AUINT8);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_Representative_Sample(): could not send sample data.");
    free(propval.a.v);
    return -1;
  }
  free(propval.a.v);
  
  switch(sampledata->filetype) {
  case LIBMTP_FILETYPE_JPEG:
  case LIBMTP_FILETYPE_JFIF:
  case LIBMTP_FILETYPE_TIFF:
  case LIBMTP_FILETYPE_BMP:
  case LIBMTP_FILETYPE_GIF:
  case LIBMTP_FILETYPE_PICT:
  case LIBMTP_FILETYPE_PNG:
    if (!FLAG_BROKEN_SET_SAMPLE_DIMENSIONS(ptp_usb)) {
      
      set_object_u32(device, id, PTP_OPC_RepresentativeSampleHeight, sampledata->height);
      set_object_u32(device, id, PTP_OPC_RepresentativeSampleWidth, sampledata->width);		
    }
    break;
  default:
    
    set_object_u32(device, id, PTP_OPC_RepresentativeSampleDuration, sampledata->duration);
    set_object_u32(device, id, PTP_OPC_RepresentativeSampleSize, sampledata->size);
    break;  		
  }
    
  return 0;
}

int LIBMTP_Get_Representative_Sample(LIBMTP_mtpdevice_t *device,
                          uint32_t const id,
                          LIBMTP_filesampledata_t *sampledata)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTPPropertyValue propval;
  PTPObject *ob;
  uint32_t i;
  uint16_t *props = NULL;
  uint32_t propcnt = 0;
  int supported = 0;

  
  ret = ptp_object_want (params, id, PTPOBJECT_OBJECTINFO_LOADED, &ob);
  if (ret != PTP_RC_OK) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_Representative_Sample(): could not get object info.");
    return -1;
  }

  
  ret = ptp_mtp_getobjectpropssupported(params, ob->oi.ObjectFormat, &propcnt, &props);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Representative_Sample(): could not get object properties.");
    return -1;
  }

  for (i = 0; i < propcnt; i++) {
    if (props[i] == PTP_OPC_RepresentativeSampleData) {
      supported = 1;
      break;
    }
  }
  if (!supported) {
    free(props);
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_Representative_Sample(): object type doesn't support RepresentativeSampleData.");
    return -1;
  }
  free(props);
  
  
  ret = ptp_mtp_getobjectpropvalue(params,id,PTP_OPC_RepresentativeSampleData,
				   &propval,PTP_DTC_AUINT8);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Representative_Sample(): could not get sample data.");
    return -1;
  }
  
  
  sampledata->size = propval.a.count;
  sampledata->data = malloc(sizeof(PTPPropertyValue) * propval.a.count);
  for (i = 0; i < propval.a.count; i++) {
    sampledata->data[i] = propval.a.v[i].u8;
  }
  free(propval.a.v);

  
  sampledata->width = get_u32_from_object(device, id, PTP_OPC_RepresentativeSampleWidth, 0);
  sampledata->height = get_u32_from_object(device, id, PTP_OPC_RepresentativeSampleHeight, 0);
  sampledata->duration = get_u32_from_object(device, id, PTP_OPC_RepresentativeSampleDuration, 0);
  sampledata->filetype = map_ptp_type_to_libmtp_type(
        get_u16_from_object(device, id, PTP_OPC_RepresentativeSampleFormat, LIBMTP_FILETYPE_UNKNOWN));
    
  return 0;
}

int LIBMTP_Update_Album(LIBMTP_mtpdevice_t *device,
			   LIBMTP_album_t const * const metadata)
{
  return update_abstract_list(device,
			      metadata->name,
			      metadata->artist,
			      metadata->composer,
			      metadata->genre,
			      metadata->album_id,
			      PTP_OFC_MTP_AbstractAudioAlbum,
			      metadata->tracks,
			      metadata->no_tracks);
}

void ptp_nikon_getptpipguid (unsigned char* guid) {
  return;
}

 
static void add_object_to_cache(LIBMTP_mtpdevice_t *device, uint32_t object_id)
{
  PTPParams *params = (PTPParams *)device->params;
  uint16_t ret;

  ret = ptp_add_object_to_cache(params, object_id);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "add_object_to_cache(): couldn't add object to cache");
  }
}


static void update_metadata_cache(LIBMTP_mtpdevice_t *device, uint32_t object_id)
{
  PTPParams *params = (PTPParams *)device->params;

  ptp_remove_object_from_cache(params, object_id);
  add_object_to_cache(device, object_id);
}

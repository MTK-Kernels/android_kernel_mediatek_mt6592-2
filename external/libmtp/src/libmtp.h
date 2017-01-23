/**
 * \file libmtp.h
 * Interface to the Media Transfer Protocol library.
 *
 * Copyright (C) 2005-2008 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2005-2008 Richard A. Low <richard@wentnet.com>
 * Copyright (C) 2007 Ted Bullock <tbullock@canada.com>
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
 * <code>
 * #include <libmtp.h>
 * </code>
 */
#ifndef LIBMTP_H_INCLUSION_GUARD
#define LIBMTP_H_INCLUSION_GUARD

#define LIBMTP_VERSION 1.0.1
#define LIBMTP_VERSION_STRING "1.0.1"

#ifdef _MSC_VER
#include <windows.h>
#define __WIN32__
#define snprintf _snprintf
#define ssize_t SSIZE_T
typedef char int8_t;
typedef unsigned char uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#endif

#include <stdio.h>
#include <usb.h>
#include <stdint.h>
#include <utime.h>

typedef enum {
  LIBMTP_FILETYPE_FOLDER,
  LIBMTP_FILETYPE_WAV,
  LIBMTP_FILETYPE_MP3,
  LIBMTP_FILETYPE_WMA,
  LIBMTP_FILETYPE_OGG,
  LIBMTP_FILETYPE_AUDIBLE,
  LIBMTP_FILETYPE_MP4,
  LIBMTP_FILETYPE_UNDEF_AUDIO,
  LIBMTP_FILETYPE_WMV,
  LIBMTP_FILETYPE_AVI,
  LIBMTP_FILETYPE_MPEG,
  LIBMTP_FILETYPE_ASF,
  LIBMTP_FILETYPE_QT,
  LIBMTP_FILETYPE_UNDEF_VIDEO,
  LIBMTP_FILETYPE_JPEG,
  LIBMTP_FILETYPE_JFIF,
  LIBMTP_FILETYPE_TIFF,
  LIBMTP_FILETYPE_BMP,
  LIBMTP_FILETYPE_GIF,
  LIBMTP_FILETYPE_PICT,
  LIBMTP_FILETYPE_PNG,
  LIBMTP_FILETYPE_VCALENDAR1,
  LIBMTP_FILETYPE_VCALENDAR2,
  LIBMTP_FILETYPE_VCARD2,
  LIBMTP_FILETYPE_VCARD3,
  LIBMTP_FILETYPE_WINDOWSIMAGEFORMAT,
  LIBMTP_FILETYPE_WINEXEC,
  LIBMTP_FILETYPE_TEXT,
  LIBMTP_FILETYPE_HTML,
  LIBMTP_FILETYPE_FIRMWARE,
  LIBMTP_FILETYPE_AAC,
  LIBMTP_FILETYPE_MEDIACARD,
  LIBMTP_FILETYPE_FLAC,
  LIBMTP_FILETYPE_MP2,
  LIBMTP_FILETYPE_M4A,
  LIBMTP_FILETYPE_DOC,
  LIBMTP_FILETYPE_XML,
  LIBMTP_FILETYPE_XLS,
  LIBMTP_FILETYPE_PPT,
  LIBMTP_FILETYPE_MHT,
  LIBMTP_FILETYPE_JP2,
  LIBMTP_FILETYPE_JPX,
  LIBMTP_FILETYPE_ALBUM,
  LIBMTP_FILETYPE_PLAYLIST,
  LIBMTP_FILETYPE_UNKNOWN
} LIBMTP_filetype_t;

#define LIBMTP_FILETYPE_IS_AUDIO(a)\
(a == LIBMTP_FILETYPE_WAV ||\
 a == LIBMTP_FILETYPE_MP3 ||\
 a == LIBMTP_FILETYPE_MP2 ||\
 a == LIBMTP_FILETYPE_WMA ||\
 a == LIBMTP_FILETYPE_OGG ||\
 a == LIBMTP_FILETYPE_FLAC ||\
 a == LIBMTP_FILETYPE_AAC ||\
 a == LIBMTP_FILETYPE_M4A ||\
 a == LIBMTP_FILETYPE_AUDIBLE ||\
 a == LIBMTP_FILETYPE_UNDEF_AUDIO)

#define LIBMTP_FILETYPE_IS_VIDEO(a)\
(a == LIBMTP_FILETYPE_WMV ||\
 a == LIBMTP_FILETYPE_AVI ||\
 a == LIBMTP_FILETYPE_MPEG ||\
 a == LIBMTP_FILETYPE_UNDEF_VIDEO)

#define LIBMTP_FILETYPE_IS_AUDIOVIDEO(a)\
(a == LIBMTP_FILETYPE_MP4 ||\
 a == LIBMTP_FILETYPE_ASF ||\
 a == LIBMTP_FILETYPE_QT)

#define LIBMTP_FILETYPE_IS_TRACK(a)\
(LIBMTP_FILETYPE_IS_AUDIO(a) ||\
 LIBMTP_FILETYPE_IS_VIDEO(a) ||\
 LIBMTP_FILETYPE_IS_AUDIOVIDEO(a))

#define LIBMTP_FILETYPE_IS_IMAGE(a)\
(a == LIBMTP_FILETYPE_JPEG ||\
a == LIBMTP_FILETYPE_JFIF ||\
a == LIBMTP_FILETYPE_TIFF ||\
a == LIBMTP_FILETYPE_BMP ||\
a == LIBMTP_FILETYPE_GIF ||\
a == LIBMTP_FILETYPE_PICT ||\
a == LIBMTP_FILETYPE_PNG ||\
a == LIBMTP_FILETYPE_JP2 ||\
a == LIBMTP_FILETYPE_JPX ||\
a == LIBMTP_FILETYPE_WINDOWSIMAGEFORMAT)

#define LIBMTP_FILETYPE_IS_ADDRESSBOOK(a)\
(a == LIBMTP_FILETYPE_VCARD2 ||\
a == LIBMTP_FILETYPE_VCARD2)

#define LIBMTP_FILETYPE_IS_CALENDAR(a)\
(a == LIBMTP_FILETYPE_VCALENDAR1 ||\
a == LIBMTP_FILETYPE_VCALENDAR2)

typedef enum {
  LIBMTP_PROPERTY_StorageID,
  LIBMTP_PROPERTY_ObjectFormat,
  LIBMTP_PROPERTY_ProtectionStatus,
  LIBMTP_PROPERTY_ObjectSize,
  LIBMTP_PROPERTY_AssociationType,
  LIBMTP_PROPERTY_AssociationDesc,
  LIBMTP_PROPERTY_ObjectFileName,
  LIBMTP_PROPERTY_DateCreated,
  LIBMTP_PROPERTY_DateModified,
  LIBMTP_PROPERTY_Keywords,
  LIBMTP_PROPERTY_ParentObject,
  LIBMTP_PROPERTY_AllowedFolderContents,
  LIBMTP_PROPERTY_Hidden,
  LIBMTP_PROPERTY_SystemObject,
  LIBMTP_PROPERTY_PersistantUniqueObjectIdentifier,
  LIBMTP_PROPERTY_SyncID,
  LIBMTP_PROPERTY_PropertyBag,
  LIBMTP_PROPERTY_Name,
  LIBMTP_PROPERTY_CreatedBy,
  LIBMTP_PROPERTY_Artist,
  LIBMTP_PROPERTY_DateAuthored,
  LIBMTP_PROPERTY_Description,
  LIBMTP_PROPERTY_URLReference,
  LIBMTP_PROPERTY_LanguageLocale,
  LIBMTP_PROPERTY_CopyrightInformation,
  LIBMTP_PROPERTY_Source,
  LIBMTP_PROPERTY_OriginLocation,
  LIBMTP_PROPERTY_DateAdded,
  LIBMTP_PROPERTY_NonConsumable,
  LIBMTP_PROPERTY_CorruptOrUnplayable,
  LIBMTP_PROPERTY_ProducerSerialNumber,
  LIBMTP_PROPERTY_RepresentativeSampleFormat,
  LIBMTP_PROPERTY_RepresentativeSampleSize,
  LIBMTP_PROPERTY_RepresentativeSampleHeight,
  LIBMTP_PROPERTY_RepresentativeSampleWidth,
  LIBMTP_PROPERTY_RepresentativeSampleDuration,
  LIBMTP_PROPERTY_RepresentativeSampleData,
  LIBMTP_PROPERTY_Width,
  LIBMTP_PROPERTY_Height,
  LIBMTP_PROPERTY_Duration,
  LIBMTP_PROPERTY_Rating,
  LIBMTP_PROPERTY_Track,
  LIBMTP_PROPERTY_Genre,
  LIBMTP_PROPERTY_Credits,
  LIBMTP_PROPERTY_Lyrics,
  LIBMTP_PROPERTY_SubscriptionContentID,
  LIBMTP_PROPERTY_ProducedBy,
  LIBMTP_PROPERTY_UseCount,
  LIBMTP_PROPERTY_SkipCount,
  LIBMTP_PROPERTY_LastAccessed,
  LIBMTP_PROPERTY_ParentalRating,
  LIBMTP_PROPERTY_MetaGenre,
  LIBMTP_PROPERTY_Composer,
  LIBMTP_PROPERTY_EffectiveRating,
  LIBMTP_PROPERTY_Subtitle,
  LIBMTP_PROPERTY_OriginalReleaseDate,
  LIBMTP_PROPERTY_AlbumName,
  LIBMTP_PROPERTY_AlbumArtist,
  LIBMTP_PROPERTY_Mood,
  LIBMTP_PROPERTY_DRMStatus,
  LIBMTP_PROPERTY_SubDescription,
  LIBMTP_PROPERTY_IsCropped,
  LIBMTP_PROPERTY_IsColorCorrected,
  LIBMTP_PROPERTY_ImageBitDepth,
  LIBMTP_PROPERTY_Fnumber,
  LIBMTP_PROPERTY_ExposureTime,
  LIBMTP_PROPERTY_ExposureIndex,
  LIBMTP_PROPERTY_DisplayName,
  LIBMTP_PROPERTY_BodyText,
  LIBMTP_PROPERTY_Subject,
  LIBMTP_PROPERTY_Priority,
  LIBMTP_PROPERTY_GivenName,
  LIBMTP_PROPERTY_MiddleNames,
  LIBMTP_PROPERTY_FamilyName,
  LIBMTP_PROPERTY_Prefix,
  LIBMTP_PROPERTY_Suffix,
  LIBMTP_PROPERTY_PhoneticGivenName,
  LIBMTP_PROPERTY_PhoneticFamilyName,
  LIBMTP_PROPERTY_EmailPrimary,
  LIBMTP_PROPERTY_EmailPersonal1,
  LIBMTP_PROPERTY_EmailPersonal2,
  LIBMTP_PROPERTY_EmailBusiness1,
  LIBMTP_PROPERTY_EmailBusiness2,
  LIBMTP_PROPERTY_EmailOthers,
  LIBMTP_PROPERTY_PhoneNumberPrimary,
  LIBMTP_PROPERTY_PhoneNumberPersonal,
  LIBMTP_PROPERTY_PhoneNumberPersonal2,
  LIBMTP_PROPERTY_PhoneNumberBusiness,
  LIBMTP_PROPERTY_PhoneNumberBusiness2,
  LIBMTP_PROPERTY_PhoneNumberMobile,
  LIBMTP_PROPERTY_PhoneNumberMobile2,
  LIBMTP_PROPERTY_FaxNumberPrimary,
  LIBMTP_PROPERTY_FaxNumberPersonal,
  LIBMTP_PROPERTY_FaxNumberBusiness,
  LIBMTP_PROPERTY_PagerNumber,
  LIBMTP_PROPERTY_PhoneNumberOthers,
  LIBMTP_PROPERTY_PrimaryWebAddress,
  LIBMTP_PROPERTY_PersonalWebAddress,
  LIBMTP_PROPERTY_BusinessWebAddress,
  LIBMTP_PROPERTY_InstantMessengerAddress,
  LIBMTP_PROPERTY_InstantMessengerAddress2,
  LIBMTP_PROPERTY_InstantMessengerAddress3,
  LIBMTP_PROPERTY_PostalAddressPersonalFull,
  LIBMTP_PROPERTY_PostalAddressPersonalFullLine1,
  LIBMTP_PROPERTY_PostalAddressPersonalFullLine2,
  LIBMTP_PROPERTY_PostalAddressPersonalFullCity,
  LIBMTP_PROPERTY_PostalAddressPersonalFullRegion,
  LIBMTP_PROPERTY_PostalAddressPersonalFullPostalCode,
  LIBMTP_PROPERTY_PostalAddressPersonalFullCountry,
  LIBMTP_PROPERTY_PostalAddressBusinessFull,
  LIBMTP_PROPERTY_PostalAddressBusinessLine1,
  LIBMTP_PROPERTY_PostalAddressBusinessLine2,
  LIBMTP_PROPERTY_PostalAddressBusinessCity,
  LIBMTP_PROPERTY_PostalAddressBusinessRegion,
  LIBMTP_PROPERTY_PostalAddressBusinessPostalCode,
  LIBMTP_PROPERTY_PostalAddressBusinessCountry,
  LIBMTP_PROPERTY_PostalAddressOtherFull,
  LIBMTP_PROPERTY_PostalAddressOtherLine1,
  LIBMTP_PROPERTY_PostalAddressOtherLine2,
  LIBMTP_PROPERTY_PostalAddressOtherCity,
  LIBMTP_PROPERTY_PostalAddressOtherRegion,
  LIBMTP_PROPERTY_PostalAddressOtherPostalCode,
  LIBMTP_PROPERTY_PostalAddressOtherCountry,
  LIBMTP_PROPERTY_OrganizationName,
  LIBMTP_PROPERTY_PhoneticOrganizationName,
  LIBMTP_PROPERTY_Role,
  LIBMTP_PROPERTY_Birthdate,
  LIBMTP_PROPERTY_MessageTo,
  LIBMTP_PROPERTY_MessageCC,
  LIBMTP_PROPERTY_MessageBCC,
  LIBMTP_PROPERTY_MessageRead,
  LIBMTP_PROPERTY_MessageReceivedTime,
  LIBMTP_PROPERTY_MessageSender,
  LIBMTP_PROPERTY_ActivityBeginTime,
  LIBMTP_PROPERTY_ActivityEndTime,
  LIBMTP_PROPERTY_ActivityLocation,
  LIBMTP_PROPERTY_ActivityRequiredAttendees,
  LIBMTP_PROPERTY_ActivityOptionalAttendees,
  LIBMTP_PROPERTY_ActivityResources,
  LIBMTP_PROPERTY_ActivityAccepted,
  LIBMTP_PROPERTY_Owner,
  LIBMTP_PROPERTY_Editor,
  LIBMTP_PROPERTY_Webmaster,
  LIBMTP_PROPERTY_URLSource,
  LIBMTP_PROPERTY_URLDestination,
  LIBMTP_PROPERTY_TimeBookmark,
  LIBMTP_PROPERTY_ObjectBookmark,
  LIBMTP_PROPERTY_ByteBookmark,
  LIBMTP_PROPERTY_LastBuildDate,
  LIBMTP_PROPERTY_TimetoLive,
  LIBMTP_PROPERTY_MediaGUID,
  LIBMTP_PROPERTY_TotalBitRate,
  LIBMTP_PROPERTY_BitRateType,
  LIBMTP_PROPERTY_SampleRate,
  LIBMTP_PROPERTY_NumberOfChannels,
  LIBMTP_PROPERTY_AudioBitDepth,
  LIBMTP_PROPERTY_ScanDepth,
  LIBMTP_PROPERTY_AudioWAVECodec,
  LIBMTP_PROPERTY_AudioBitRate,
  LIBMTP_PROPERTY_VideoFourCCCodec,
  LIBMTP_PROPERTY_VideoBitRate,
  LIBMTP_PROPERTY_FramesPerThousandSeconds,
  LIBMTP_PROPERTY_KeyFrameDistance,
  LIBMTP_PROPERTY_BufferSize,
  LIBMTP_PROPERTY_EncodingQuality,
  LIBMTP_PROPERTY_EncodingProfile,
  LIBMTP_PROPERTY_BuyFlag,
  LIBMTP_PROPERTY_UNKNOWN
} LIBMTP_property_t;

typedef enum {
  LIBMTP_DATATYPE_INT8,
  LIBMTP_DATATYPE_UINT8,
  LIBMTP_DATATYPE_INT16,
  LIBMTP_DATATYPE_UINT16,
  LIBMTP_DATATYPE_INT32,
  LIBMTP_DATATYPE_UINT32,
  LIBMTP_DATATYPE_INT64,
  LIBMTP_DATATYPE_UINT64,
} LIBMTP_datatype_t;

typedef enum {
  LIBMTP_ERROR_NONE,
  LIBMTP_ERROR_GENERAL,
  LIBMTP_ERROR_PTP_LAYER,
  LIBMTP_ERROR_USB_LAYER,
  LIBMTP_ERROR_MEMORY_ALLOCATION,
  LIBMTP_ERROR_NO_DEVICE_ATTACHED,
  LIBMTP_ERROR_STORAGE_FULL,
  LIBMTP_ERROR_CONNECTING,
  LIBMTP_ERROR_CANCELLED
} LIBMTP_error_number_t;
typedef struct LIBMTP_device_entry_struct LIBMTP_device_entry_t; 
typedef struct LIBMTP_raw_device_struct LIBMTP_raw_device_t; 
typedef struct LIBMTP_error_struct LIBMTP_error_t; 
typedef struct LIBMTP_allowed_values_struct LIBMTP_allowed_values_t; 
typedef struct LIBMTP_mtpdevice_struct LIBMTP_mtpdevice_t; 
typedef struct LIBMTP_file_struct LIBMTP_file_t; 
typedef struct LIBMTP_track_struct LIBMTP_track_t; 
typedef struct LIBMTP_playlist_struct LIBMTP_playlist_t; 
typedef struct LIBMTP_album_struct LIBMTP_album_t; 
typedef struct LIBMTP_folder_struct LIBMTP_folder_t; 
typedef struct LIBMTP_object_struct LIBMTP_object_t; 
typedef struct LIBMTP_filesampledata_struct LIBMTP_filesampledata_t; 
typedef struct LIBMTP_devicestorage_struct LIBMTP_devicestorage_t; 

typedef int (* LIBMTP_progressfunc_t) (uint64_t const sent, uint64_t const total,
                		void const * const data);

/**
 * Callback function for get by handler function
 * @param params the device parameters
 * @param priv a user-defined dereferencable pointer
 * @param wantlen the number of bytes wanted
 * @param data a buffer to write the data to
 * @param gotlen pointer to the number of bytes actually written
 *        to data
 * @return LIBMTP_HANDLER_RETURN_OK if successful,
 *         LIBMTP_HANDLER_RETURN_ERROR on error or
 *         LIBMTP_HANDLER_RETURN_CANCEL to cancel the transfer
 */
typedef uint16_t (* MTPDataGetFunc)	(void* params, void* priv,
					uint32_t wantlen, unsigned char *data, uint32_t *gotlen);

typedef uint16_t (* MTPDataPutFunc)	(void* params, void* priv,
					uint32_t sendlen, unsigned char *data, uint32_t *putlen);

#define LIBMTP_HANDLER_RETURN_OK 0
#define LIBMTP_HANDLER_RETURN_ERROR 1
#define LIBMTP_HANDLER_RETURN_CANCEL 2


struct LIBMTP_device_entry_struct {
  char *vendor; 
  uint16_t vendor_id; 
  char *product; 
  uint16_t product_id; 
  uint32_t device_flags; 
};

struct LIBMTP_raw_device_struct {
  LIBMTP_device_entry_t device_entry; 
  uint32_t bus_location; 
  uint8_t devnum; 
};

struct LIBMTP_error_struct {
  LIBMTP_error_number_t errornumber;
  char *error_text;
  LIBMTP_error_t *next;
};

struct LIBMTP_allowed_values_struct {
  uint8_t   u8max;
  uint8_t   u8min;
  uint8_t   u8step;
  uint8_t*  u8vals;
  int8_t    i8max;
  int8_t    i8min;
  int8_t    i8step;
  int8_t*   i8vals;
  uint16_t  u16max;
  uint16_t  u16min;
  uint16_t  u16step;
  uint16_t* u16vals;
  int16_t   i16max;
  int16_t   i16min;
  int16_t   i16step;
  int16_t*  i16vals;
  uint32_t  u32max;
  uint32_t  u32min;
  uint32_t  u32step;
  uint32_t* u32vals;
  int32_t   i32max;
  int32_t   i32min;
  int32_t   i32step;
  int32_t*  i32vals;
  uint64_t  u64max;
  uint64_t  u64min;
  uint64_t  u64step;
  uint64_t* u64vals;
  int64_t   i64max;
  int64_t   i64min;
  int64_t   i64step;
  int64_t*  i64vals;
  uint16_t  num_entries;
  LIBMTP_datatype_t datatype;
  int is_range;
};

struct LIBMTP_mtpdevice_struct {
  uint8_t object_bitsize;
  void *params;
  void *usbinfo;
  LIBMTP_devicestorage_t *storage;
  LIBMTP_error_t *errorstack;
  
  uint8_t maximum_battery_level;
  
  uint32_t default_music_folder;
  
  uint32_t default_playlist_folder;
  
  uint32_t default_picture_folder;
  
  uint32_t default_video_folder;
  
  uint32_t default_organizer_folder;
  
  uint32_t default_zencast_folder;
  
  uint32_t default_album_folder;
  
  uint32_t default_text_folder;
  
  void *cd;
  
  
  LIBMTP_mtpdevice_t *next;
};

struct LIBMTP_file_struct {
  uint32_t item_id; 
  uint32_t parent_id; 
  uint32_t storage_id; 
  char *filename; 
  uint64_t filesize; 
  time_t modificationdate; 
  LIBMTP_filetype_t filetype; 
  LIBMTP_file_t *next; 
};

struct LIBMTP_track_struct {
  uint32_t item_id; 
  uint32_t parent_id; 
  uint32_t storage_id; 
  char *title; 
  char *artist; 
  char *composer; 
  char *genre; 
  char *album; 
  char *date; 
  char *filename; 
  uint16_t tracknumber; 
  uint32_t duration; 
  uint32_t samplerate; 
  uint16_t nochannels; 
  uint32_t wavecodec; 
  uint32_t bitrate; 
  uint16_t bitratetype; 
  uint16_t rating; 
  uint32_t usecount; 
  uint64_t filesize; 
  time_t modificationdate; 
  LIBMTP_filetype_t filetype; 
  LIBMTP_track_t *next; 
};

struct LIBMTP_playlist_struct {
  uint32_t playlist_id; 
  uint32_t parent_id; 
  uint32_t storage_id; 
  char *name; 
  uint32_t *tracks; 
  uint32_t no_tracks; 
  LIBMTP_playlist_t *next; 
};

struct LIBMTP_album_struct {
  uint32_t album_id; 
  uint32_t parent_id; 
  uint32_t storage_id; 
  char *name; 
  char *artist; 
  char *composer; 
  char *genre; 
  uint32_t *tracks; 
  uint32_t no_tracks; 
  LIBMTP_album_t *next; 
};

struct LIBMTP_folder_struct {
  uint32_t folder_id; 
  uint32_t parent_id; 
  uint32_t storage_id; 
  char *name; 
  LIBMTP_folder_t *sibling; 
  LIBMTP_folder_t *child; 
};

struct LIBMTP_filesampledata_struct {
  uint32_t width; 
  uint32_t height; 
  uint32_t duration; 
  LIBMTP_filetype_t filetype; 
  uint64_t size; 
  char *data; 
};

struct LIBMTP_devicestorage_struct {
  uint32_t id; 
  uint16_t StorageType; 
  uint16_t FilesystemType; 
  uint16_t AccessCapability; 
  uint64_t MaxCapacity; 
  uint64_t FreeSpaceInBytes; 
  uint64_t FreeSpaceInObjects; 
  char *StorageDescription; 
  char *VolumeIdentifier; 
  LIBMTP_devicestorage_t *next; 
  LIBMTP_devicestorage_t *prev; 
};
  


#ifdef __cplusplus
extern "C" {
#endif

void LIBMTP_Init(void);
int LIBMTP_Get_Supported_Devices_List(LIBMTP_device_entry_t ** const, int * const);
LIBMTP_error_number_t LIBMTP_Detect_Raw_Devices(LIBMTP_raw_device_t **, int *);
LIBMTP_mtpdevice_t *LIBMTP_Open_Raw_Device(LIBMTP_raw_device_t *);
LIBMTP_mtpdevice_t *LIBMTP_Get_First_Device(void);
LIBMTP_error_number_t LIBMTP_Get_Connected_Devices(LIBMTP_mtpdevice_t **);
uint32_t LIBMTP_Number_Devices_In_List(LIBMTP_mtpdevice_t *);
void LIBMTP_Release_Device_List(LIBMTP_mtpdevice_t*);
void LIBMTP_Release_Device(LIBMTP_mtpdevice_t*);
void LIBMTP_Dump_Device_Info(LIBMTP_mtpdevice_t*);
int LIBMTP_Reset_Device(LIBMTP_mtpdevice_t*);
char *LIBMTP_Get_Manufacturername(LIBMTP_mtpdevice_t*);
char *LIBMTP_Get_Modelname(LIBMTP_mtpdevice_t*);
char *LIBMTP_Get_Serialnumber(LIBMTP_mtpdevice_t*);
char *LIBMTP_Get_Deviceversion(LIBMTP_mtpdevice_t*);
char *LIBMTP_Get_Friendlyname(LIBMTP_mtpdevice_t*);
int LIBMTP_Set_Friendlyname(LIBMTP_mtpdevice_t*, char const * const);
char *LIBMTP_Get_Syncpartner(LIBMTP_mtpdevice_t*);
int LIBMTP_Set_Syncpartner(LIBMTP_mtpdevice_t*, char const * const);
int LIBMTP_Get_Batterylevel(LIBMTP_mtpdevice_t *,
			    uint8_t * const,
			    uint8_t * const);
int LIBMTP_Get_Secure_Time(LIBMTP_mtpdevice_t *, char ** const);
int LIBMTP_Get_Device_Certificate(LIBMTP_mtpdevice_t *, char ** const);
int LIBMTP_Get_Supported_Filetypes(LIBMTP_mtpdevice_t *, uint16_t ** const, uint16_t * const);
LIBMTP_error_t *LIBMTP_Get_Errorstack(LIBMTP_mtpdevice_t*);
void LIBMTP_Clear_Errorstack(LIBMTP_mtpdevice_t*);
void LIBMTP_Dump_Errorstack(LIBMTP_mtpdevice_t*);

void LIBMTP_Set_Device_Timeout(LIBMTP_mtpdevice_t *device, int milliseconds);
void LIBMTP_Get_Device_Timeout(LIBMTP_mtpdevice_t *device, int * milliseconds);

#define LIBMTP_STORAGE_SORTBY_NOTSORTED 0
#define LIBMTP_STORAGE_SORTBY_FREESPACE 1
#define LIBMTP_STORAGE_SORTBY_MAXSPACE  2

int LIBMTP_Get_Storage(LIBMTP_mtpdevice_t *, int const);
int LIBMTP_Format_Storage(LIBMTP_mtpdevice_t *, LIBMTP_devicestorage_t *);

char *LIBMTP_Get_String_From_Object(LIBMTP_mtpdevice_t *, uint32_t const, LIBMTP_property_t const);
uint64_t LIBMTP_Get_u64_From_Object(LIBMTP_mtpdevice_t *, uint32_t const,
      LIBMTP_property_t const, uint64_t const);
uint32_t LIBMTP_Get_u32_From_Object(LIBMTP_mtpdevice_t *, uint32_t const,
      LIBMTP_property_t const, uint32_t const);
uint16_t LIBMTP_Get_u16_From_Object(LIBMTP_mtpdevice_t *, uint32_t const,
      LIBMTP_property_t const, uint16_t const);
uint8_t LIBMTP_Get_u8_From_Object(LIBMTP_mtpdevice_t *, uint32_t const,
      LIBMTP_property_t const, uint8_t const);
int LIBMTP_Set_Object_String(LIBMTP_mtpdevice_t *, uint32_t const,
      LIBMTP_property_t const, char const * const);
int LIBMTP_Set_Object_u32(LIBMTP_mtpdevice_t *, uint32_t const,
      LIBMTP_property_t const, uint32_t const);
int LIBMTP_Set_Object_u16(LIBMTP_mtpdevice_t *, uint32_t const,
      LIBMTP_property_t const, uint16_t const);
int LIBMTP_Set_Object_u8(LIBMTP_mtpdevice_t *, uint32_t const,
      LIBMTP_property_t const, uint8_t const);
char const * LIBMTP_Get_Property_Description(LIBMTP_property_t inproperty);
int LIBMTP_Is_Property_Supported(LIBMTP_mtpdevice_t*, LIBMTP_property_t const,
            LIBMTP_filetype_t const);
int LIBMTP_Get_Allowed_Property_Values(LIBMTP_mtpdevice_t*, LIBMTP_property_t const,
            LIBMTP_filetype_t const, LIBMTP_allowed_values_t*);
void LIBMTP_destroy_allowed_values_t(LIBMTP_allowed_values_t*);

LIBMTP_file_t *LIBMTP_new_file_t(void);
void LIBMTP_destroy_file_t(LIBMTP_file_t*);
char const * LIBMTP_Get_Filetype_Description(LIBMTP_filetype_t);
LIBMTP_file_t *LIBMTP_Get_Filelisting(LIBMTP_mtpdevice_t *);
LIBMTP_file_t *LIBMTP_Get_Filelisting_With_Callback(LIBMTP_mtpdevice_t *,
      LIBMTP_progressfunc_t const, void const * const);
LIBMTP_file_t *LIBMTP_Get_Filemetadata(LIBMTP_mtpdevice_t *, uint32_t const);
int LIBMTP_Get_File_To_File(LIBMTP_mtpdevice_t*, uint32_t, char const * const,
			LIBMTP_progressfunc_t const, void const * const);
int LIBMTP_Get_File_To_File_Descriptor(LIBMTP_mtpdevice_t*, uint32_t const, int const,
			LIBMTP_progressfunc_t const, void const * const, struct utimbuf * mtime);
int LIBMTP_Get_File_To_Handler(LIBMTP_mtpdevice_t *, uint32_t const, MTPDataPutFunc, void *,
                   LIBMTP_progressfunc_t const, void const * const);
int LIBMTP_Send_File_From_File(LIBMTP_mtpdevice_t *, char const * const,
	                 LIBMTP_file_t * const, LIBMTP_progressfunc_t const,
			 void const * const);
int LIBMTP_Send_File_From_File_Descriptor(LIBMTP_mtpdevice_t *, int const,
	                LIBMTP_file_t * const, LIBMTP_progressfunc_t const,
			void const * const);
int LIBMTP_Send_File_From_Handler(LIBMTP_mtpdevice_t *, MTPDataGetFunc, void *,
      LIBMTP_file_t * const, LIBMTP_progressfunc_t const, void const * const);
int LIBMTP_Set_File_Name(LIBMTP_mtpdevice_t *, LIBMTP_file_t *, const char *);
LIBMTP_filesampledata_t *LIBMTP_new_filesampledata_t(void);
void LIBMTP_destroy_filesampledata_t(LIBMTP_filesampledata_t *);
int LIBMTP_Get_Representative_Sample_Format(LIBMTP_mtpdevice_t *,
                        LIBMTP_filetype_t const,
                        LIBMTP_filesampledata_t **);
int LIBMTP_Send_Representative_Sample(LIBMTP_mtpdevice_t *, uint32_t const,
                          LIBMTP_filesampledata_t *);
int LIBMTP_Get_Representative_Sample(LIBMTP_mtpdevice_t *, uint32_t const,
                          LIBMTP_filesampledata_t *);


void LIBMTP_Set_Load_Cache_On_Demand(int flag);

LIBMTP_file_t * LIBMTP_Get_Files_And_Folders(LIBMTP_mtpdevice_t *device,
      uint32_t storageId, uint32_t parentId);

LIBMTP_track_t *LIBMTP_new_track_t(void);
void LIBMTP_destroy_track_t(LIBMTP_track_t*);
LIBMTP_track_t *LIBMTP_Get_Tracklisting(LIBMTP_mtpdevice_t*);
LIBMTP_track_t *LIBMTP_Get_Tracklisting_With_Callback(LIBMTP_mtpdevice_t*,
      LIBMTP_progressfunc_t const, void const * const);
LIBMTP_track_t *LIBMTP_Get_Trackmetadata(LIBMTP_mtpdevice_t*, uint32_t const);
int LIBMTP_Get_Track_To_File(LIBMTP_mtpdevice_t*, uint32_t, char const * const,
			LIBMTP_progressfunc_t const, void const * const);
int LIBMTP_Get_Track_To_File_Descriptor(LIBMTP_mtpdevice_t*, uint32_t const, int const,
			LIBMTP_progressfunc_t const, void const * const, struct utimbuf * mtime);
int LIBMTP_Get_Track_To_Handler(LIBMTP_mtpdevice_t *, uint32_t const, MTPDataPutFunc,
      void *, LIBMTP_progressfunc_t const, void const * const);
int LIBMTP_Send_Track_From_File(LIBMTP_mtpdevice_t *,
			 char const * const, LIBMTP_track_t * const,
                         LIBMTP_progressfunc_t const,
			 void const * const);
int LIBMTP_Send_Track_From_File_Descriptor(LIBMTP_mtpdevice_t *,
			 int const, LIBMTP_track_t * const,
                         LIBMTP_progressfunc_t const,
			 void const * const);
int LIBMTP_Send_Track_From_Handler(LIBMTP_mtpdevice_t *,
			 MTPDataGetFunc, void *, LIBMTP_track_t * const,
                         LIBMTP_progressfunc_t const,
			 void const * const);
int LIBMTP_Update_Track_Metadata(LIBMTP_mtpdevice_t *,
			LIBMTP_track_t const * const);
int LIBMTP_Track_Exists(LIBMTP_mtpdevice_t *, uint32_t);
int LIBMTP_Set_Track_Name(LIBMTP_mtpdevice_t *, LIBMTP_track_t *, const char *);

LIBMTP_folder_t *LIBMTP_new_folder_t(void);
void LIBMTP_destroy_folder_t(LIBMTP_folder_t*);
LIBMTP_folder_t *LIBMTP_Get_Folder_List(LIBMTP_mtpdevice_t*);
LIBMTP_folder_t *LIBMTP_Find_Folder(LIBMTP_folder_t*, uint32_t const);
uint32_t LIBMTP_Create_Folder(LIBMTP_mtpdevice_t*, char *, uint32_t, uint32_t);
int LIBMTP_Set_Folder_Name(LIBMTP_mtpdevice_t *, LIBMTP_folder_t *, const char *);


LIBMTP_playlist_t *LIBMTP_new_playlist_t(void);
void LIBMTP_destroy_playlist_t(LIBMTP_playlist_t *);
LIBMTP_playlist_t *LIBMTP_Get_Playlist_List(LIBMTP_mtpdevice_t *);
LIBMTP_playlist_t *LIBMTP_Get_Playlist(LIBMTP_mtpdevice_t *, uint32_t const);
int LIBMTP_Create_New_Playlist(LIBMTP_mtpdevice_t *, LIBMTP_playlist_t * const);
int LIBMTP_Update_Playlist(LIBMTP_mtpdevice_t *, LIBMTP_playlist_t * const);
int LIBMTP_Set_Playlist_Name(LIBMTP_mtpdevice_t *, LIBMTP_playlist_t *, const char *);

LIBMTP_album_t *LIBMTP_new_album_t(void);
void LIBMTP_destroy_album_t(LIBMTP_album_t *);
LIBMTP_album_t *LIBMTP_Get_Album_List(LIBMTP_mtpdevice_t *);
LIBMTP_album_t *LIBMTP_Get_Album(LIBMTP_mtpdevice_t *, uint32_t const);
int LIBMTP_Create_New_Album(LIBMTP_mtpdevice_t *, LIBMTP_album_t * const);
int LIBMTP_Update_Album(LIBMTP_mtpdevice_t *, LIBMTP_album_t const * const);
int LIBMTP_Set_Album_Name(LIBMTP_mtpdevice_t *, LIBMTP_album_t *, const char *);

int LIBMTP_Delete_Object(LIBMTP_mtpdevice_t *, uint32_t);
int LIBMTP_Set_Object_Filename(LIBMTP_mtpdevice_t *, uint32_t , char *);


#ifdef __cplusplus
}
#endif

#endif 


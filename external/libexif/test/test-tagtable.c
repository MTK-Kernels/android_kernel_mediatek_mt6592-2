/* test-tagtable.c
 *
 * Test various functions that involve the tag table.
 *
 * Copyright (c) 2009 Dan Fandrich <dan@coneharvesters.com>
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

#include "config.h"
#include <libexif/exif-tag.h>
#include <stdio.h>
#include <string.h>

#define VALIDATE(s) if (!(s)) {printf("Test %s FAILED\n", #s); fail=1;}

#define TESTBLOCK(t) {int rc = (t); fail |= rc; \
                      if (rc) printf("%s tests FAILED\n", #t);}

static int support_level(void)
{
    int fail = 0;

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_PLANAR_CONFIGURATION,
               EXIF_IFD_0, EXIF_DATA_TYPE_UNCOMPRESSED_CHUNKY) == 
             EXIF_SUPPORT_LEVEL_OPTIONAL)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_PLANAR_CONFIGURATION,
               EXIF_IFD_0, EXIF_DATA_TYPE_UNCOMPRESSED_PLANAR) == 
             EXIF_SUPPORT_LEVEL_MANDATORY)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_PLANAR_CONFIGURATION,
               EXIF_IFD_0, EXIF_DATA_TYPE_UNCOMPRESSED_YCC) == 
             EXIF_SUPPORT_LEVEL_OPTIONAL)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_PLANAR_CONFIGURATION,
               EXIF_IFD_0, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_NOT_RECORDED)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_PLANAR_CONFIGURATION,
               EXIF_IFD_0, EXIF_DATA_TYPE_UNKNOWN) == 
             EXIF_SUPPORT_LEVEL_UNKNOWN)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_YCBCR_POSITIONING,
               EXIF_IFD_0, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_MANDATORY)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_YCBCR_POSITIONING,
               EXIF_IFD_1, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_OPTIONAL)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_YCBCR_POSITIONING,
               EXIF_IFD_EXIF, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_NOT_RECORDED)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_YCBCR_POSITIONING,
               EXIF_IFD_GPS, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_NOT_RECORDED)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_YCBCR_POSITIONING,
               EXIF_IFD_INTEROPERABILITY, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_NOT_RECORDED)


    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_GPS_VERSION_ID,
               EXIF_IFD_GPS, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_OPTIONAL)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_GPS_VERSION_ID,
               EXIF_IFD_GPS, EXIF_DATA_TYPE_UNKNOWN) == 
             EXIF_SUPPORT_LEVEL_OPTIONAL)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_GPS_VERSION_ID,
               EXIF_IFD_0, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_NOT_RECORDED)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_GPS_VERSION_ID,
               EXIF_IFD_0, EXIF_DATA_TYPE_UNKNOWN) == 
             EXIF_SUPPORT_LEVEL_UNKNOWN)

    
    VALIDATE(exif_tag_get_support_level_in_ifd(0xffff,
               EXIF_IFD_0, EXIF_DATA_TYPE_UNKNOWN) == 
             EXIF_SUPPORT_LEVEL_UNKNOWN)

    VALIDATE(exif_tag_get_support_level_in_ifd(0xffff,
               EXIF_IFD_0, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_NOT_RECORDED)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_DOCUMENT_NAME,
               EXIF_IFD_0, EXIF_DATA_TYPE_UNKNOWN) == 
             EXIF_SUPPORT_LEVEL_UNKNOWN)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_DOCUMENT_NAME,
               EXIF_IFD_1, EXIF_DATA_TYPE_UNCOMPRESSED_CHUNKY) == 
             EXIF_SUPPORT_LEVEL_UNKNOWN)


    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_INTEROPERABILITY_INDEX,
               EXIF_IFD_INTEROPERABILITY, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_OPTIONAL)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_INTEROPERABILITY_INDEX,
               EXIF_IFD_INTEROPERABILITY, EXIF_DATA_TYPE_UNKNOWN) == 
             EXIF_SUPPORT_LEVEL_OPTIONAL)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_INTEROPERABILITY_INDEX,
               EXIF_IFD_0, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_NOT_RECORDED)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_INTEROPERABILITY_INDEX,
               EXIF_IFD_0, EXIF_DATA_TYPE_UNKNOWN) == 
             EXIF_SUPPORT_LEVEL_UNKNOWN)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_GPS_LATITUDE_REF,
               EXIF_IFD_GPS, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_OPTIONAL)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_GPS_LATITUDE_REF,
               EXIF_IFD_GPS, EXIF_DATA_TYPE_UNKNOWN) == 
             EXIF_SUPPORT_LEVEL_OPTIONAL)

    
    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_EXIF_VERSION,
               EXIF_IFD_COUNT, EXIF_DATA_TYPE_UNKNOWN) == 
             EXIF_SUPPORT_LEVEL_UNKNOWN)

    VALIDATE(exif_tag_get_support_level_in_ifd(EXIF_TAG_EXIF_VERSION,
               EXIF_IFD_COUNT, EXIF_DATA_TYPE_COMPRESSED) == 
             EXIF_SUPPORT_LEVEL_UNKNOWN)

    return fail;
}

static int name(void)
{
    int fail = 0;

    VALIDATE(!strcmp(exif_tag_get_name_in_ifd(
                        EXIF_TAG_GPS_VERSION_ID, EXIF_IFD_GPS), 
                     "GPSVersionID"))

    VALIDATE(exif_tag_get_name_in_ifd(
                        EXIF_TAG_GPS_VERSION_ID, EXIF_IFD_0) == NULL)

    VALIDATE(!strcmp(exif_tag_get_name_in_ifd(
                EXIF_TAG_INTEROPERABILITY_INDEX, EXIF_IFD_INTEROPERABILITY), 
                     "InteroperabilityIndex"))

    VALIDATE(!strcmp(exif_tag_get_name_in_ifd(
                        EXIF_TAG_GPS_LATITUDE_REF, EXIF_IFD_GPS), 
                     "GPSLatitudeRef"))

    
    VALIDATE(exif_tag_get_name_in_ifd(
                        EXIF_TAG_EXIF_VERSION, EXIF_IFD_0) == NULL)

    
    VALIDATE(exif_tag_get_name_in_ifd(
                        EXIF_TAG_EXIF_VERSION, EXIF_IFD_COUNT) == NULL)

    return fail;
}

int
main ()
{
    int fail = 0;

    TESTBLOCK(support_level())
    TESTBLOCK(name())

    return fail;
}

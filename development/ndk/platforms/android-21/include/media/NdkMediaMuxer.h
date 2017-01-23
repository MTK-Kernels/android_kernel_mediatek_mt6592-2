/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



#ifndef _NDK_MEDIA_MUXER_H
#define _NDK_MEDIA_MUXER_H

#include <sys/types.h>

#include "NdkMediaCodec.h"
#include "NdkMediaError.h"
#include "NdkMediaFormat.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AMediaMuxer;
typedef struct AMediaMuxer AMediaMuxer;

typedef enum {
    AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4 = 0,
    AMEDIAMUXER_OUTPUT_FORMAT_WEBM   = 1,
} OutputFormat;

AMediaMuxer* AMediaMuxer_new(int fd, OutputFormat format);

media_status_t AMediaMuxer_delete(AMediaMuxer*);

media_status_t AMediaMuxer_setLocation(AMediaMuxer*, float latitude, float longitude);

media_status_t AMediaMuxer_setOrientationHint(AMediaMuxer*, int degrees);

ssize_t AMediaMuxer_addTrack(AMediaMuxer*, const AMediaFormat* format);

media_status_t AMediaMuxer_start(AMediaMuxer*);

media_status_t AMediaMuxer_stop(AMediaMuxer*);

/**
 * Writes an encoded sample into the muxer.
 * The application needs to make sure that the samples are written into
 * the right tracks. Also, it needs to make sure the samples for each track
 * are written in chronological order (e.g. in the order they are provided
 * by the encoder.)
 */
media_status_t AMediaMuxer_writeSampleData(AMediaMuxer *muxer,
        size_t trackIdx, const uint8_t *data, const AMediaCodecBufferInfo *info);

#ifdef __cplusplus
} 
#endif

#endif 

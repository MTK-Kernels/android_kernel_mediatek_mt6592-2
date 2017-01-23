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



#ifndef _NDK_MEDIA_EXTRACTOR_H
#define _NDK_MEDIA_EXTRACTOR_H

#include <sys/types.h>

#include "NdkMediaCodec.h"
#include "NdkMediaFormat.h"
#include "NdkMediaCrypto.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AMediaExtractor;
typedef struct AMediaExtractor AMediaExtractor;


AMediaExtractor* AMediaExtractor_new();

media_status_t AMediaExtractor_delete(AMediaExtractor*);

media_status_t AMediaExtractor_setDataSourceFd(AMediaExtractor*, int fd, off64_t offset, off64_t length);

media_status_t AMediaExtractor_setDataSource(AMediaExtractor*, const char *location); 

size_t AMediaExtractor_getTrackCount(AMediaExtractor*);

AMediaFormat* AMediaExtractor_getTrackFormat(AMediaExtractor*, size_t idx);

media_status_t AMediaExtractor_selectTrack(AMediaExtractor*, size_t idx);

media_status_t AMediaExtractor_unselectTrack(AMediaExtractor*, size_t idx);

ssize_t AMediaExtractor_readSampleData(AMediaExtractor*, uint8_t *buffer, size_t capacity);

uint32_t AMediaExtractor_getSampleFlags(AMediaExtractor*); 

int AMediaExtractor_getSampleTrackIndex(AMediaExtractor*);

int64_t AMediaExtractor_getSampleTime(AMediaExtractor*);

bool AMediaExtractor_advance(AMediaExtractor*);

typedef enum {
    AMEDIAEXTRACTOR_SEEK_PREVIOUS_SYNC,
    AMEDIAEXTRACTOR_SEEK_NEXT_SYNC,
    AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC
} SeekMode;

media_status_t AMediaExtractor_seekTo(AMediaExtractor*, int64_t seekPosUs, SeekMode mode);

typedef struct PsshEntry {
    AMediaUUID uuid;
    size_t datalen;
    void *data;
} PsshEntry;

typedef struct PsshInfo {
    size_t numentries;
    PsshEntry entries[0];
} PsshInfo;

PsshInfo* AMediaExtractor_getPsshInfo(AMediaExtractor*);


AMediaCodecCryptoInfo *AMediaExtractor_getSampleCryptoInfo(AMediaExtractor *);


enum {
    AMEDIAEXTRACTOR_SAMPLE_FLAG_SYNC = 1,
    AMEDIAEXTRACTOR_SAMPLE_FLAG_ENCRYPTED = 2,
};

#ifdef __cplusplus
} 
#endif

#endif 

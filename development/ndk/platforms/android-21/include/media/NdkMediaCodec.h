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


#ifndef _NDK_MEDIA_CODEC_H
#define _NDK_MEDIA_CODEC_H

#include <android/native_window.h>

#include "NdkMediaCrypto.h"
#include "NdkMediaError.h"
#include "NdkMediaFormat.h"

#ifdef __cplusplus
extern "C" {
#endif


struct AMediaCodec;
typedef struct AMediaCodec AMediaCodec;

struct AMediaCodecBufferInfo {
    int32_t offset;
    int32_t size;
    int64_t presentationTimeUs;
    uint32_t flags;
};
typedef struct AMediaCodecBufferInfo AMediaCodecBufferInfo;
typedef struct AMediaCodecCryptoInfo AMediaCodecCryptoInfo;

enum {
    AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM = 4,
    AMEDIACODEC_CONFIGURE_FLAG_ENCODE = 1,
    AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED = -3,
    AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED = -2,
    AMEDIACODEC_INFO_TRY_AGAIN_LATER = -1
};

AMediaCodec* AMediaCodec_createCodecByName(const char *name);

AMediaCodec* AMediaCodec_createDecoderByType(const char *mime_type);

AMediaCodec* AMediaCodec_createEncoderByType(const char *mime_type);

media_status_t AMediaCodec_delete(AMediaCodec*);

media_status_t AMediaCodec_configure(
        AMediaCodec*,
        const AMediaFormat* format,
        ANativeWindow* surface,
        AMediaCrypto *crypto,
        uint32_t flags);

media_status_t AMediaCodec_start(AMediaCodec*);

media_status_t AMediaCodec_stop(AMediaCodec*);

media_status_t AMediaCodec_flush(AMediaCodec*);

uint8_t* AMediaCodec_getInputBuffer(AMediaCodec*, size_t idx, size_t *out_size);

uint8_t* AMediaCodec_getOutputBuffer(AMediaCodec*, size_t idx, size_t *out_size);

ssize_t AMediaCodec_dequeueInputBuffer(AMediaCodec*, int64_t timeoutUs);

media_status_t AMediaCodec_queueInputBuffer(AMediaCodec*,
        size_t idx, off_t offset, size_t size, uint64_t time, uint32_t flags);

media_status_t AMediaCodec_queueSecureInputBuffer(AMediaCodec*,
        size_t idx, off_t offset, AMediaCodecCryptoInfo*, uint64_t time, uint32_t flags);

ssize_t AMediaCodec_dequeueOutputBuffer(AMediaCodec*, AMediaCodecBufferInfo *info, int64_t timeoutUs);
AMediaFormat* AMediaCodec_getOutputFormat(AMediaCodec*);

media_status_t AMediaCodec_releaseOutputBuffer(AMediaCodec*, size_t idx, bool render);

media_status_t AMediaCodec_releaseOutputBufferAtTime(
        AMediaCodec *mData, size_t idx, int64_t timestampNs);


typedef enum {
    AMEDIACODECRYPTOINFO_MODE_CLEAR = 0,
    AMEDIACODECRYPTOINFO_MODE_AES_CTR = 1
} cryptoinfo_mode_t;

AMediaCodecCryptoInfo *AMediaCodecCryptoInfo_new(
        int numsubsamples,
        uint8_t key[16],
        uint8_t iv[16],
        cryptoinfo_mode_t mode,
        size_t *clearbytes,
        size_t *encryptedbytes);

media_status_t AMediaCodecCryptoInfo_delete(AMediaCodecCryptoInfo*);

size_t AMediaCodecCryptoInfo_getNumSubSamples(AMediaCodecCryptoInfo*);

media_status_t AMediaCodecCryptoInfo_getKey(AMediaCodecCryptoInfo*, uint8_t *dst);

media_status_t AMediaCodecCryptoInfo_getIV(AMediaCodecCryptoInfo*, uint8_t *dst);

cryptoinfo_mode_t AMediaCodecCryptoInfo_getMode(AMediaCodecCryptoInfo*);

media_status_t AMediaCodecCryptoInfo_getClearBytes(AMediaCodecCryptoInfo*, size_t *dst);

media_status_t AMediaCodecCryptoInfo_getEncryptedBytes(AMediaCodecCryptoInfo*, size_t *dst);

#ifdef __cplusplus
} 
#endif

#endif 

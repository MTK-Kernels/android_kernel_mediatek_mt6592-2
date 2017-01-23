/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_READER_H_
#define VIDEO_READER_H_

#include "./video_common.h"

struct VpxVideoReaderStruct;
typedef struct VpxVideoReaderStruct VpxVideoReader;

#ifdef __cplusplus
extern "C" {
#endif

VpxVideoReader *vpx_video_reader_open(const char *filename);

void vpx_video_reader_close(VpxVideoReader *reader);

int vpx_video_reader_read_frame(VpxVideoReader *reader);

const uint8_t *vpx_video_reader_get_frame(VpxVideoReader *reader,
                                          size_t *size);

const VpxVideoInfo *vpx_video_reader_get_info(VpxVideoReader *reader);

#ifdef __cplusplus
}  
#endif

#endif  

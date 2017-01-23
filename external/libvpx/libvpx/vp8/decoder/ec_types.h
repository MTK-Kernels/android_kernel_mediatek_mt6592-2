/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP8_DECODER_EC_TYPES_H_
#define VP8_DECODER_EC_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_OVERLAPS 16


typedef struct
{
    int overlap;
    union b_mode_info *bmi;
} OVERLAP_NODE;

typedef struct
{
    
    OVERLAP_NODE overlaps[MAX_OVERLAPS];
} B_OVERLAP;

typedef struct
{
    B_OVERLAP overlaps[16];
} MB_OVERLAP;

typedef struct
{
    MV mv;
    MV_REFERENCE_FRAME ref_frame;
} EC_BLOCK;

#ifdef __cplusplus
}  
#endif

#endif  

/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_ENCODER_VP9_LOOKAHEAD_H_
#define VP9_ENCODER_VP9_LOOKAHEAD_H_

#include "vpx_scale/yv12config.h"
#include "vpx/vpx_integer.h"

#if CONFIG_SPATIAL_SVC
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LAG_BUFFERS 25

struct lookahead_entry {
  YV12_BUFFER_CONFIG  img;
  int64_t             ts_start;
  int64_t             ts_end;
  unsigned int        flags;
};

#define MAX_PRE_FRAMES 1

struct lookahead_ctx {
  unsigned int max_sz;         
  unsigned int sz;             
  unsigned int read_idx;       
  unsigned int write_idx;      
  struct lookahead_entry *buf; 
};

struct lookahead_ctx *vp9_lookahead_init(unsigned int width,
                                         unsigned int height,
                                         unsigned int subsampling_x,
                                         unsigned int subsampling_y,
#if CONFIG_VP9_HIGHBITDEPTH
                                         int use_highbitdepth,
#endif
                                         unsigned int depth);


void vp9_lookahead_destroy(struct lookahead_ctx *ctx);


int vp9_lookahead_push(struct lookahead_ctx *ctx, YV12_BUFFER_CONFIG *src,
                       int64_t ts_start, int64_t ts_end,
#if CONFIG_VP9_HIGHBITDEPTH
                       int use_highbitdepth,
#endif
                       unsigned int flags);


struct lookahead_entry *vp9_lookahead_pop(struct lookahead_ctx *ctx,
                                          int drain);


struct lookahead_entry *vp9_lookahead_peek(struct lookahead_ctx *ctx,
                                           int index);


unsigned int vp9_lookahead_depth(struct lookahead_ctx *ctx);

#ifdef __cplusplus
}  
#endif

#endif  

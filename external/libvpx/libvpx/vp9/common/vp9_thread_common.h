/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VP9_COMMON_VP9_LOOPFILTER_THREAD_H_
#define VP9_COMMON_VP9_LOOPFILTER_THREAD_H_
#include "./vpx_config.h"
#include "vp9/common/vp9_loopfilter.h"
#include "vpx_util/vpx_thread.h"

struct VP9Common;
struct FRAME_COUNTS;

typedef struct VP9LfSyncData {
#if CONFIG_MULTITHREAD
  pthread_mutex_t *mutex_;
  pthread_cond_t *cond_;
#endif
  
  int *cur_sb_col;
  
  
  int sync_range;
  int rows;

  
  LFWorkerData *lfdata;
  int num_workers;
} VP9LfSync;

void vp9_loop_filter_alloc(VP9LfSync *lf_sync, struct VP9Common *cm, int rows,
                           int width, int num_workers);

void vp9_loop_filter_dealloc(VP9LfSync *lf_sync);

void vp9_loop_filter_frame_mt(YV12_BUFFER_CONFIG *frame,
                              struct VP9Common *cm,
                              struct macroblockd_plane planes[MAX_MB_PLANE],
                              int frame_filter_level,
                              int y_only, int partial_frame,
                              VPxWorker *workers, int num_workers,
                              VP9LfSync *lf_sync);

void vp9_accumulate_frame_counts(struct VP9Common *cm,
                                 struct FRAME_COUNTS *counts, int is_dec);

#endif  

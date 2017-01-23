/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP9_ENCODER_VP9_AQ_CYCLICREFRESH_H_
#define VP9_ENCODER_VP9_AQ_CYCLICREFRESH_H_

#include "vp9/common/vp9_blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CR_SEGMENT_ID_BASE    0
#define CR_SEGMENT_ID_BOOST1  1
#define CR_SEGMENT_ID_BOOST2  2

#define CR_MAX_RATE_TARGET_RATIO 4.0

struct VP9_COMP;

struct CYCLIC_REFRESH;
typedef struct CYCLIC_REFRESH CYCLIC_REFRESH;

CYCLIC_REFRESH *vp9_cyclic_refresh_alloc(int mi_rows, int mi_cols);

void vp9_cyclic_refresh_free(CYCLIC_REFRESH *cr);

int vp9_cyclic_refresh_estimate_bits_at_q(const struct VP9_COMP *cpi,
                                          double correction_factor);

int vp9_cyclic_refresh_rc_bits_per_mb(const struct VP9_COMP *cpi, int i,
                                      double correction_factor);

void vp9_cyclic_refresh_update_segment(struct VP9_COMP *const cpi,
                                       MB_MODE_INFO *const mbmi,
                                       int mi_row, int mi_col, BLOCK_SIZE bsize,
                                       int64_t rate, int64_t dist, int skip);

void vp9_cyclic_refresh_update__map(struct VP9_COMP *const cpi);

void vp9_cyclic_refresh_postencode(struct VP9_COMP *const cpi);

void vp9_cyclic_refresh_set_golden_update(struct VP9_COMP *const cpi);

void vp9_cyclic_refresh_check_golden_update(struct VP9_COMP *const cpi);

void vp9_cyclic_refresh_update_parameters(struct VP9_COMP *const cpi);

void vp9_cyclic_refresh_setup(struct VP9_COMP *const cpi);

int vp9_cyclic_refresh_get_rdmult(const CYCLIC_REFRESH *cr);

void vp9_cyclic_refresh_reset_resize(struct VP9_COMP *const cpi);

static INLINE int cyclic_refresh_segment_id_boosted(int segment_id) {
  return segment_id == CR_SEGMENT_ID_BOOST1 ||
         segment_id == CR_SEGMENT_ID_BOOST2;
}

static INLINE int cyclic_refresh_segment_id(int segment_id) {
  if (segment_id == CR_SEGMENT_ID_BOOST1)
    return CR_SEGMENT_ID_BOOST1;
  else if (segment_id == CR_SEGMENT_ID_BOOST2)
    return CR_SEGMENT_ID_BOOST2;
  else
    return CR_SEGMENT_ID_BASE;
}

#ifdef __cplusplus
}  
#endif

#endif  

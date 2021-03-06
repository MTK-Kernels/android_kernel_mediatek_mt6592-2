/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP8_COMMON_ENTROPY_H_
#define VP8_COMMON_ENTROPY_H_

#include "treecoder.h"
#include "blockd.h"

#ifdef __cplusplus
extern "C" {
#endif


#define ZERO_TOKEN              0       
#define ONE_TOKEN               1       
#define TWO_TOKEN               2       
#define THREE_TOKEN             3       
#define FOUR_TOKEN              4       
#define DCT_VAL_CATEGORY1       5       
#define DCT_VAL_CATEGORY2       6       
#define DCT_VAL_CATEGORY3       7       
#define DCT_VAL_CATEGORY4       8       
#define DCT_VAL_CATEGORY5       9       
#define DCT_VAL_CATEGORY6       10      
#define DCT_EOB_TOKEN           11      

#define MAX_ENTROPY_TOKENS 12
#define ENTROPY_NODES 11

extern const vp8_tree_index vp8_coef_tree[];

extern const struct vp8_token_struct vp8_coef_encodings[MAX_ENTROPY_TOKENS];

typedef struct
{
    vp8_tree_p tree;
    const vp8_prob *prob;
    int Len;
    int base_val;
} vp8_extra_bit_struct;

extern const vp8_extra_bit_struct vp8_extra_bits[12];    

#define PROB_UPDATE_BASELINE_COST   7

#define MAX_PROB                255
#define DCT_MAX_VALUE           2048




#define BLOCK_TYPES 4


#define COEF_BANDS 8
extern DECLARE_ALIGNED(16, const unsigned char, vp8_coef_bands[16]);


 
#   define PREV_COEF_CONTEXTS       3

extern DECLARE_ALIGNED(16, const unsigned char, vp8_prev_token_class[MAX_ENTROPY_TOKENS]);

extern const vp8_prob vp8_coef_update_probs [BLOCK_TYPES] [COEF_BANDS] [PREV_COEF_CONTEXTS] [ENTROPY_NODES];


struct VP8Common;
void vp8_default_coef_probs(struct VP8Common *);

extern DECLARE_ALIGNED(16, const int, vp8_default_zig_zag1d[16]);
extern DECLARE_ALIGNED(16, const short, vp8_default_inv_zig_zag[16]);
extern DECLARE_ALIGNED(16, const short, vp8_default_zig_zag_mask[16]);
extern const int vp8_mb_feature_data_bits[MB_LVL_MAX];

void vp8_coef_tree_initialize(void);
#ifdef __cplusplus
}  
#endif

#endif  

/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP8_COMMON_TREECODER_H_
#define VP8_COMMON_TREECODER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char vp8bc_index_t; 


typedef unsigned char vp8_prob;

#define vp8_prob_half ( (vp8_prob) 128)

typedef signed char vp8_tree_index;
struct bool_coder_spec;

typedef struct bool_coder_spec bool_coder_spec;
typedef struct bool_writer bool_writer;
typedef struct bool_reader bool_reader;

typedef const bool_coder_spec c_bool_coder_spec;
typedef const bool_writer c_bool_writer;
typedef const bool_reader c_bool_reader;



# define vp8_complement( x) (255 - x)



typedef const vp8_tree_index vp8_tree[], *vp8_tree_p;


typedef const struct vp8_token_struct
{
    int value;
    int Len;
} vp8_token;


void vp8_tokens_from_tree(struct vp8_token_struct *, vp8_tree);
void vp8_tokens_from_tree_offset(struct vp8_token_struct *, vp8_tree,
                                 int offset);



void vp8_tree_probs_from_distribution(
    int n,                      
    vp8_token tok               [  ],
    vp8_tree tree,
    vp8_prob probs          [  ],
    unsigned int branch_ct       [  ] [2],
    const unsigned int num_events[  ],
    unsigned int Pfactor,
    int Round
);


void vp8bc_tree_probs_from_distribution(
    int n,                      
    vp8_token tok               [  ],
    vp8_tree tree,
    vp8_prob probs          [  ],
    unsigned int branch_ct       [  ] [2],
    const unsigned int num_events[  ],
    c_bool_coder_spec *s
);


#ifdef __cplusplus
}  
#endif

#endif  

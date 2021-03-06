/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VP8_DECODER_DBOOLHUFF_H_
#define VP8_DECODER_DBOOLHUFF_H_

#include <stddef.h>
#include <limits.h>

#include "vpx_config.h"
#include "vpx_ports/mem.h"
#include "vpx/vp8dx.h"
#include "vpx/vpx_integer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t VP8_BD_VALUE;

#define VP8_BD_VALUE_SIZE ((int)sizeof(VP8_BD_VALUE)*CHAR_BIT)

#define VP8_LOTS_OF_BITS (0x40000000)

typedef struct
{
    const unsigned char *user_buffer_end;
    const unsigned char *user_buffer;
    VP8_BD_VALUE         value;
    int                  count;
    unsigned int         range;
    vpx_decrypt_cb       decrypt_cb;
    void                *decrypt_state;
} BOOL_DECODER;

DECLARE_ALIGNED(16, extern const unsigned char, vp8_norm[256]);

int vp8dx_start_decode(BOOL_DECODER *br,
                       const unsigned char *source,
                       unsigned int source_sz,
                       vpx_decrypt_cb decrypt_cb,
                       void *decrypt_state);

void vp8dx_bool_decoder_fill(BOOL_DECODER *br);


static int vp8dx_decode_bool(BOOL_DECODER *br, int probability) {
    unsigned int bit = 0;
    VP8_BD_VALUE value;
    unsigned int split;
    VP8_BD_VALUE bigsplit;
    int count;
    unsigned int range;

    split = 1 + (((br->range - 1) * probability) >> 8);

    if(br->count < 0)
        vp8dx_bool_decoder_fill(br);

    value = br->value;
    count = br->count;

    bigsplit = (VP8_BD_VALUE)split << (VP8_BD_VALUE_SIZE - 8);

    range = split;

    if (value >= bigsplit)
    {
        range = br->range - split;
        value = value - bigsplit;
        bit = 1;
    }

    {
        register unsigned int shift = vp8_norm[range];
        range <<= shift;
        value <<= shift;
        count -= shift;
    }
    br->value = value;
    br->count = count;
    br->range = range;

    return bit;
}

static int vp8_decode_value(BOOL_DECODER *br, int bits)
{
    int z = 0;
    int bit;

    for (bit = bits - 1; bit >= 0; bit--)
    {
        z |= (vp8dx_decode_bool(br, 0x80) << bit);
    }

    return z;
}

static int vp8dx_bool_error(BOOL_DECODER *br)
{
    if ((br->count > VP8_BD_VALUE_SIZE) && (br->count < VP8_LOTS_OF_BITS))
    {
        return 1;
    }

    
    return 0;
}

#ifdef __cplusplus
}  
#endif

#endif  

/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdio.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/mips/convolve_common_dspr2.h"
#include "vpx_dsp/vpx_dsp_common.h"
#include "vpx_dsp/vpx_filter.h"
#include "vpx_ports/mem.h"

#if HAVE_DSPR2
static void convolve_horiz_4_transposed_dspr2(const uint8_t *src,
                                              int32_t src_stride,
                                              uint8_t *dst,
                                              int32_t dst_stride,
                                              const int16_t *filter_x0,
                                              int32_t h) {
  int32_t y;
  uint8_t *cm = vpx_ff_cropTbl;
  uint8_t *dst_ptr;
  int32_t vector1b, vector2b, vector3b, vector4b;
  int32_t Temp1, Temp2, Temp3, Temp4;
  uint32_t vector4a = 64;
  uint32_t tp1, tp2;
  uint32_t p1, p2, p3, p4;
  uint32_t tn1, tn2;

  vector1b = ((const int32_t *)filter_x0)[0];
  vector2b = ((const int32_t *)filter_x0)[1];
  vector3b = ((const int32_t *)filter_x0)[2];
  vector4b = ((const int32_t *)filter_x0)[3];

  for (y = h; y--;) {
    dst_ptr = dst;
    
    prefetch_load(src + src_stride);
    prefetch_load(src + src_stride + 32);

    __asm__ __volatile__ (
        "ulw              %[tp1],         0(%[src])                      \n\t"
        "ulw              %[tp2],         4(%[src])                      \n\t"

        
        "mtlo             %[vector4a],    $ac3                           \n\t"
        "mthi             $zero,          $ac3                           \n\t"
        "preceu.ph.qbr    %[p1],          %[tp1]                         \n\t"
        "preceu.ph.qbl    %[p2],          %[tp1]                         \n\t"
        "preceu.ph.qbr    %[p3],          %[tp2]                         \n\t"
        "preceu.ph.qbl    %[p4],          %[tp2]                         \n\t"
        "dpa.w.ph         $ac3,           %[p1],          %[vector1b]    \n\t"
        "dpa.w.ph         $ac3,           %[p2],          %[vector2b]    \n\t"
        "dpa.w.ph         $ac3,           %[p3],          %[vector3b]    \n\t"
        "ulw              %[tn2],         8(%[src])                      \n\t"
        "dpa.w.ph         $ac3,           %[p4],          %[vector4b]    \n\t"
        "extp             %[Temp1],       $ac3,           31             \n\t"

        
        "mtlo             %[vector4a],    $ac2                           \n\t"
        "mthi             $zero,          $ac2                           \n\t"
        "preceu.ph.qbr    %[p1],          %[tn2]                         \n\t"
        "balign           %[tn1],         %[tn2],         3              \n\t"
        "balign           %[tn2],         %[tp2],         3              \n\t"
        "balign           %[tp2],         %[tp1],         3              \n\t"
        "dpa.w.ph         $ac2,           %[p2],          %[vector1b]    \n\t"
        "dpa.w.ph         $ac2,           %[p3],          %[vector2b]    \n\t"
        "dpa.w.ph         $ac2,           %[p4],          %[vector3b]    \n\t"
        "dpa.w.ph         $ac2,           %[p1],          %[vector4b]    \n\t"
        "extp             %[Temp3],       $ac2,           31             \n\t"

        
        "lbux             %[tp1],         %[Temp1](%[cm])                \n\t"
        "mtlo             %[vector4a],    $ac3                           \n\t"
        "mthi             $zero,          $ac3                           \n\t"
        "preceu.ph.qbr    %[p1],          %[tp2]                         \n\t"
        "preceu.ph.qbl    %[p2],          %[tp2]                         \n\t"
        "preceu.ph.qbr    %[p3],          %[tn2]                         \n\t"
        "preceu.ph.qbl    %[p4],          %[tn2]                         \n\t"
        "dpa.w.ph         $ac3,           %[p1],          %[vector1b]    \n\t"
        "dpa.w.ph         $ac3,           %[p2],          %[vector2b]    \n\t"
        "dpa.w.ph         $ac3,           %[p3],          %[vector3b]    \n\t"
        "dpa.w.ph         $ac3,           %[p4],          %[vector4b]    \n\t"
        "extp             %[Temp2],       $ac3,           31             \n\t"

        
        "lbux             %[tp2],         %[Temp3](%[cm])                \n\t"
        "mtlo             %[vector4a],    $ac2                           \n\t"
        "mthi             $zero,          $ac2                           \n\t"
        "preceu.ph.qbr    %[p1],          %[tn1]                         \n\t"
        "dpa.w.ph         $ac2,           %[p2],          %[vector1b]    \n\t"
        "dpa.w.ph         $ac2,           %[p3],          %[vector2b]    \n\t"
        "dpa.w.ph         $ac2,           %[p4],          %[vector3b]    \n\t"
        "dpa.w.ph         $ac2,           %[p1],          %[vector4b]    \n\t"
        "extp             %[Temp4],       $ac2,           31             \n\t"

        
        "lbux             %[tn1],         %[Temp2](%[cm])                \n\t"
        "lbux             %[p2],          %[Temp4](%[cm])                \n\t"

        
        "sb               %[tp1],         0(%[dst_ptr])                  \n\t"
        "addu             %[dst_ptr],     %[dst_ptr],     %[dst_stride]  \n\t"

        "sb               %[tn1],         0(%[dst_ptr])                  \n\t"
        "addu             %[dst_ptr],     %[dst_ptr],     %[dst_stride]  \n\t"

        "sb               %[tp2],         0(%[dst_ptr])                  \n\t"
        "addu             %[dst_ptr],     %[dst_ptr],     %[dst_stride]  \n\t"

        "sb               %[p2],          0(%[dst_ptr])                  \n\t"
        "addu             %[dst_ptr],     %[dst_ptr],     %[dst_stride]  \n\t"

        : [tp1] "=&r" (tp1), [tp2] "=&r" (tp2), [tn1] "=&r" (tn1), [tn2] "=&r" (tn2),
          [p1] "=&r" (p1), [p2] "=&r" (p2), [p3] "=&r" (p3), [p4] "=&r" (p4),
          [Temp1] "=&r" (Temp1), [Temp2] "=&r" (Temp2), [Temp3] "=&r" (Temp3), [Temp4] "=&r" (Temp4),
          [dst_ptr] "+r" (dst_ptr)
        : [vector1b] "r" (vector1b), [vector2b] "r" (vector2b),
          [vector3b] "r" (vector3b), [vector4b] "r" (vector4b),
          [vector4a] "r" (vector4a),
          [cm] "r" (cm), [src] "r" (src), [dst_stride] "r" (dst_stride)
    );

    
    src += src_stride;
    dst += 1;
  }
}

static void convolve_horiz_8_transposed_dspr2(const uint8_t *src,
                                              int32_t src_stride,
                                              uint8_t *dst,
                                              int32_t dst_stride,
                                              const int16_t *filter_x0,
                                              int32_t h) {
  int32_t y;
  uint8_t *cm = vpx_ff_cropTbl;
  uint8_t *dst_ptr;
  uint32_t vector4a = 64;
  int32_t vector1b, vector2b, vector3b, vector4b;
  int32_t Temp1, Temp2, Temp3;
  uint32_t tp1, tp2, tp3;
  uint32_t p1, p2, p3, p4, n1;
  uint8_t *odd_dst;
  uint32_t dst_pitch_2 = (dst_stride << 1);

  vector1b = ((const int32_t *)filter_x0)[0];
  vector2b = ((const int32_t *)filter_x0)[1];
  vector3b = ((const int32_t *)filter_x0)[2];
  vector4b = ((const int32_t *)filter_x0)[3];

  for (y = h; y--;) {
    
    prefetch_load(src + src_stride);
    prefetch_load(src + src_stride + 32);

    dst_ptr = dst;
    odd_dst = (dst_ptr + dst_stride);

    __asm__ __volatile__ (
        "ulw              %[tp2],         0(%[src])                       \n\t"
        "ulw              %[tp1],         4(%[src])                       \n\t"

        
        "mtlo             %[vector4a],    $ac3                            \n\t"
        "mthi             $zero,          $ac3                            \n\t"
        "mtlo             %[vector4a],    $ac2                            \n\t"
        "mthi             $zero,          $ac2                            \n\t"
        "preceu.ph.qbr    %[p1],          %[tp2]                          \n\t"
        "preceu.ph.qbl    %[p2],          %[tp2]                          \n\t"
        "preceu.ph.qbr    %[p3],          %[tp1]                          \n\t"
        "preceu.ph.qbl    %[p4],          %[tp1]                          \n\t"
        "ulw              %[tp3],         8(%[src])                       \n\t"
        "dpa.w.ph         $ac3,           %[p1],          %[vector1b]     \n\t"
        "dpa.w.ph         $ac3,           %[p2],          %[vector2b]     \n\t"
        "dpa.w.ph         $ac3,           %[p3],          %[vector3b]     \n\t"
        "dpa.w.ph         $ac3,           %[p4],          %[vector4b]     \n\t"
        "extp             %[Temp1],       $ac3,           31              \n\t"

        
        "preceu.ph.qbr    %[p1],          %[tp3]                          \n\t"
        "preceu.ph.qbl    %[n1],          %[tp3]                          \n\t"
        "ulw              %[tp2],         12(%[src])                      \n\t"
        "dpa.w.ph         $ac2,           %[p2],          %[vector1b]     \n\t"
        "dpa.w.ph         $ac2,           %[p3],          %[vector2b]     \n\t"
        "dpa.w.ph         $ac2,           %[p4],          %[vector3b]     \n\t"
        "dpa.w.ph         $ac2,           %[p1],          %[vector4b]     \n\t"
        "extp             %[Temp3],       $ac2,           31              \n\t"

        
        "lbux             %[Temp2],       %[Temp1](%[cm])                 \n\t"
        "mtlo             %[vector4a],    $ac1                            \n\t"
        "mthi             $zero,          $ac1                            \n\t"
        "preceu.ph.qbr    %[p2],          %[tp2]                          \n\t"
        "dpa.w.ph         $ac1,           %[p3],          %[vector1b]     \n\t"
        "dpa.w.ph         $ac1,           %[p4],          %[vector2b]     \n\t"
        "dpa.w.ph         $ac1,           %[p1],          %[vector3b]     \n\t"
        "lbux             %[tp3],         %[Temp3](%[cm])                 \n\t"
        "dpa.w.ph         $ac1,           %[n1],          %[vector4b]     \n\t"
        "extp             %[p3],          $ac1,           31              \n\t"

        
        "mtlo             %[vector4a],    $ac2                            \n\t"
        "mthi             $zero,          $ac2                            \n\t"
        "mtlo             %[vector4a],    $ac3                            \n\t"
        "mthi             $zero,          $ac3                            \n\t"
        "sb               %[Temp2],       0(%[dst_ptr])                   \n\t"
        "addu             %[dst_ptr],     %[dst_ptr],     %[dst_pitch_2]  \n\t"
        "sb               %[tp3],         0(%[dst_ptr])                   \n\t"
        "addu             %[dst_ptr],     %[dst_ptr],     %[dst_pitch_2]  \n\t"

        "ulw              %[tp1],         1(%[src])                       \n\t"
        "ulw              %[tp3],         5(%[src])                       \n\t"

        "dpa.w.ph         $ac2,           %[p4],          %[vector1b]     \n\t"
        "dpa.w.ph         $ac2,           %[p1],          %[vector2b]     \n\t"
        "dpa.w.ph         $ac2,           %[n1],          %[vector3b]     \n\t"
        "dpa.w.ph         $ac2,           %[p2],          %[vector4b]     \n\t"
        "extp             %[Temp3],       $ac2,           31              \n\t"

        "lbux             %[tp2],         %[p3](%[cm])                    \n\t"

        
        "mtlo             %[vector4a],    $ac1                            \n\t"
        "mthi             $zero,          $ac1                            \n\t"
        "preceu.ph.qbr    %[p1],          %[tp1]                          \n\t"
        "preceu.ph.qbl    %[p2],          %[tp1]                          \n\t"
        "preceu.ph.qbr    %[p3],          %[tp3]                          \n\t"
        "preceu.ph.qbl    %[p4],          %[tp3]                          \n\t"
        "sb               %[tp2],         0(%[dst_ptr])                   \n\t"
        "addu             %[dst_ptr],     %[dst_ptr],     %[dst_pitch_2]  \n\t"
        "ulw              %[tp2],         9(%[src])                       \n\t"

        "dpa.w.ph         $ac3,           %[p1],          %[vector1b]     \n\t"
        "dpa.w.ph         $ac3,           %[p2],          %[vector2b]     \n\t"
        "dpa.w.ph         $ac3,           %[p3],          %[vector3b]     \n\t"
        "dpa.w.ph         $ac3,           %[p4],          %[vector4b]     \n\t"
        "extp             %[Temp2],       $ac3,           31              \n\t"

        
        "lbux             %[tp1],         %[Temp3](%[cm])                 \n\t"
        "mtlo             %[vector4a],    $ac3                            \n\t"
        "mthi             $zero,          $ac3                            \n\t"
        "mtlo             %[vector4a],    $ac2                            \n\t"
        "mthi             $zero,          $ac2                            \n\t"
        "preceu.ph.qbr    %[p1],          %[tp2]                          \n\t"
        "preceu.ph.qbl    %[n1],          %[tp2]                          \n\t"
        "ulw              %[Temp1],       13(%[src])                      \n\t"
        "dpa.w.ph         $ac1,           %[p2],          %[vector1b]     \n\t"
        "sb               %[tp1],         0(%[dst_ptr])                   \n\t"
        "addu             %[dst_ptr],     %[dst_ptr],     %[dst_pitch_2]  \n\t"
        "dpa.w.ph         $ac1,           %[p3],          %[vector2b]     \n\t"
        "dpa.w.ph         $ac1,           %[p4],          %[vector3b]     \n\t"
        "dpa.w.ph         $ac1,           %[p1],          %[vector4b]     \n\t"
        "extp             %[Temp3],       $ac1,           31              \n\t"

        
        "lbux             %[tp3],         %[Temp2](%[cm])                 \n\t"
        "preceu.ph.qbr    %[p2],          %[Temp1]                        \n\t"
        "dpa.w.ph         $ac3,           %[p3],          %[vector1b]     \n\t"
        "dpa.w.ph         $ac3,           %[p4],          %[vector2b]     \n\t"
        "dpa.w.ph         $ac3,           %[p1],          %[vector3b]     \n\t"
        "dpa.w.ph         $ac3,           %[n1],          %[vector4b]     \n\t"
        "extp             %[Temp2],       $ac3,           31              \n\t"

        
        "sb               %[tp3],         0(%[odd_dst])                   \n\t"
        "addu             %[odd_dst],     %[odd_dst],     %[dst_pitch_2]  \n\t"
        "dpa.w.ph         $ac2,           %[p4],          %[vector1b]     \n\t"
        "dpa.w.ph         $ac2,           %[p1],          %[vector2b]     \n\t"
        "dpa.w.ph         $ac2,           %[n1],          %[vector3b]     \n\t"
        "dpa.w.ph         $ac2,           %[p2],          %[vector4b]     \n\t"
        "extp             %[Temp1],       $ac2,           31              \n\t"

        
        "lbux             %[p4],          %[Temp3](%[cm])                 \n\t"
        "lbux             %[p2],          %[Temp2](%[cm])                 \n\t"
        "lbux             %[n1],          %[Temp1](%[cm])                 \n\t"

        
        "sb               %[p4],          0(%[odd_dst])                   \n\t"
        "addu             %[odd_dst],     %[odd_dst],     %[dst_pitch_2]  \n\t"

        "sb               %[p2],          0(%[odd_dst])                   \n\t"
        "addu             %[odd_dst],     %[odd_dst],     %[dst_pitch_2]  \n\t"

        "sb               %[n1],          0(%[odd_dst])                   \n\t"

        : [tp1] "=&r" (tp1), [tp2] "=&r" (tp2), [tp3] "=&r" (tp3),
          [p1] "=&r" (p1), [p2] "=&r" (p2), [p3] "=&r" (p3), [p4] "=&r" (p4),
          [n1] "=&r" (n1),
          [Temp1] "=&r" (Temp1), [Temp2] "=&r" (Temp2), [Temp3] "=&r" (Temp3),
          [dst_ptr] "+r" (dst_ptr), [odd_dst] "+r" (odd_dst)
        : [vector1b] "r" (vector1b), [vector2b] "r" (vector2b),
          [vector3b] "r" (vector3b), [vector4b] "r" (vector4b),
          [vector4a] "r" (vector4a), [cm] "r" (cm),
          [src] "r" (src), [dst_pitch_2] "r" (dst_pitch_2)
    );

    
    src += src_stride;
    dst += 1;
  }
}

static void convolve_horiz_16_transposed_dspr2(const uint8_t *src_ptr,
                                               int32_t src_stride,
                                               uint8_t *dst_ptr,
                                               int32_t dst_stride,
                                               const int16_t *filter_x0,
                                               int32_t h,
                                               int32_t count) {
  int32_t c, y;
  const uint8_t *src;
  uint8_t *dst;
  uint8_t *cm = vpx_ff_cropTbl;
  uint32_t vector_64 = 64;
  int32_t  filter12, filter34, filter56, filter78;
  int32_t  Temp1, Temp2, Temp3;
  uint32_t qload1, qload2;
  uint32_t p1, p2, p3, p4, p5;
  uint32_t st1, st2, st3;
  uint32_t dst_pitch_2 = (dst_stride << 1);
  uint8_t  *odd_dst;

  filter12 = ((const int32_t *)filter_x0)[0];
  filter34 = ((const int32_t *)filter_x0)[1];
  filter56 = ((const int32_t *)filter_x0)[2];
  filter78 = ((const int32_t *)filter_x0)[3];

  for (y = h; y--;) {
    
    prefetch_load(src_ptr + src_stride);
    prefetch_load(src_ptr + src_stride + 32);

    src = src_ptr;
    dst = dst_ptr;

    odd_dst = (dst + dst_stride);

    for (c = 0; c < count; c++) {
      __asm__ __volatile__ (
          "ulw              %[qload1],        0(%[src])                       \n\t"
          "ulw              %[qload2],        4(%[src])                       \n\t"

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "mtlo             %[vector_64],     $ac2                            \n\t" 
          "mthi             $zero,            $ac2                            \n\t"
          "preceu.ph.qbr    %[p3],            %[qload2]                       \n\t"
          "preceu.ph.qbl    %[p4],            %[qload2]                       \n\t"
          "preceu.ph.qbr    %[p1],            %[qload1]                       \n\t"
          "preceu.ph.qbl    %[p2],            %[qload1]                       \n\t"
          "ulw              %[qload2],        8(%[src])                       \n\t"
          "dpa.w.ph         $ac1,             %[p1],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p2],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p3],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p4],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 

          
          "mtlo             %[vector_64],     $ac3                            \n\t" 
          "mthi             $zero,            $ac3                            \n\t"
          "preceu.ph.qbr    %[p1],            %[qload2]                       \n\t"
          "preceu.ph.qbl    %[p5],            %[qload2]                       \n\t"
          "ulw              %[qload1],        12(%[src])                      \n\t"
          "dpa.w.ph         $ac2,             %[p2],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac2,             %[p3],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac2,             %[p4],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac2,             %[p1],          %[filter78]     \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 
          "extp             %[Temp2],         $ac2,           31              \n\t" 

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "preceu.ph.qbr    %[p2],            %[qload1]                       \n\t"
          "sb               %[st1],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]             \n\t"
          "dpa.w.ph         $ac3,             %[p3],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac3,             %[p4],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac3,             %[p1],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac3,             %[p5],          %[filter78]     \n\t" 
          "extp             %[Temp3],         $ac3,           31              \n\t" 
          "lbux             %[st2],           %[Temp2](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac2                            \n\t" 
          "mthi             $zero,            $ac2                            \n\t"
          "preceu.ph.qbl    %[p3],            %[qload1]                       \n\t"
          "sb               %[st2],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "ulw              %[qload2],        16(%[src])                      \n\t"
          "dpa.w.ph         $ac1,             %[p4],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p1],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p5],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p2],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 
          "lbux             %[st3],           %[Temp3](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac3                            \n\t" 
          "mthi             $zero,            $ac3                            \n\t"
          "preceu.ph.qbr    %[p4],            %[qload2]                       \n\t"
          "sb               %[st3],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac2,             %[p1],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac2,             %[p5],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac2,             %[p2],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac2,             %[p3],          %[filter78]     \n\t" 
          "extp             %[Temp2],         $ac2,           31              \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "preceu.ph.qbl    %[p1],            %[qload2]                       \n\t"
          "sb               %[st1],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "ulw              %[qload1],        20(%[src])                      \n\t"
          "dpa.w.ph         $ac3,             %[p5],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac3,             %[p2],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac3,             %[p3],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac3,             %[p4],          %[filter78]     \n\t" 
          "extp             %[Temp3],         $ac3,           31              \n\t" 
          "lbux             %[st2],           %[Temp2](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac2                            \n\t" 
          "mthi             $zero,            $ac2                            \n\t"
          "preceu.ph.qbr    %[p5],            %[qload1]                       \n\t"
          "sb               %[st2],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac1,             %[p2],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p3],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p4],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p1],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 
          "lbux             %[st3],           %[Temp3](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac3                            \n\t" 
          "mthi             $zero,            $ac3                            \n\t"
          "dpa.w.ph         $ac2,             %[p3],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac2,             %[p4],          %[filter34]     \n\t" 
          "sb               %[st3],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac2,             %[p1],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac2,             %[p5],          %[filter78]     \n\t" 
          "extp             %[Temp2],         $ac2,           31              \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 

          
          "ulw              %[qload1],        1(%[src])                       \n\t"
          "ulw              %[qload2],        5(%[src])                       \n\t"

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "preceu.ph.qbr    %[p1],            %[qload1]                       \n\t"
          "preceu.ph.qbl    %[p2],            %[qload1]                       \n\t"
          "preceu.ph.qbr    %[p3],            %[qload2]                       \n\t"
          "preceu.ph.qbl    %[p4],            %[qload2]                       \n\t"
          "sb               %[st1],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "ulw              %[qload2],        9(%[src])                       \n\t"
          "dpa.w.ph         $ac3,             %[p1],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac3,             %[p2],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac3,             %[p3],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac3,             %[p4],          %[filter78]     \n\t" 
          "extp             %[Temp3],         $ac3,           31              \n\t" 
          "lbux             %[st2],           %[Temp2](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac2                            \n\t" 
          "mthi             $zero,            $ac2                            \n\t"
          "preceu.ph.qbr    %[p1],            %[qload2]                       \n\t"
          "preceu.ph.qbl    %[p5],            %[qload2]                       \n\t"
          "sb               %[st2],           0(%[dst])                       \n\t" 
          "ulw              %[qload1],        13(%[src])                      \n\t"
          "dpa.w.ph         $ac1,             %[p2],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p3],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p4],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p1],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 
          "lbux             %[st3],           %[Temp3](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac3                            \n\t" 
          "mthi             $zero,            $ac3                            \n\t"
          "preceu.ph.qbr    %[p2],            %[qload1]                       \n\t"
          "sb               %[st3],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac2,             %[p3],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac2,             %[p4],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac2,             %[p1],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac2,             %[p5],          %[filter78]     \n\t" 
          "extp             %[Temp2],         $ac2,           31              \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "preceu.ph.qbl    %[p3],            %[qload1]                       \n\t"
          "sb               %[st1],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"
          "ulw              %[qload2],        17(%[src])                      \n\t"
          "dpa.w.ph         $ac3,             %[p4],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac3,             %[p1],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac3,             %[p5],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac3,             %[p2],          %[filter78]     \n\t" 
          "extp             %[Temp3],         $ac3,           31              \n\t" 
          "lbux             %[st2],           %[Temp2](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac2                            \n\t" 
          "mthi             $zero,            $ac2                            \n\t"
          "preceu.ph.qbr    %[p4],            %[qload2]                       \n\t"
          "sb               %[st2],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac1,             %[p1],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p5],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p2],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p3],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 
          "lbux             %[st3],           %[Temp3](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac3                            \n\t" 
          "mthi             $zero,            $ac3                            \n\t"
          "preceu.ph.qbl    %[p1],            %[qload2]                       \n\t"
          "sb               %[st3],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"
          "ulw              %[qload1],        21(%[src])                      \n\t"
          "dpa.w.ph         $ac2,             %[p5],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac2,             %[p2],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac2,             %[p3],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac2,             %[p4],          %[filter78]     \n\t" 
          "extp             %[Temp2],         $ac2,           31              \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "preceu.ph.qbr    %[p5],            %[qload1]                       \n\t"
          "sb               %[st1],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac3,             %[p2],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac3,             %[p3],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac3,             %[p4],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac3,             %[p1],          %[filter78]     \n\t" 
          "extp             %[Temp3],         $ac3,           31              \n\t" 

          
          "dpa.w.ph         $ac1,             %[p3],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p4],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p1],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p5],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 

          "lbux             %[st2],           %[Temp2](%[cm])                 \n\t" 
          "lbux             %[st3],           %[Temp3](%[cm])                 \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 

          "sb               %[st2],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"

          "sb               %[st3],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"

          "sb               %[st1],           0(%[odd_dst])                   \n\t" 

          : [qload1] "=&r" (qload1), [qload2] "=&r" (qload2), [p5] "=&r" (p5),
            [st1] "=&r" (st1), [st2] "=&r" (st2), [st3] "=&r" (st3),
            [p1] "=&r" (p1), [p2] "=&r" (p2), [p3] "=&r" (p3), [p4] "=&r" (p4),
            [Temp1] "=&r" (Temp1), [Temp2] "=&r" (Temp2), [Temp3] "=&r" (Temp3),
            [dst] "+r" (dst), [odd_dst] "+r" (odd_dst)
          : [filter12] "r" (filter12), [filter34] "r" (filter34),
            [filter56] "r" (filter56), [filter78] "r" (filter78),
            [vector_64] "r" (vector_64), [cm] "r" (cm),
            [src] "r" (src), [dst_pitch_2] "r" (dst_pitch_2)
      );

      src += 16;
      dst = (dst_ptr + ((c + 1) * 16 * dst_stride));
      odd_dst = (dst + dst_stride);
    }

    
    src_ptr += src_stride;

    dst_ptr += 1;
  }
}

static void convolve_horiz_64_transposed_dspr2(const uint8_t *src_ptr,
                                               int32_t src_stride,
                                               uint8_t *dst_ptr,
                                               int32_t dst_stride,
                                               const int16_t *filter_x0,
                                               int32_t h) {
  int32_t c, y;
  const uint8_t *src;
  uint8_t *dst;
  uint8_t *cm = vpx_ff_cropTbl;
  uint32_t vector_64 = 64;
  int32_t  filter12, filter34, filter56, filter78;
  int32_t  Temp1, Temp2, Temp3;
  uint32_t qload1, qload2;
  uint32_t p1, p2, p3, p4, p5;
  uint32_t st1, st2, st3;
  uint32_t dst_pitch_2 = (dst_stride << 1);
  uint8_t  *odd_dst;

  filter12 = ((const int32_t *)filter_x0)[0];
  filter34 = ((const int32_t *)filter_x0)[1];
  filter56 = ((const int32_t *)filter_x0)[2];
  filter78 = ((const int32_t *)filter_x0)[3];

  for (y = h; y--;) {
    
    prefetch_load(src_ptr + src_stride);
    prefetch_load(src_ptr + src_stride + 32);
    prefetch_load(src_ptr + src_stride + 64);

    src = src_ptr;
    dst = dst_ptr;

    odd_dst = (dst + dst_stride);

    for (c = 0; c < 4; c++) {
      __asm__ __volatile__ (
          "ulw              %[qload1],        0(%[src])                       \n\t"
          "ulw              %[qload2],        4(%[src])                       \n\t"

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "mtlo             %[vector_64],     $ac2                            \n\t" 
          "mthi             $zero,            $ac2                            \n\t"
          "preceu.ph.qbr    %[p3],            %[qload2]                       \n\t"
          "preceu.ph.qbl    %[p4],            %[qload2]                       \n\t"
          "preceu.ph.qbr    %[p1],            %[qload1]                       \n\t"
          "preceu.ph.qbl    %[p2],            %[qload1]                       \n\t"
          "ulw              %[qload2],        8(%[src])                       \n\t"
          "dpa.w.ph         $ac1,             %[p1],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p2],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p3],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p4],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 

          
          "mtlo             %[vector_64],     $ac3                            \n\t" 
          "mthi             $zero,            $ac3                            \n\t"
          "preceu.ph.qbr    %[p1],            %[qload2]                       \n\t"
          "preceu.ph.qbl    %[p5],            %[qload2]                       \n\t"
          "ulw              %[qload1],        12(%[src])                      \n\t"
          "dpa.w.ph         $ac2,             %[p2],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac2,             %[p3],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac2,             %[p4],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac2,             %[p1],          %[filter78]     \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 
          "extp             %[Temp2],         $ac2,           31              \n\t" 

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "preceu.ph.qbr    %[p2],            %[qload1]                       \n\t"
          "sb               %[st1],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]             \n\t"
          "dpa.w.ph         $ac3,             %[p3],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac3,             %[p4],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac3,             %[p1],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac3,             %[p5],          %[filter78]     \n\t" 
          "extp             %[Temp3],         $ac3,           31              \n\t" 
          "lbux             %[st2],           %[Temp2](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac2                            \n\t" 
          "mthi             $zero,            $ac2                            \n\t"
          "preceu.ph.qbl    %[p3],            %[qload1]                       \n\t"
          "sb               %[st2],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "ulw              %[qload2],        16(%[src])                      \n\t"
          "dpa.w.ph         $ac1,             %[p4],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p1],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p5],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p2],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 
          "lbux             %[st3],           %[Temp3](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac3                            \n\t" 
          "mthi             $zero,            $ac3                            \n\t"
          "preceu.ph.qbr    %[p4],            %[qload2]                       \n\t"
          "sb               %[st3],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac2,             %[p1],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac2,             %[p5],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac2,             %[p2],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac2,             %[p3],          %[filter78]     \n\t" 
          "extp             %[Temp2],         $ac2,           31              \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "preceu.ph.qbl    %[p1],            %[qload2]                       \n\t"
          "sb               %[st1],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "ulw              %[qload1],        20(%[src])                      \n\t"
          "dpa.w.ph         $ac3,             %[p5],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac3,             %[p2],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac3,             %[p3],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac3,             %[p4],          %[filter78]     \n\t" 
          "extp             %[Temp3],         $ac3,           31              \n\t" 
          "lbux             %[st2],           %[Temp2](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac2                            \n\t" 
          "mthi             $zero,            $ac2                            \n\t"
          "preceu.ph.qbr    %[p5],            %[qload1]                       \n\t"
          "sb               %[st2],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac1,             %[p2],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p3],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p4],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p1],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 
          "lbux             %[st3],           %[Temp3](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac3                            \n\t" 
          "mthi             $zero,            $ac3                            \n\t"
          "dpa.w.ph         $ac2,             %[p3],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac2,             %[p4],          %[filter34]     \n\t" 
          "sb               %[st3],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac2,             %[p1],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac2,             %[p5],          %[filter78]     \n\t" 
          "extp             %[Temp2],         $ac2,           31              \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 

          
          "ulw              %[qload1],        1(%[src])                       \n\t"
          "ulw              %[qload2],        5(%[src])                       \n\t"

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "preceu.ph.qbr    %[p1],            %[qload1]                       \n\t"
          "preceu.ph.qbl    %[p2],            %[qload1]                       \n\t"
          "preceu.ph.qbr    %[p3],            %[qload2]                       \n\t"
          "preceu.ph.qbl    %[p4],            %[qload2]                       \n\t"
          "sb               %[st1],           0(%[dst])                       \n\t" 
          "addu             %[dst],           %[dst],         %[dst_pitch_2]  \n\t"
          "ulw              %[qload2],        9(%[src])                       \n\t"
          "dpa.w.ph         $ac3,             %[p1],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac3,             %[p2],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac3,             %[p3],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac3,             %[p4],          %[filter78]     \n\t" 
          "extp             %[Temp3],         $ac3,           31              \n\t" 
          "lbux             %[st2],           %[Temp2](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac2                            \n\t" 
          "mthi             $zero,            $ac2                            \n\t"
          "preceu.ph.qbr    %[p1],            %[qload2]                       \n\t"
          "preceu.ph.qbl    %[p5],            %[qload2]                       \n\t"
          "sb               %[st2],           0(%[dst])                       \n\t" 
          "ulw              %[qload1],        13(%[src])                      \n\t"
          "dpa.w.ph         $ac1,             %[p2],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p3],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p4],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p1],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 
          "lbux             %[st3],           %[Temp3](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac3                            \n\t" 
          "mthi             $zero,            $ac3                            \n\t"
          "preceu.ph.qbr    %[p2],            %[qload1]                       \n\t"
          "sb               %[st3],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac2,             %[p3],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac2,             %[p4],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac2,             %[p1],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac2,             %[p5],          %[filter78]     \n\t" 
          "extp             %[Temp2],         $ac2,           31              \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "preceu.ph.qbl    %[p3],            %[qload1]                       \n\t"
          "sb               %[st1],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"
          "ulw              %[qload2],        17(%[src])                      \n\t"
          "dpa.w.ph         $ac3,             %[p4],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac3,             %[p1],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac3,             %[p5],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac3,             %[p2],          %[filter78]     \n\t" 
          "extp             %[Temp3],         $ac3,           31              \n\t" 
          "lbux             %[st2],           %[Temp2](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac2                            \n\t" 
          "mthi             $zero,            $ac2                            \n\t"
          "preceu.ph.qbr    %[p4],            %[qload2]                       \n\t"
          "sb               %[st2],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac1,             %[p1],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p5],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p2],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p3],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 
          "lbux             %[st3],           %[Temp3](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac3                            \n\t" 
          "mthi             $zero,            $ac3                            \n\t"
          "preceu.ph.qbl    %[p1],            %[qload2]                       \n\t"
          "sb               %[st3],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"
          "ulw              %[qload1],        21(%[src])                      \n\t"
          "dpa.w.ph         $ac2,             %[p5],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac2,             %[p2],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac2,             %[p3],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac2,             %[p4],          %[filter78]     \n\t" 
          "extp             %[Temp2],         $ac2,           31              \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 

          
          "mtlo             %[vector_64],     $ac1                            \n\t" 
          "mthi             $zero,            $ac1                            \n\t"
          "preceu.ph.qbr    %[p5],            %[qload1]                       \n\t"
          "sb               %[st1],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"
          "dpa.w.ph         $ac3,             %[p2],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac3,             %[p3],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac3,             %[p4],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac3,             %[p1],          %[filter78]     \n\t" 
          "extp             %[Temp3],         $ac3,           31              \n\t" 

          
          "dpa.w.ph         $ac1,             %[p3],          %[filter12]     \n\t" 
          "dpa.w.ph         $ac1,             %[p4],          %[filter34]     \n\t" 
          "dpa.w.ph         $ac1,             %[p1],          %[filter56]     \n\t" 
          "dpa.w.ph         $ac1,             %[p5],          %[filter78]     \n\t" 
          "extp             %[Temp1],         $ac1,           31              \n\t" 

          "lbux             %[st2],           %[Temp2](%[cm])                 \n\t" 
          "lbux             %[st3],           %[Temp3](%[cm])                 \n\t" 
          "lbux             %[st1],           %[Temp1](%[cm])                 \n\t" 

          "sb               %[st2],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"

          "sb               %[st3],           0(%[odd_dst])                   \n\t" 
          "addu             %[odd_dst],       %[odd_dst],     %[dst_pitch_2]  \n\t"

          "sb               %[st1],           0(%[odd_dst])                   \n\t" 

          : [qload1] "=&r" (qload1), [qload2] "=&r" (qload2), [p5] "=&r" (p5),
            [st1] "=&r" (st1), [st2] "=&r" (st2), [st3] "=&r" (st3),
            [p1] "=&r" (p1), [p2] "=&r" (p2), [p3] "=&r" (p3), [p4] "=&r" (p4),
            [Temp1] "=&r" (Temp1), [Temp2] "=&r" (Temp2), [Temp3] "=&r" (Temp3),
            [dst] "+r" (dst), [odd_dst] "+r" (odd_dst)
          : [filter12] "r" (filter12), [filter34] "r" (filter34),
            [filter56] "r" (filter56), [filter78] "r" (filter78),
            [vector_64] "r" (vector_64), [cm] "r" (cm),
            [src] "r" (src), [dst_pitch_2] "r" (dst_pitch_2)
      );

      src += 16;
      dst = (dst_ptr + ((c + 1) * 16 * dst_stride));
      odd_dst = (dst + dst_stride);
    }

    
    src_ptr += src_stride;

    dst_ptr += 1;
  }
}

void convolve_horiz_transposed(const uint8_t *src, ptrdiff_t src_stride,
                               uint8_t *dst, ptrdiff_t dst_stride,
                               const int16_t *filter, int w, int h) {
  int x, y, k;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      int sum = 0;

      for (k = 0; k < 8; ++k)
        sum += src[x + k] * filter[k];

      dst[x * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
    }

    src += src_stride;
    dst += 1;
  }
}

void copy_horiz_transposed(const uint8_t *src, ptrdiff_t src_stride,
                           uint8_t *dst, ptrdiff_t dst_stride,
                           int w, int h) {
  int x, y;

  for (y = 0; y < h; ++y) {
    for (x = 0; x < w; ++x) {
      dst[x * dst_stride] = src[x];
    }

    src += src_stride;
    dst += 1;
  }
}

void vpx_convolve8_dspr2(const uint8_t *src, ptrdiff_t src_stride,
                         uint8_t *dst, ptrdiff_t dst_stride,
                         const int16_t *filter_x, int x_step_q4,
                         const int16_t *filter_y, int y_step_q4,
                         int w, int h) {
  DECLARE_ALIGNED(32, uint8_t, temp[64 * 135]);
  int32_t intermediate_height = ((h * y_step_q4) >> 4) + 7;
  uint32_t pos = 38;

  assert(x_step_q4 == 16);
  assert(y_step_q4 == 16);
  assert(((const int32_t *)filter_x)[1] != 0x800000);
  assert(((const int32_t *)filter_y)[1] != 0x800000);


  
  __asm__ __volatile__ (
    "wrdsp      %[pos],     1           \n\t"
    :
    : [pos] "r" (pos)
  );

  if (intermediate_height < h)
    intermediate_height = h;

  
  if (filter_x[3] == 0x80) {
    copy_horiz_transposed(src - src_stride * 3, src_stride,
                          temp, intermediate_height,
                          w, intermediate_height);
  } else if (((const int32_t *)filter_x)[0] == 0) {
    vpx_convolve2_dspr2(src - src_stride * 3, src_stride,
                        temp, intermediate_height,
                        filter_x,
                        w, intermediate_height);
  } else {
    src -= (src_stride * 3 + 3);

    
    prefetch_load(src);
    prefetch_load(src + 32);

    switch (w) {
      case 4:
        convolve_horiz_4_transposed_dspr2(src, src_stride,
                                          temp, intermediate_height,
                                          filter_x, intermediate_height);
        break;
      case 8:
        convolve_horiz_8_transposed_dspr2(src, src_stride,
                                          temp, intermediate_height,
                                          filter_x, intermediate_height);
        break;
      case 16:
      case 32:
        convolve_horiz_16_transposed_dspr2(src, src_stride,
                                           temp, intermediate_height,
                                           filter_x, intermediate_height,
                                           (w/16));
        break;
      case 64:
        prefetch_load(src + 32);
        convolve_horiz_64_transposed_dspr2(src, src_stride,
                                           temp, intermediate_height,
                                           filter_x, intermediate_height);
        break;
      default:
        convolve_horiz_transposed(src, src_stride,
                                  temp, intermediate_height,
                                  filter_x, w, intermediate_height);
        break;
    }
  }

  
  if (filter_y[3] == 0x80) {
    copy_horiz_transposed(temp + 3, intermediate_height,
                          dst, dst_stride,
                          h, w);
  } else if (((const int32_t *)filter_y)[0] == 0) {
    vpx_convolve2_dspr2(temp + 3, intermediate_height,
                        dst, dst_stride,
                        filter_y,
                        h, w);
  } else {
    switch (h) {
      case 4:
        convolve_horiz_4_transposed_dspr2(temp, intermediate_height,
                                          dst, dst_stride,
                                          filter_y, w);
        break;
      case 8:
        convolve_horiz_8_transposed_dspr2(temp, intermediate_height,
                                          dst, dst_stride,
                                          filter_y, w);
        break;
      case 16:
      case 32:
        convolve_horiz_16_transposed_dspr2(temp, intermediate_height,
                                           dst, dst_stride,
                                           filter_y, w, (h/16));
        break;
      case 64:
        convolve_horiz_64_transposed_dspr2(temp, intermediate_height,
                                           dst, dst_stride,
                                           filter_y, w);
        break;
      default:
        convolve_horiz_transposed(temp, intermediate_height,
                                  dst, dst_stride,
                                  filter_y, h, w);
        break;
    }
  }
}

void vpx_convolve_copy_dspr2(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const int16_t *filter_x, int filter_x_stride,
                             const int16_t *filter_y, int filter_y_stride,
                             int w, int h) {
  int x, y;

  
  prefetch_load(src);
  prefetch_load(src + 32);
  prefetch_store(dst);

  switch (w) {
    case 4:
      {
      uint32_t tp1;

      
      for (y = h; y--; ) {
        prefetch_load(src + src_stride);
        prefetch_load(src + src_stride + 32);
        prefetch_store(dst + dst_stride);

        __asm__ __volatile__ (
            "ulw              %[tp1],         (%[src])      \n\t"
            "sw               %[tp1],         (%[dst])      \n\t"  

            : [tp1] "=&r" (tp1)
            : [src] "r" (src), [dst] "r" (dst)
        );

        src += src_stride;
        dst += dst_stride;
      }
      }
      break;
    case 8:
      {
      uint32_t tp1, tp2;

      
      for (y = h; y--; ) {
        prefetch_load(src + src_stride);
        prefetch_load(src + src_stride + 32);
        prefetch_store(dst + dst_stride);

        __asm__ __volatile__ (
            "ulw              %[tp1],         0(%[src])      \n\t"
            "ulw              %[tp2],         4(%[src])      \n\t"
            "sw               %[tp1],         0(%[dst])      \n\t"  
            "sw               %[tp2],         4(%[dst])      \n\t"  

            : [tp1] "=&r" (tp1), [tp2] "=&r" (tp2)
            : [src] "r" (src), [dst] "r" (dst)
        );

        src += src_stride;
        dst += dst_stride;
      }
      }
      break;
    case 16:
      {
      uint32_t tp1, tp2, tp3, tp4;

      
      for (y = h; y--; ) {
        prefetch_load(src + src_stride);
        prefetch_load(src + src_stride + 32);
        prefetch_store(dst + dst_stride);

        __asm__ __volatile__ (
            "ulw              %[tp1],         0(%[src])      \n\t"
            "ulw              %[tp2],         4(%[src])      \n\t"
            "ulw              %[tp3],         8(%[src])      \n\t"
            "ulw              %[tp4],         12(%[src])     \n\t"

            "sw               %[tp1],         0(%[dst])      \n\t"  
            "sw               %[tp2],         4(%[dst])      \n\t"  
            "sw               %[tp3],         8(%[dst])      \n\t"  
            "sw               %[tp4],         12(%[dst])     \n\t"  

            : [tp1] "=&r" (tp1), [tp2] "=&r" (tp2),
              [tp3] "=&r" (tp3), [tp4] "=&r" (tp4)
            : [src] "r" (src), [dst] "r" (dst)
        );

        src += src_stride;
        dst += dst_stride;
      }
      }
      break;
    case 32:
      {
      uint32_t tp1, tp2, tp3, tp4;
      uint32_t tp5, tp6, tp7, tp8;

      
      for (y = h; y--; ) {
        prefetch_load(src + src_stride);
        prefetch_load(src + src_stride + 32);
        prefetch_store(dst + dst_stride);

        __asm__ __volatile__ (
            "ulw              %[tp1],         0(%[src])      \n\t"
            "ulw              %[tp2],         4(%[src])      \n\t"
            "ulw              %[tp3],         8(%[src])      \n\t"
            "ulw              %[tp4],         12(%[src])     \n\t"
            "ulw              %[tp5],         16(%[src])     \n\t"
            "ulw              %[tp6],         20(%[src])     \n\t"
            "ulw              %[tp7],         24(%[src])     \n\t"
            "ulw              %[tp8],         28(%[src])     \n\t"

            "sw               %[tp1],         0(%[dst])      \n\t"  
            "sw               %[tp2],         4(%[dst])      \n\t"  
            "sw               %[tp3],         8(%[dst])      \n\t"  
            "sw               %[tp4],         12(%[dst])     \n\t"  
            "sw               %[tp5],         16(%[dst])     \n\t"  
            "sw               %[tp6],         20(%[dst])     \n\t"  
            "sw               %[tp7],         24(%[dst])     \n\t"  
            "sw               %[tp8],         28(%[dst])     \n\t"  

            : [tp1] "=&r" (tp1), [tp2] "=&r" (tp2),
              [tp3] "=&r" (tp3), [tp4] "=&r" (tp4),
              [tp5] "=&r" (tp5), [tp6] "=&r" (tp6),
              [tp7] "=&r" (tp7), [tp8] "=&r" (tp8)
            : [src] "r" (src), [dst] "r" (dst)
        );

        src += src_stride;
        dst += dst_stride;
      }
      }
      break;
    case 64:
      {
      uint32_t tp1, tp2, tp3, tp4;
      uint32_t tp5, tp6, tp7, tp8;

      prefetch_load(src + 64);
      prefetch_store(dst + 32);

      
      for (y = h; y--; ) {
        prefetch_load(src + src_stride);
        prefetch_load(src + src_stride + 32);
        prefetch_load(src + src_stride + 64);
        prefetch_store(dst + dst_stride);
        prefetch_store(dst + dst_stride + 32);

        __asm__ __volatile__ (
            "ulw              %[tp1],         0(%[src])      \n\t"
            "ulw              %[tp2],         4(%[src])      \n\t"
            "ulw              %[tp3],         8(%[src])      \n\t"
            "ulw              %[tp4],         12(%[src])     \n\t"
            "ulw              %[tp5],         16(%[src])     \n\t"
            "ulw              %[tp6],         20(%[src])     \n\t"
            "ulw              %[tp7],         24(%[src])     \n\t"
            "ulw              %[tp8],         28(%[src])     \n\t"

            "sw               %[tp1],         0(%[dst])      \n\t"  
            "sw               %[tp2],         4(%[dst])      \n\t"  
            "sw               %[tp3],         8(%[dst])      \n\t"  
            "sw               %[tp4],         12(%[dst])     \n\t"  
            "sw               %[tp5],         16(%[dst])     \n\t"  
            "sw               %[tp6],         20(%[dst])     \n\t"  
            "sw               %[tp7],         24(%[dst])     \n\t"  
            "sw               %[tp8],         28(%[dst])     \n\t"  

            "ulw              %[tp1],         32(%[src])     \n\t"
            "ulw              %[tp2],         36(%[src])     \n\t"
            "ulw              %[tp3],         40(%[src])     \n\t"
            "ulw              %[tp4],         44(%[src])     \n\t"
            "ulw              %[tp5],         48(%[src])     \n\t"
            "ulw              %[tp6],         52(%[src])     \n\t"
            "ulw              %[tp7],         56(%[src])     \n\t"
            "ulw              %[tp8],         60(%[src])     \n\t"

            "sw               %[tp1],         32(%[dst])     \n\t"  
            "sw               %[tp2],         36(%[dst])     \n\t"  
            "sw               %[tp3],         40(%[dst])     \n\t"  
            "sw               %[tp4],         44(%[dst])     \n\t"  
            "sw               %[tp5],         48(%[dst])     \n\t"  
            "sw               %[tp6],         52(%[dst])     \n\t"  
            "sw               %[tp7],         56(%[dst])     \n\t"  
            "sw               %[tp8],         60(%[dst])     \n\t"  

            : [tp1] "=&r" (tp1), [tp2] "=&r" (tp2),
              [tp3] "=&r" (tp3), [tp4] "=&r" (tp4),
              [tp5] "=&r" (tp5), [tp6] "=&r" (tp6),
              [tp7] "=&r" (tp7), [tp8] "=&r" (tp8)
            : [src] "r" (src), [dst] "r" (dst)
        );

        src += src_stride;
        dst += dst_stride;
      }
      }
      break;
    default:
      for (y = h; y--; ) {
        for (x = 0; x < w; ++x) {
          dst[x] = src[x];
        }

        src += src_stride;
        dst += dst_stride;
      }
      break;
  }
}
#endif

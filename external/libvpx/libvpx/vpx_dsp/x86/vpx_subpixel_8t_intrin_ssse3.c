/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <tmmintrin.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/vpx_filter.h"
#include "vpx_dsp/x86/convolve.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem.h"
#include "vpx_ports/emmintrin_compat.h"

DECLARE_ALIGNED(16, static const uint8_t, filt1_4_h8[16]) = {
  0, 1, 1, 2, 2, 3, 3, 4, 2, 3, 3, 4, 4, 5, 5, 6
};

DECLARE_ALIGNED(16, static const uint8_t, filt2_4_h8[16]) = {
  4, 5, 5, 6, 6, 7, 7, 8, 6, 7, 7, 8, 8, 9, 9, 10
};

DECLARE_ALIGNED(16, static const uint8_t, filt1_global[16]) = {
  0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8
};

DECLARE_ALIGNED(16, static const uint8_t, filt2_global[16]) = {
  2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10
};

DECLARE_ALIGNED(16, static const uint8_t, filt3_global[16]) = {
  4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12
};

DECLARE_ALIGNED(16, static const uint8_t, filt4_global[16]) = {
  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14
};

filter8_1dfunction vpx_filter_block1d8_v8_intrin_ssse3;
filter8_1dfunction vpx_filter_block1d8_h8_intrin_ssse3;
filter8_1dfunction vpx_filter_block1d4_h8_intrin_ssse3;

void vpx_filter_block1d4_h8_intrin_ssse3(const uint8_t *src_ptr,
                                         ptrdiff_t src_pixels_per_line,
                                         uint8_t *output_ptr,
                                         ptrdiff_t output_pitch,
                                         uint32_t output_height,
                                         const int16_t *filter) {
  __m128i firstFilters, secondFilters, shuffle1, shuffle2;
  __m128i srcRegFilt1, srcRegFilt2, srcRegFilt3, srcRegFilt4;
  __m128i addFilterReg64, filtersReg, srcReg, minReg;
  unsigned int i;

  
  addFilterReg64 =_mm_set1_epi32((int)0x0400040u);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  
  
  filtersReg =_mm_packs_epi16(filtersReg, filtersReg);

  
  firstFilters = _mm_shufflelo_epi16(filtersReg, 0);
  
  secondFilters = _mm_shufflelo_epi16(filtersReg, 0xAAu);
  
  
  firstFilters = _mm_shufflehi_epi16(firstFilters, 0x55u);
  
  
  secondFilters = _mm_shufflehi_epi16(secondFilters, 0xFFu);

  
  shuffle1 =_mm_load_si128((__m128i const *)filt1_4_h8);
  shuffle2 = _mm_load_si128((__m128i const *)filt2_4_h8);

  for (i = 0; i < output_height; i++) {
    srcReg = _mm_loadu_si128((const __m128i *)(src_ptr - 3));

    
    srcRegFilt1= _mm_shuffle_epi8(srcReg, shuffle1);
    srcRegFilt2= _mm_shuffle_epi8(srcReg, shuffle2);

    
    srcRegFilt1 = _mm_maddubs_epi16(srcRegFilt1, firstFilters);
    srcRegFilt2 = _mm_maddubs_epi16(srcRegFilt2, secondFilters);

    
    srcRegFilt3 =  _mm_srli_si128(srcRegFilt1, 8);
    srcRegFilt4 =  _mm_srli_si128(srcRegFilt2, 8);

    minReg = _mm_min_epi16(srcRegFilt3, srcRegFilt2);

    
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt4);
    srcRegFilt3 = _mm_max_epi16(srcRegFilt3, srcRegFilt2);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, minReg);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt3);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, addFilterReg64);

    
    srcRegFilt1 = _mm_srai_epi16(srcRegFilt1, 7);

    
    srcRegFilt1 = _mm_packus_epi16(srcRegFilt1, srcRegFilt1);
    src_ptr+=src_pixels_per_line;

    
    *((int*)&output_ptr[0])= _mm_cvtsi128_si32(srcRegFilt1);

    output_ptr+=output_pitch;
  }
}

void vpx_filter_block1d8_h8_intrin_ssse3(const uint8_t *src_ptr,
                                         ptrdiff_t src_pixels_per_line,
                                         uint8_t *output_ptr,
                                         ptrdiff_t output_pitch,
                                         uint32_t output_height,
                                         const int16_t *filter) {
  __m128i firstFilters, secondFilters, thirdFilters, forthFilters, srcReg;
  __m128i filt1Reg, filt2Reg, filt3Reg, filt4Reg;
  __m128i srcRegFilt1, srcRegFilt2, srcRegFilt3, srcRegFilt4;
  __m128i addFilterReg64, filtersReg, minReg;
  unsigned int i;

  
  addFilterReg64 = _mm_set1_epi32((int)0x0400040u);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  
  
  filtersReg =_mm_packs_epi16(filtersReg, filtersReg);

  
  
  firstFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x100u));
  
  
  secondFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x302u));
  
  
  thirdFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x504u));
  
  
  forthFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x706u));

  filt1Reg = _mm_load_si128((__m128i const *)filt1_global);
  filt2Reg = _mm_load_si128((__m128i const *)filt2_global);
  filt3Reg = _mm_load_si128((__m128i const *)filt3_global);
  filt4Reg = _mm_load_si128((__m128i const *)filt4_global);

  for (i = 0; i < output_height; i++) {
    srcReg = _mm_loadu_si128((const __m128i *)(src_ptr - 3));

    
    srcRegFilt1= _mm_shuffle_epi8(srcReg, filt1Reg);
    srcRegFilt2= _mm_shuffle_epi8(srcReg, filt2Reg);

    
    srcRegFilt1 = _mm_maddubs_epi16(srcRegFilt1, firstFilters);
    srcRegFilt2 = _mm_maddubs_epi16(srcRegFilt2, secondFilters);

    
    srcRegFilt3= _mm_shuffle_epi8(srcReg, filt3Reg);
    srcRegFilt4= _mm_shuffle_epi8(srcReg, filt4Reg);

    
    srcRegFilt3 = _mm_maddubs_epi16(srcRegFilt3, thirdFilters);
    srcRegFilt4 = _mm_maddubs_epi16(srcRegFilt4, forthFilters);

    
    minReg = _mm_min_epi16(srcRegFilt2, srcRegFilt3);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt4);

    srcRegFilt2= _mm_max_epi16(srcRegFilt2, srcRegFilt3);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, minReg);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt2);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, addFilterReg64);

    
    srcRegFilt1 = _mm_srai_epi16(srcRegFilt1, 7);

    
    srcRegFilt1 = _mm_packus_epi16(srcRegFilt1, srcRegFilt1);

    src_ptr+=src_pixels_per_line;

    
    _mm_storel_epi64((__m128i*)&output_ptr[0], srcRegFilt1);

    output_ptr+=output_pitch;
  }
}

static void vpx_filter_block1d16_h8_intrin_ssse3(const uint8_t *src_ptr,
                                                 ptrdiff_t src_pixels_per_line,
                                                 uint8_t *output_ptr,
                                                 ptrdiff_t output_pitch,
                                                 uint32_t output_height,
                                                 const int16_t *filter) {
  __m128i addFilterReg64, filtersReg, srcReg1, srcReg2;
  __m128i filt1Reg, filt2Reg, filt3Reg, filt4Reg;
  __m128i firstFilters, secondFilters, thirdFilters, forthFilters;
  __m128i srcRegFilt1_1, srcRegFilt2_1, srcRegFilt2, srcRegFilt3;
  unsigned int i;

  
  addFilterReg64 = _mm_set1_epi32((int)0x0400040u);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  
  
  filtersReg =_mm_packs_epi16(filtersReg, filtersReg);

  
  
  firstFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x100u));
  
  
  secondFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x302u));
  
  
  thirdFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x504u));
  
  
  forthFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x706u));

  filt1Reg = _mm_load_si128((__m128i const *)filt1_global);
  filt2Reg = _mm_load_si128((__m128i const *)filt2_global);
  filt3Reg = _mm_load_si128((__m128i const *)filt3_global);
  filt4Reg = _mm_load_si128((__m128i const *)filt4_global);

  for (i = 0; i < output_height; i++) {
    srcReg1 = _mm_loadu_si128((const __m128i *)(src_ptr - 3));

    
    srcRegFilt1_1= _mm_shuffle_epi8(srcReg1, filt1Reg);
    srcRegFilt2= _mm_shuffle_epi8(srcReg1, filt4Reg);

    
    srcRegFilt1_1 = _mm_maddubs_epi16(srcRegFilt1_1, firstFilters);
    srcRegFilt2 = _mm_maddubs_epi16(srcRegFilt2, forthFilters);

    
    srcRegFilt1_1 = _mm_adds_epi16(srcRegFilt1_1, srcRegFilt2);

    
    srcRegFilt3= _mm_shuffle_epi8(srcReg1, filt2Reg);
    srcRegFilt2= _mm_shuffle_epi8(srcReg1, filt3Reg);

    
    srcRegFilt3 = _mm_maddubs_epi16(srcRegFilt3, secondFilters);
    srcRegFilt2 = _mm_maddubs_epi16(srcRegFilt2, thirdFilters);

    
    srcRegFilt1_1 = _mm_adds_epi16(srcRegFilt1_1,
                                   _mm_min_epi16(srcRegFilt3, srcRegFilt2));

    
    
    srcReg2 = _mm_loadu_si128((const __m128i *)(src_ptr + 5));

    
    srcRegFilt1_1 = _mm_adds_epi16(srcRegFilt1_1,
                                   _mm_max_epi16(srcRegFilt3, srcRegFilt2));

    
    srcRegFilt2_1= _mm_shuffle_epi8(srcReg2, filt1Reg);
    srcRegFilt2= _mm_shuffle_epi8(srcReg2, filt4Reg);

    
    srcRegFilt2_1 = _mm_maddubs_epi16(srcRegFilt2_1, firstFilters);
    srcRegFilt2 = _mm_maddubs_epi16(srcRegFilt2, forthFilters);

    
    srcRegFilt2_1 = _mm_adds_epi16(srcRegFilt2_1, srcRegFilt2);

    
    srcRegFilt3= _mm_shuffle_epi8(srcReg2, filt2Reg);
    srcRegFilt2= _mm_shuffle_epi8(srcReg2, filt3Reg);

    
    srcRegFilt3 = _mm_maddubs_epi16(srcRegFilt3, secondFilters);
    srcRegFilt2 = _mm_maddubs_epi16(srcRegFilt2, thirdFilters);

    
    srcRegFilt2_1 = _mm_adds_epi16(srcRegFilt2_1,
    _mm_min_epi16(srcRegFilt3, srcRegFilt2));
    srcRegFilt2_1 = _mm_adds_epi16(srcRegFilt2_1,
    _mm_max_epi16(srcRegFilt3, srcRegFilt2));

    srcRegFilt1_1 = _mm_adds_epi16(srcRegFilt1_1, addFilterReg64);
    srcRegFilt2_1 = _mm_adds_epi16(srcRegFilt2_1, addFilterReg64);

    
    srcRegFilt1_1 = _mm_srai_epi16(srcRegFilt1_1, 7);
    srcRegFilt2_1 = _mm_srai_epi16(srcRegFilt2_1, 7);

    
    
    
    srcRegFilt1_1 = _mm_packus_epi16(srcRegFilt1_1, srcRegFilt2_1);

    src_ptr+=src_pixels_per_line;

    
    _mm_store_si128((__m128i*)output_ptr, srcRegFilt1_1);

    output_ptr+=output_pitch;
  }
}

void vpx_filter_block1d8_v8_intrin_ssse3(const uint8_t *src_ptr,
                                         ptrdiff_t src_pitch,
                                         uint8_t *output_ptr,
                                         ptrdiff_t out_pitch,
                                         uint32_t output_height,
                                         const int16_t *filter) {
  __m128i addFilterReg64, filtersReg, minReg;
  __m128i firstFilters, secondFilters, thirdFilters, forthFilters;
  __m128i srcRegFilt1, srcRegFilt2, srcRegFilt3, srcRegFilt5;
  __m128i srcReg1, srcReg2, srcReg3, srcReg4, srcReg5, srcReg6, srcReg7;
  __m128i srcReg8;
  unsigned int i;

  
  addFilterReg64 = _mm_set1_epi32((int)0x0400040u);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  
  
  filtersReg =_mm_packs_epi16(filtersReg, filtersReg);

  
  firstFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x100u));
  
  secondFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x302u));
  
  thirdFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x504u));
  
  forthFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x706u));

  
  srcReg1 = _mm_loadl_epi64((const __m128i *)src_ptr);
  srcReg2 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch));
  srcReg3 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg4 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg5 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 4));
  srcReg6 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 5));
  srcReg7 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 6));

  for (i = 0; i < output_height; i++) {
    
    srcReg8 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 7));

    
    srcRegFilt1 = _mm_unpacklo_epi8(srcReg1, srcReg2);
    srcRegFilt3 = _mm_unpacklo_epi8(srcReg3, srcReg4);

    
    srcRegFilt2 = _mm_unpacklo_epi8(srcReg5, srcReg6);
    srcRegFilt5 = _mm_unpacklo_epi8(srcReg7, srcReg8);

    
    srcRegFilt1 = _mm_maddubs_epi16(srcRegFilt1, firstFilters);
    srcRegFilt3 = _mm_maddubs_epi16(srcRegFilt3, secondFilters);
    srcRegFilt2 = _mm_maddubs_epi16(srcRegFilt2, thirdFilters);
    srcRegFilt5 = _mm_maddubs_epi16(srcRegFilt5, forthFilters);

    
    minReg = _mm_min_epi16(srcRegFilt2, srcRegFilt3);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt5);
    srcRegFilt2 = _mm_max_epi16(srcRegFilt2, srcRegFilt3);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, minReg);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt2);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, addFilterReg64);

    
    srcRegFilt1 = _mm_srai_epi16(srcRegFilt1, 7);

    
    srcRegFilt1 = _mm_packus_epi16(srcRegFilt1, srcRegFilt1);

    src_ptr+=src_pitch;

    
    srcReg1 = srcReg2;
    srcReg2 = srcReg3;
    srcReg3 = srcReg4;
    srcReg4 = srcReg5;
    srcReg5 = srcReg6;
    srcReg6 = srcReg7;
    srcReg7 = srcReg8;

    
    _mm_storel_epi64((__m128i*)&output_ptr[0], srcRegFilt1);

    output_ptr+=out_pitch;
  }
}

static void vpx_filter_block1d16_v8_intrin_ssse3(const uint8_t *src_ptr,
                                                 ptrdiff_t src_pitch,
                                                 uint8_t *output_ptr,
                                                 ptrdiff_t out_pitch,
                                                 uint32_t output_height,
                                                 const int16_t *filter) {
  __m128i addFilterReg64, filtersReg, srcRegFilt1, srcRegFilt3;
  __m128i firstFilters, secondFilters, thirdFilters, forthFilters;
  __m128i srcRegFilt5, srcRegFilt6, srcRegFilt7, srcRegFilt8;
  __m128i srcReg1, srcReg2, srcReg3, srcReg4, srcReg5, srcReg6, srcReg7;
  __m128i srcReg8;
  unsigned int i;

  
  addFilterReg64 = _mm_set1_epi32((int)0x0400040u);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  
  
  filtersReg =_mm_packs_epi16(filtersReg, filtersReg);

  
  firstFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x100u));
  
  secondFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x302u));
  
  thirdFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x504u));
  
  forthFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x706u));

  
  srcReg1 = _mm_loadu_si128((const __m128i *)(src_ptr));
  srcReg2 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch));
  srcReg3 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg4 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg5 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 4));
  srcReg6 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 5));
  srcReg7 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 6));

  for (i = 0; i < output_height; i++) {
    
    srcReg8 = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 7));

    
    srcRegFilt5 = _mm_unpacklo_epi8(srcReg1, srcReg2);
    srcRegFilt6 = _mm_unpacklo_epi8(srcReg7, srcReg8);
    srcRegFilt1 = _mm_unpackhi_epi8(srcReg1, srcReg2);
    srcRegFilt3 = _mm_unpackhi_epi8(srcReg7, srcReg8);

    
    srcRegFilt5 = _mm_maddubs_epi16(srcRegFilt5, firstFilters);
    srcRegFilt6 = _mm_maddubs_epi16(srcRegFilt6, forthFilters);
    srcRegFilt1 = _mm_maddubs_epi16(srcRegFilt1, firstFilters);
    srcRegFilt3 = _mm_maddubs_epi16(srcRegFilt3, forthFilters);

    
    srcRegFilt5 = _mm_adds_epi16(srcRegFilt5, srcRegFilt6);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt3);

    
    srcRegFilt3 = _mm_unpacklo_epi8(srcReg3, srcReg4);
    srcRegFilt6 = _mm_unpackhi_epi8(srcReg3, srcReg4);

    
    srcRegFilt3 = _mm_maddubs_epi16(srcRegFilt3, secondFilters);
    srcRegFilt6 = _mm_maddubs_epi16(srcRegFilt6, secondFilters);

    
    srcRegFilt7 = _mm_unpacklo_epi8(srcReg5, srcReg6);
    srcRegFilt8 = _mm_unpackhi_epi8(srcReg5, srcReg6);

    
    srcRegFilt7 = _mm_maddubs_epi16(srcRegFilt7, thirdFilters);
    srcRegFilt8 = _mm_maddubs_epi16(srcRegFilt8, thirdFilters);

    
    srcRegFilt5 = _mm_adds_epi16(srcRegFilt5,
                                 _mm_min_epi16(srcRegFilt3, srcRegFilt7));
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1,
                                 _mm_min_epi16(srcRegFilt6, srcRegFilt8));

    
    srcRegFilt5 = _mm_adds_epi16(srcRegFilt5,
                                 _mm_max_epi16(srcRegFilt3, srcRegFilt7));
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1,
                                 _mm_max_epi16(srcRegFilt6, srcRegFilt8));
    srcRegFilt5 = _mm_adds_epi16(srcRegFilt5, addFilterReg64);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, addFilterReg64);

    
    srcRegFilt5 = _mm_srai_epi16(srcRegFilt5, 7);
    srcRegFilt1 = _mm_srai_epi16(srcRegFilt1, 7);

    
    
    
    srcRegFilt1 = _mm_packus_epi16(srcRegFilt5, srcRegFilt1);

    src_ptr+=src_pitch;

    
    srcReg1 = srcReg2;
    srcReg2 = srcReg3;
    srcReg3 = srcReg4;
    srcReg4 = srcReg5;
    srcReg5 = srcReg6;
    srcReg6 = srcReg7;
    srcReg7 = srcReg8;

    
    _mm_store_si128((__m128i*)output_ptr, srcRegFilt1);

    output_ptr+=out_pitch;
  }
}

#if ARCH_X86_64
filter8_1dfunction vpx_filter_block1d16_v8_intrin_ssse3;
filter8_1dfunction vpx_filter_block1d16_h8_intrin_ssse3;
filter8_1dfunction vpx_filter_block1d8_v8_intrin_ssse3;
filter8_1dfunction vpx_filter_block1d8_h8_intrin_ssse3;
filter8_1dfunction vpx_filter_block1d4_v8_ssse3;
filter8_1dfunction vpx_filter_block1d4_h8_intrin_ssse3;
#define vpx_filter_block1d16_v8_ssse3 vpx_filter_block1d16_v8_intrin_ssse3
#define vpx_filter_block1d16_h8_ssse3 vpx_filter_block1d16_h8_intrin_ssse3
#define vpx_filter_block1d8_v8_ssse3 vpx_filter_block1d8_v8_intrin_ssse3
#define vpx_filter_block1d8_h8_ssse3 vpx_filter_block1d8_h8_intrin_ssse3
#define vpx_filter_block1d4_h8_ssse3 vpx_filter_block1d4_h8_intrin_ssse3
#else  
filter8_1dfunction vpx_filter_block1d16_v8_ssse3;
filter8_1dfunction vpx_filter_block1d16_h8_ssse3;
filter8_1dfunction vpx_filter_block1d8_v8_ssse3;
filter8_1dfunction vpx_filter_block1d8_h8_ssse3;
filter8_1dfunction vpx_filter_block1d4_v8_ssse3;
filter8_1dfunction vpx_filter_block1d4_h8_ssse3;
#endif  
filter8_1dfunction vpx_filter_block1d16_v8_avg_ssse3;
filter8_1dfunction vpx_filter_block1d16_h8_avg_ssse3;
filter8_1dfunction vpx_filter_block1d8_v8_avg_ssse3;
filter8_1dfunction vpx_filter_block1d8_h8_avg_ssse3;
filter8_1dfunction vpx_filter_block1d4_v8_avg_ssse3;
filter8_1dfunction vpx_filter_block1d4_h8_avg_ssse3;

filter8_1dfunction vpx_filter_block1d16_v2_ssse3;
filter8_1dfunction vpx_filter_block1d16_h2_ssse3;
filter8_1dfunction vpx_filter_block1d8_v2_ssse3;
filter8_1dfunction vpx_filter_block1d8_h2_ssse3;
filter8_1dfunction vpx_filter_block1d4_v2_ssse3;
filter8_1dfunction vpx_filter_block1d4_h2_ssse3;
filter8_1dfunction vpx_filter_block1d16_v2_avg_ssse3;
filter8_1dfunction vpx_filter_block1d16_h2_avg_ssse3;
filter8_1dfunction vpx_filter_block1d8_v2_avg_ssse3;
filter8_1dfunction vpx_filter_block1d8_h2_avg_ssse3;
filter8_1dfunction vpx_filter_block1d4_v2_avg_ssse3;
filter8_1dfunction vpx_filter_block1d4_h2_avg_ssse3;

FUN_CONV_1D(horiz, x_step_q4, filter_x, h, src, , ssse3);
FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , ssse3);
FUN_CONV_1D(avg_horiz, x_step_q4, filter_x, h, src, avg_, ssse3);
FUN_CONV_1D(avg_vert, y_step_q4, filter_y, v, src - src_stride * 3, avg_,
            ssse3);

#define TRANSPOSE_8X8(in0, in1, in2, in3, in4, in5, in6, in7,           \
                      out0, out1, out2, out3, out4, out5, out6, out7) { \
  const __m128i tr0_0 = _mm_unpacklo_epi8(in0, in1);                    \
  const __m128i tr0_1 = _mm_unpacklo_epi8(in2, in3);                    \
  const __m128i tr0_2 = _mm_unpacklo_epi8(in4, in5);                    \
  const __m128i tr0_3 = _mm_unpacklo_epi8(in6, in7);                    \
                                                                        \
  const __m128i tr1_0 = _mm_unpacklo_epi16(tr0_0, tr0_1);               \
  const __m128i tr1_1 = _mm_unpackhi_epi16(tr0_0, tr0_1);               \
  const __m128i tr1_2 = _mm_unpacklo_epi16(tr0_2, tr0_3);               \
  const __m128i tr1_3 = _mm_unpackhi_epi16(tr0_2, tr0_3);               \
                                                                        \
  const __m128i tr2_0 = _mm_unpacklo_epi32(tr1_0, tr1_2);               \
  const __m128i tr2_1 = _mm_unpackhi_epi32(tr1_0, tr1_2);               \
  const __m128i tr2_2 = _mm_unpacklo_epi32(tr1_1, tr1_3);               \
  const __m128i tr2_3 = _mm_unpackhi_epi32(tr1_1, tr1_3);               \
                                                                        \
  out0 = _mm_unpacklo_epi64(tr2_0, tr2_0);                              \
  out1 = _mm_unpackhi_epi64(tr2_0, tr2_0);                              \
  out2 = _mm_unpacklo_epi64(tr2_1, tr2_1);                              \
  out3 = _mm_unpackhi_epi64(tr2_1, tr2_1);                              \
  out4 = _mm_unpacklo_epi64(tr2_2, tr2_2);                              \
  out5 = _mm_unpackhi_epi64(tr2_2, tr2_2);                              \
  out6 = _mm_unpacklo_epi64(tr2_3, tr2_3);                              \
  out7 = _mm_unpackhi_epi64(tr2_3, tr2_3);                              \
}

static void filter_horiz_w8_ssse3(const uint8_t *src_x, ptrdiff_t src_pitch,
                                  uint8_t *dst, const int16_t *x_filter) {
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  const __m128i f_values = _mm_load_si128((const __m128i *)x_filter);
  
  const __m128i f1f0 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0200u));
  const __m128i f3f2 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0604u));
  const __m128i f5f4 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0a08u));
  const __m128i f7f6 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0e0cu));
  const __m128i A = _mm_loadl_epi64((const __m128i *)src_x);
  const __m128i B = _mm_loadl_epi64((const __m128i *)(src_x + src_pitch));
  const __m128i C = _mm_loadl_epi64((const __m128i *)(src_x + src_pitch * 2));
  const __m128i D = _mm_loadl_epi64((const __m128i *)(src_x + src_pitch * 3));
  const __m128i E = _mm_loadl_epi64((const __m128i *)(src_x + src_pitch * 4));
  const __m128i F = _mm_loadl_epi64((const __m128i *)(src_x + src_pitch * 5));
  const __m128i G = _mm_loadl_epi64((const __m128i *)(src_x + src_pitch * 6));
  const __m128i H = _mm_loadl_epi64((const __m128i *)(src_x + src_pitch * 7));
  
  const __m128i tr0_0 = _mm_unpacklo_epi16(A, B);
  
  const __m128i tr0_1 = _mm_unpacklo_epi16(C, D);
  
  const __m128i tr0_2 = _mm_unpacklo_epi16(E, F);
  
  const __m128i tr0_3 = _mm_unpacklo_epi16(G, H);
  
  const __m128i tr1_0 = _mm_unpacklo_epi32(tr0_0, tr0_1);
  
  const __m128i tr1_1 = _mm_unpackhi_epi32(tr0_0, tr0_1);
  
  const __m128i tr1_2 = _mm_unpacklo_epi32(tr0_2, tr0_3);
  
  const __m128i tr1_3 = _mm_unpackhi_epi32(tr0_2, tr0_3);
  
  const __m128i s1s0 = _mm_unpacklo_epi64(tr1_0, tr1_2);
  const __m128i s3s2 = _mm_unpackhi_epi64(tr1_0, tr1_2);
  const __m128i s5s4 = _mm_unpacklo_epi64(tr1_1, tr1_3);
  const __m128i s7s6 = _mm_unpackhi_epi64(tr1_1, tr1_3);
  
  const __m128i x0 = _mm_maddubs_epi16(s1s0, f1f0);
  const __m128i x1 = _mm_maddubs_epi16(s3s2, f3f2);
  const __m128i x2 = _mm_maddubs_epi16(s5s4, f5f4);
  const __m128i x3 = _mm_maddubs_epi16(s7s6, f7f6);
  
  const __m128i min_x2x1 = _mm_min_epi16(x2, x1);
  const __m128i max_x2x1 = _mm_max_epi16(x2, x1);
  __m128i temp = _mm_adds_epi16(x0, x3);
  temp = _mm_adds_epi16(temp, min_x2x1);
  temp = _mm_adds_epi16(temp, max_x2x1);
  
  temp = _mm_mulhrs_epi16(temp, k_256);
  
  temp = _mm_packus_epi16(temp, temp);
  
  _mm_storel_epi64((__m128i*)dst, temp);
}

static void transpose8x8_to_dst(const uint8_t *src, ptrdiff_t src_stride,
                                uint8_t *dst, ptrdiff_t dst_stride) {
  __m128i A, B, C, D, E, F, G, H;

  A = _mm_loadl_epi64((const __m128i *)src);
  B = _mm_loadl_epi64((const __m128i *)(src + src_stride));
  C = _mm_loadl_epi64((const __m128i *)(src + src_stride * 2));
  D = _mm_loadl_epi64((const __m128i *)(src + src_stride * 3));
  E = _mm_loadl_epi64((const __m128i *)(src + src_stride * 4));
  F = _mm_loadl_epi64((const __m128i *)(src + src_stride * 5));
  G = _mm_loadl_epi64((const __m128i *)(src + src_stride * 6));
  H = _mm_loadl_epi64((const __m128i *)(src + src_stride * 7));

  TRANSPOSE_8X8(A, B, C, D, E, F, G, H,
                A, B, C, D, E, F, G, H);

  _mm_storel_epi64((__m128i*)dst, A);
  _mm_storel_epi64((__m128i*)(dst + dst_stride * 1), B);
  _mm_storel_epi64((__m128i*)(dst + dst_stride * 2), C);
  _mm_storel_epi64((__m128i*)(dst + dst_stride * 3), D);
  _mm_storel_epi64((__m128i*)(dst + dst_stride * 4), E);
  _mm_storel_epi64((__m128i*)(dst + dst_stride * 5), F);
  _mm_storel_epi64((__m128i*)(dst + dst_stride * 6), G);
  _mm_storel_epi64((__m128i*)(dst + dst_stride * 7), H);
}

static void scaledconvolve_horiz_w8(const uint8_t *src, ptrdiff_t src_stride,
                                    uint8_t *dst, ptrdiff_t dst_stride,
                                    const InterpKernel *x_filters,
                                    int x0_q4, int x_step_q4, int w, int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[8 * 8]);
  int x, y, z;
  src -= SUBPEL_TAPS / 2 - 1;

  
  
  y = h + (8 - (h & 0x7));

  do {
    int x_q4 = x0_q4;
    for (x = 0; x < w; x += 8) {
      
      for (z = 0; z < 8; ++z) {
        const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
        const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
        if (x_q4 & SUBPEL_MASK) {
          filter_horiz_w8_ssse3(src_x, src_stride, temp + (z * 8), x_filter);
        } else {
          int i;
          for (i = 0; i < 8; ++i) {
            temp[z * 8 + i] = src_x[i * src_stride + 3];
          }
        }
        x_q4 += x_step_q4;
      }

      
      transpose8x8_to_dst(temp, 8, dst + x, dst_stride);
    }

    src += src_stride * 8;
    dst += dst_stride * 8;
  } while (y -= 8);
}

static void filter_horiz_w4_ssse3(const uint8_t *src_ptr, ptrdiff_t src_pitch,
                                  uint8_t *dst, const int16_t *filter) {
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  const __m128i f_values = _mm_load_si128((const __m128i *)filter);
  
  const __m128i f1f0 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0200u));
  const __m128i f3f2 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0604u));
  const __m128i f5f4 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0a08u));
  const __m128i f7f6 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0e0cu));
  const __m128i A = _mm_loadl_epi64((const __m128i *)src_ptr);
  const __m128i B = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch));
  const __m128i C = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 2));
  const __m128i D = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 3));
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  
  const __m128i tr0_0 = _mm_unpacklo_epi16(A, B);
  
  const __m128i tr0_1 = _mm_unpacklo_epi16(C, D);
  
  const __m128i s1s0  = _mm_unpacklo_epi32(tr0_0, tr0_1);
  
  const __m128i s5s4 = _mm_unpackhi_epi32(tr0_0, tr0_1);
  
  const __m128i s3s2 = _mm_srli_si128(s1s0, 8);
  
  const __m128i s7s6 = _mm_srli_si128(s5s4, 8);
  
  const __m128i x0 = _mm_maddubs_epi16(s1s0, f1f0);
  const __m128i x1 = _mm_maddubs_epi16(s3s2, f3f2);
  const __m128i x2 = _mm_maddubs_epi16(s5s4, f5f4);
  const __m128i x3 = _mm_maddubs_epi16(s7s6, f7f6);
  
  const __m128i min_x2x1 = _mm_min_epi16(x2, x1);
  const __m128i max_x2x1 = _mm_max_epi16(x2, x1);
  __m128i temp = _mm_adds_epi16(x0, x3);
  temp = _mm_adds_epi16(temp, min_x2x1);
  temp = _mm_adds_epi16(temp, max_x2x1);
  
  temp = _mm_mulhrs_epi16(temp, k_256);
  
  temp = _mm_packus_epi16(temp, temp);
  
  *(int *)dst = _mm_cvtsi128_si32(temp);
}

static void transpose4x4_to_dst(const uint8_t *src, ptrdiff_t src_stride,
                                uint8_t *dst, ptrdiff_t dst_stride) {
  __m128i A = _mm_cvtsi32_si128(*(const int *)src);
  __m128i B = _mm_cvtsi32_si128(*(const int *)(src + src_stride));
  __m128i C = _mm_cvtsi32_si128(*(const int *)(src + src_stride * 2));
  __m128i D = _mm_cvtsi32_si128(*(const int *)(src + src_stride * 3));
  
  const __m128i tr0_0 = _mm_unpacklo_epi8(A, B);
  
  const __m128i tr0_1 = _mm_unpacklo_epi8(C, D);
  
  A = _mm_unpacklo_epi16(tr0_0, tr0_1);
  B = _mm_srli_si128(A, 4);
  C = _mm_srli_si128(A, 8);
  D = _mm_srli_si128(A, 12);

  *(int *)(dst) =  _mm_cvtsi128_si32(A);
  *(int *)(dst + dst_stride) =  _mm_cvtsi128_si32(B);
  *(int *)(dst + dst_stride * 2) =  _mm_cvtsi128_si32(C);
  *(int *)(dst + dst_stride * 3) =  _mm_cvtsi128_si32(D);
}

static void scaledconvolve_horiz_w4(const uint8_t *src, ptrdiff_t src_stride,
                                    uint8_t *dst, ptrdiff_t dst_stride,
                                    const InterpKernel *x_filters,
                                    int x0_q4, int x_step_q4, int w, int h) {
  DECLARE_ALIGNED(16, uint8_t, temp[4 * 4]);
  int x, y, z;
  src -= SUBPEL_TAPS / 2 - 1;

  for (y = 0; y < h; y += 4) {
    int x_q4 = x0_q4;
    for (x = 0; x < w; x += 4) {
      
      for (z = 0; z < 4; ++z) {
        const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
        const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
        if (x_q4 & SUBPEL_MASK) {
          filter_horiz_w4_ssse3(src_x, src_stride, temp + (z * 4), x_filter);
        } else {
          int i;
          for (i = 0; i < 4; ++i) {
            temp[z * 4 + i] = src_x[i * src_stride + 3];
          }
        }
        x_q4 += x_step_q4;
      }

      
      transpose4x4_to_dst(temp, 4, dst + x, dst_stride);
    }

    src += src_stride * 4;
    dst += dst_stride * 4;
  }
}

static void filter_vert_w4_ssse3(const uint8_t *src_ptr, ptrdiff_t src_pitch,
                                 uint8_t *dst, const int16_t *filter) {
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  const __m128i f_values = _mm_load_si128((const __m128i *)filter);
  
  const __m128i f1f0 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0200u));
  const __m128i f3f2 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0604u));
  const __m128i f5f4 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0a08u));
  const __m128i f7f6 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0e0cu));
  const __m128i A = _mm_cvtsi32_si128(*(const int *)src_ptr);
  const __m128i B = _mm_cvtsi32_si128(*(const int *)(src_ptr + src_pitch));
  const __m128i C = _mm_cvtsi32_si128(*(const int *)(src_ptr + src_pitch * 2));
  const __m128i D = _mm_cvtsi32_si128(*(const int *)(src_ptr + src_pitch * 3));
  const __m128i E = _mm_cvtsi32_si128(*(const int *)(src_ptr + src_pitch * 4));
  const __m128i F = _mm_cvtsi32_si128(*(const int *)(src_ptr + src_pitch * 5));
  const __m128i G = _mm_cvtsi32_si128(*(const int *)(src_ptr + src_pitch * 6));
  const __m128i H = _mm_cvtsi32_si128(*(const int *)(src_ptr + src_pitch * 7));
  const __m128i s1s0 = _mm_unpacklo_epi8(A, B);
  const __m128i s3s2 = _mm_unpacklo_epi8(C, D);
  const __m128i s5s4 = _mm_unpacklo_epi8(E, F);
  const __m128i s7s6 = _mm_unpacklo_epi8(G, H);
  
  const __m128i x0 = _mm_maddubs_epi16(s1s0, f1f0);
  const __m128i x1 = _mm_maddubs_epi16(s3s2, f3f2);
  const __m128i x2 = _mm_maddubs_epi16(s5s4, f5f4);
  const __m128i x3 = _mm_maddubs_epi16(s7s6, f7f6);
  
  const __m128i min_x2x1 = _mm_min_epi16(x2, x1);
  const __m128i max_x2x1 = _mm_max_epi16(x2, x1);
  __m128i temp = _mm_adds_epi16(x0, x3);
  temp = _mm_adds_epi16(temp, min_x2x1);
  temp = _mm_adds_epi16(temp, max_x2x1);
  
  temp = _mm_mulhrs_epi16(temp, k_256);
  
  temp = _mm_packus_epi16(temp, temp);
  
  *(int *)dst = _mm_cvtsi128_si32(temp);
}

static void scaledconvolve_vert_w4(const uint8_t *src, ptrdiff_t src_stride,
                                   uint8_t *dst, ptrdiff_t dst_stride,
                                   const InterpKernel *y_filters,
                                   int y0_q4, int y_step_q4, int w, int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (y = 0; y < h; ++y) {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];

    if (y_q4 & SUBPEL_MASK) {
      filter_vert_w4_ssse3(src_y, src_stride, &dst[y * dst_stride], y_filter);
    } else {
      memcpy(&dst[y * dst_stride], &src_y[3 * src_stride], w);
    }

    y_q4 += y_step_q4;
  }
}

static void filter_vert_w8_ssse3(const uint8_t *src_ptr, ptrdiff_t src_pitch,
                                 uint8_t *dst, const int16_t *filter) {
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  const __m128i f_values = _mm_load_si128((const __m128i *)filter);
  
  const __m128i f1f0 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0200u));
  const __m128i f3f2 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0604u));
  const __m128i f5f4 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0a08u));
  const __m128i f7f6 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0e0cu));
  const __m128i A = _mm_loadl_epi64((const __m128i *)src_ptr);
  const __m128i B = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch));
  const __m128i C = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 2));
  const __m128i D = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 3));
  const __m128i E = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 4));
  const __m128i F = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 5));
  const __m128i G = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 6));
  const __m128i H = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 7));
  const __m128i s1s0 = _mm_unpacklo_epi8(A, B);
  const __m128i s3s2 = _mm_unpacklo_epi8(C, D);
  const __m128i s5s4 = _mm_unpacklo_epi8(E, F);
  const __m128i s7s6 = _mm_unpacklo_epi8(G, H);
  
  const __m128i x0 = _mm_maddubs_epi16(s1s0, f1f0);
  const __m128i x1 = _mm_maddubs_epi16(s3s2, f3f2);
  const __m128i x2 = _mm_maddubs_epi16(s5s4, f5f4);
  const __m128i x3 = _mm_maddubs_epi16(s7s6, f7f6);
  
  const __m128i min_x2x1 = _mm_min_epi16(x2, x1);
  const __m128i max_x2x1 = _mm_max_epi16(x2, x1);
  __m128i temp = _mm_adds_epi16(x0, x3);
  temp = _mm_adds_epi16(temp, min_x2x1);
  temp = _mm_adds_epi16(temp, max_x2x1);
  
  temp = _mm_mulhrs_epi16(temp, k_256);
  
  temp = _mm_packus_epi16(temp, temp);
  
  _mm_storel_epi64((__m128i*)dst, temp);
}

static void scaledconvolve_vert_w8(const uint8_t *src, ptrdiff_t src_stride,
                                   uint8_t *dst, ptrdiff_t dst_stride,
                                   const InterpKernel *y_filters,
                                   int y0_q4, int y_step_q4, int w, int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (y = 0; y < h; ++y) {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
    if (y_q4 & SUBPEL_MASK) {
      filter_vert_w8_ssse3(src_y, src_stride, &dst[y * dst_stride], y_filter);
    } else {
      memcpy(&dst[y * dst_stride], &src_y[3 * src_stride], w);
    }
    y_q4 += y_step_q4;
  }
}

static void filter_vert_w16_ssse3(const uint8_t *src_ptr, ptrdiff_t src_pitch,
                                  uint8_t *dst, const int16_t *filter, int w) {
  const __m128i k_256 = _mm_set1_epi16(1 << 8);
  const __m128i f_values = _mm_load_si128((const __m128i *)filter);
  
  const __m128i f1f0 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0200u));
  const __m128i f3f2 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0604u));
  const __m128i f5f4 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0a08u));
  const __m128i f7f6 = _mm_shuffle_epi8(f_values, _mm_set1_epi16(0x0e0cu));
  int i;

  for (i = 0; i < w; i += 16) {
    const __m128i A = _mm_loadu_si128((const __m128i *)src_ptr);
    const __m128i B = _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch));
    const __m128i C =
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 2));
    const __m128i D =
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 3));
    const __m128i E =
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 4));
    const __m128i F =
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 5));
    const __m128i G =
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 6));
    const __m128i H =
        _mm_loadu_si128((const __m128i *)(src_ptr + src_pitch * 7));
    
    const __m128i s1s0_lo = _mm_unpacklo_epi8(A, B);
    const __m128i s7s6_lo = _mm_unpacklo_epi8(G, H);
    const __m128i s1s0_hi = _mm_unpackhi_epi8(A, B);
    const __m128i s7s6_hi = _mm_unpackhi_epi8(G, H);
    
    const __m128i x0_lo = _mm_maddubs_epi16(s1s0_lo, f1f0);
    const __m128i x3_lo = _mm_maddubs_epi16(s7s6_lo, f7f6);
    const __m128i x0_hi = _mm_maddubs_epi16(s1s0_hi, f1f0);
    const __m128i x3_hi = _mm_maddubs_epi16(s7s6_hi, f7f6);
    
    const __m128i x3x0_lo = _mm_adds_epi16(x0_lo, x3_lo);
    const __m128i x3x0_hi = _mm_adds_epi16(x0_hi, x3_hi);
    
    const __m128i s3s2_lo = _mm_unpacklo_epi8(C, D);
    const __m128i s3s2_hi = _mm_unpackhi_epi8(C, D);
    
    const __m128i x1_lo = _mm_maddubs_epi16(s3s2_lo, f3f2);
    const __m128i x1_hi = _mm_maddubs_epi16(s3s2_hi, f3f2);
    
    const __m128i s5s4_lo = _mm_unpacklo_epi8(E, F);
    const __m128i s5s4_hi = _mm_unpackhi_epi8(E, F);
    
    const __m128i x2_lo = _mm_maddubs_epi16(s5s4_lo, f5f4);
    const __m128i x2_hi = _mm_maddubs_epi16(s5s4_hi, f5f4);
    
    __m128i temp_lo = _mm_adds_epi16(x3x0_lo, _mm_min_epi16(x1_lo, x2_lo));
    __m128i temp_hi = _mm_adds_epi16(x3x0_hi, _mm_min_epi16(x1_hi, x2_hi));

    
    temp_lo = _mm_adds_epi16(temp_lo, _mm_max_epi16(x1_lo, x2_lo));
    temp_hi = _mm_adds_epi16(temp_hi, _mm_max_epi16(x1_hi, x2_hi));
    
    temp_lo = _mm_mulhrs_epi16(temp_lo, k_256);
    temp_hi = _mm_mulhrs_epi16(temp_hi, k_256);
    
    
    
    temp_hi = _mm_packus_epi16(temp_lo, temp_hi);
    src_ptr += 16;
     
    _mm_store_si128((__m128i*)&dst[i], temp_hi);
  }
}

static void scaledconvolve_vert_w16(const uint8_t *src, ptrdiff_t src_stride,
                                    uint8_t *dst, ptrdiff_t dst_stride,
                                    const InterpKernel *y_filters,
                                    int y0_q4, int y_step_q4, int w, int h) {
  int y;
  int y_q4 = y0_q4;

  src -= src_stride * (SUBPEL_TAPS / 2 - 1);
  for (y = 0; y < h; ++y) {
    const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
    const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
    if (y_q4 & SUBPEL_MASK) {
      filter_vert_w16_ssse3(src_y, src_stride, &dst[y * dst_stride], y_filter,
                            w);
    } else {
      memcpy(&dst[y * dst_stride], &src_y[3 * src_stride], w);
    }
    y_q4 += y_step_q4;
  }
}

static void scaledconvolve2d(const uint8_t *src, ptrdiff_t src_stride,
                             uint8_t *dst, ptrdiff_t dst_stride,
                             const InterpKernel *const x_filters,
                             int x0_q4, int x_step_q4,
                             const InterpKernel *const y_filters,
                             int y0_q4, int y_step_q4,
                             int w, int h) {
  
  
  
  
  
  
  
  
  
  
  
  
  
  DECLARE_ALIGNED(16, uint8_t, temp[(135 + 8) * 64]);
  const int intermediate_height =
      (((h - 1) * y_step_q4 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

  assert(w <= 64);
  assert(h <= 64);
  assert(y_step_q4 <= 32);
  assert(x_step_q4 <= 32);

  if (w >= 8) {
    scaledconvolve_horiz_w8(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                            src_stride, temp, 64, x_filters, x0_q4, x_step_q4,
                            w, intermediate_height);
  } else {
    scaledconvolve_horiz_w4(src - src_stride * (SUBPEL_TAPS / 2 - 1),
                            src_stride, temp, 64, x_filters, x0_q4, x_step_q4,
                            w, intermediate_height);
  }

  if (w >= 16) {
    scaledconvolve_vert_w16(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                            dst_stride, y_filters, y0_q4, y_step_q4, w, h);
  } else if (w == 8) {
    scaledconvolve_vert_w8(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                           dst_stride, y_filters, y0_q4, y_step_q4, w, h);
  } else {
    scaledconvolve_vert_w4(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst,
                           dst_stride, y_filters, y0_q4, y_step_q4, w, h);
  }
}

static const InterpKernel *get_filter_base(const int16_t *filter) {
  
  
  return (const InterpKernel *)(((intptr_t)filter) & ~((intptr_t)0xFF));
}

static int get_filter_offset(const int16_t *f, const InterpKernel *base) {
  return (int)((const InterpKernel *)(intptr_t)f - base);
}

void vpx_scaled_2d_ssse3(const uint8_t *src, ptrdiff_t src_stride,
                         uint8_t *dst, ptrdiff_t dst_stride,
                         const int16_t *filter_x, int x_step_q4,
                         const int16_t *filter_y, int y_step_q4,
                         int w, int h) {
  const InterpKernel *const filters_x = get_filter_base(filter_x);
  const int x0_q4 = get_filter_offset(filter_x, filters_x);

  const InterpKernel *const filters_y = get_filter_base(filter_y);
  const int y0_q4 = get_filter_offset(filter_y, filters_y);

  scaledconvolve2d(src, src_stride, dst, dst_stride,
                   filters_x, x0_q4, x_step_q4,
                   filters_y, y0_q4, y_step_q4, w, h);
}

FUN_CONV_2D(, ssse3);
FUN_CONV_2D(avg_ , ssse3);

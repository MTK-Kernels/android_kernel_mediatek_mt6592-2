/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>

#include "./vpx_dsp_rtcd.h"
#include "vpx/vpx_integer.h"
#include "vpx_dsp/mips/common_dspr2.h"
#include "vpx_dsp/mips/loopfilter_filters_dspr2.h"
#include "vpx_dsp/mips/loopfilter_macros_dspr2.h"
#include "vpx_dsp/mips/loopfilter_masks_dspr2.h"
#include "vpx_mem/vpx_mem.h"

#if HAVE_DSPR2
void vpx_lpf_vertical_16_dspr2(uint8_t *s,
                               int pitch,
                               const uint8_t *blimit,
                               const uint8_t *limit,
                               const uint8_t *thresh) {
  uint8_t   i;
  uint32_t  mask, hev, flat, flat2;
  uint8_t   *s1, *s2, *s3, *s4;
  uint32_t  prim1, prim2, sec3, sec4, prim3, prim4;
  uint32_t  thresh_vec, flimit_vec, limit_vec;
  uint32_t  uflimit, ulimit, uthresh;
  uint32_t  p7, p6, p5, p4, p3, p2, p1, p0, q0, q1, q2, q3, q4, q5, q6, q7;
  uint32_t  p1_f0, p0_f0, q0_f0, q1_f0;
  uint32_t  p7_l, p6_l, p5_l, p4_l, p3_l, p2_l, p1_l, p0_l;
  uint32_t  q0_l, q1_l, q2_l, q3_l, q4_l, q5_l, q6_l, q7_l;
  uint32_t  p7_r, p6_r, p5_r, p4_r, p3_r, p2_r, p1_r, p0_r;
  uint32_t  q0_r, q1_r, q2_r, q3_r, q4_r, q5_r, q6_r, q7_r;
  uint32_t  p2_l_f1, p1_l_f1, p0_l_f1, p2_r_f1, p1_r_f1, p0_r_f1;
  uint32_t  q0_l_f1, q1_l_f1, q2_l_f1, q0_r_f1, q1_r_f1, q2_r_f1;

  uflimit = *blimit;
  ulimit = *limit;
  uthresh = *thresh;

  
  __asm__ __volatile__ (
      "replv.qb     %[thresh_vec],     %[uthresh]    \n\t"
      "replv.qb     %[flimit_vec],     %[uflimit]    \n\t"
      "replv.qb     %[limit_vec],      %[ulimit]     \n\t"

      : [thresh_vec] "=&r" (thresh_vec), [flimit_vec] "=&r" (flimit_vec),
        [limit_vec] "=r" (limit_vec)
      : [uthresh] "r" (uthresh), [uflimit] "r" (uflimit), [ulimit] "r" (ulimit)
  );

  prefetch_store(s + pitch);

  for (i = 0; i < 2; i++) {
    s1 = s;
    s2 = s + pitch;
    s3 = s2 + pitch;
    s4 = s3 + pitch;
    s  = s4 + pitch;

    __asm__ __volatile__ (
        "lw     %[p0],  -4(%[s1])    \n\t"
        "lw     %[p1],  -4(%[s2])    \n\t"
        "lw     %[p2],  -4(%[s3])    \n\t"
        "lw     %[p3],  -4(%[s4])    \n\t"
        "lw     %[p4],  -8(%[s1])    \n\t"
        "lw     %[p5],  -8(%[s2])    \n\t"
        "lw     %[p6],  -8(%[s3])    \n\t"
        "lw     %[p7],  -8(%[s4])    \n\t"

        : [p3] "=&r" (p3), [p2] "=&r" (p2), [p1] "=&r" (p1),
          [p0] "=&r" (p0), [p7] "=&r" (p7), [p6] "=&r" (p6),
          [p5] "=&r" (p5), [p4] "=&r" (p4)
        : [s1] "r" (s1), [s2] "r" (s2), [s3] "r" (s3), [s4] "r" (s4)
    );

    __asm__ __volatile__ (
        "lw     %[q3],  (%[s1])     \n\t"
        "lw     %[q2],  (%[s2])     \n\t"
        "lw     %[q1],  (%[s3])     \n\t"
        "lw     %[q0],  (%[s4])     \n\t"
        "lw     %[q7],  +4(%[s1])   \n\t"
        "lw     %[q6],  +4(%[s2])   \n\t"
        "lw     %[q5],  +4(%[s3])   \n\t"
        "lw     %[q4],  +4(%[s4])   \n\t"

        : [q3] "=&r" (q3), [q2] "=&r" (q2), [q1] "=&r" (q1),
          [q0] "=&r" (q0), [q7] "=&r" (q7), [q6] "=&r" (q6),
          [q5] "=&r" (q5), [q4] "=&r" (q4)
        : [s1] "r" (s1), [s2] "r" (s2), [s3] "r" (s3), [s4] "r" (s4)
    );

    __asm__ __volatile__ (
        "precrq.qb.ph   %[prim1],   %[p0],      %[p1]       \n\t"
        "precr.qb.ph    %[prim2],   %[p0],      %[p1]       \n\t"
        "precrq.qb.ph   %[prim3],   %[p2],      %[p3]       \n\t"
        "precr.qb.ph    %[prim4],   %[p2],      %[p3]       \n\t"

        "precrq.qb.ph   %[p1],      %[prim1],   %[prim2]    \n\t"
        "precr.qb.ph    %[p3],      %[prim1],   %[prim2]    \n\t"
        "precrq.qb.ph   %[sec3],    %[prim3],   %[prim4]    \n\t"
        "precr.qb.ph    %[sec4],    %[prim3],   %[prim4]    \n\t"

        "precrq.ph.w    %[p0],      %[p1],      %[sec3]     \n\t"
        "precrq.ph.w    %[p2],      %[p3],      %[sec4]     \n\t"
        "append         %[p1],      %[sec3],    16          \n\t"
        "append         %[p3],      %[sec4],    16          \n\t"

        : [prim1] "=&r" (prim1), [prim2] "=&r" (prim2),
          [prim3] "=&r" (prim3), [prim4] "=&r" (prim4),
          [p0] "+r" (p0), [p1] "+r" (p1), [p2] "+r" (p2), [p3] "+r" (p3),
          [sec3] "=&r" (sec3), [sec4] "=&r" (sec4)
        :
    );

    __asm__ __volatile__ (
        "precrq.qb.ph   %[prim1],   %[q3],      %[q2]       \n\t"
        "precr.qb.ph    %[prim2],   %[q3],      %[q2]       \n\t"
        "precrq.qb.ph   %[prim3],   %[q1],      %[q0]       \n\t"
        "precr.qb.ph    %[prim4],   %[q1],      %[q0]       \n\t"

        "precrq.qb.ph   %[q2],      %[prim1],   %[prim2]    \n\t"
        "precr.qb.ph    %[q0],      %[prim1],   %[prim2]    \n\t"
        "precrq.qb.ph   %[sec3],    %[prim3],   %[prim4]    \n\t"
        "precr.qb.ph    %[sec4],    %[prim3],   %[prim4]    \n\t"

        "precrq.ph.w    %[q3],      %[q2],      %[sec3]     \n\t"
        "precrq.ph.w    %[q1],      %[q0],      %[sec4]     \n\t"
        "append         %[q2],      %[sec3],    16          \n\t"
        "append         %[q0],      %[sec4],    16          \n\t"

        : [prim1] "=&r" (prim1), [prim2] "=&r" (prim2),
          [prim3] "=&r" (prim3), [prim4] "=&r" (prim4),
          [q3] "+r" (q3), [q2] "+r" (q2), [q1] "+r" (q1), [q0] "+r" (q0),
          [sec3] "=&r" (sec3), [sec4] "=&r" (sec4)
        :
    );

    __asm__ __volatile__ (
        "precrq.qb.ph   %[prim1],   %[p4],      %[p5]       \n\t"
        "precr.qb.ph    %[prim2],   %[p4],      %[p5]       \n\t"
        "precrq.qb.ph   %[prim3],   %[p6],      %[p7]       \n\t"
        "precr.qb.ph    %[prim4],   %[p6],      %[p7]       \n\t"

        "precrq.qb.ph   %[p5],      %[prim1],   %[prim2]    \n\t"
        "precr.qb.ph    %[p7],      %[prim1],   %[prim2]    \n\t"
        "precrq.qb.ph   %[sec3],    %[prim3],   %[prim4]    \n\t"
        "precr.qb.ph    %[sec4],    %[prim3],   %[prim4]    \n\t"

        "precrq.ph.w    %[p4],      %[p5],      %[sec3]     \n\t"
        "precrq.ph.w    %[p6],      %[p7],      %[sec4]     \n\t"
        "append         %[p5],      %[sec3],    16          \n\t"
        "append         %[p7],      %[sec4],    16          \n\t"

        : [prim1] "=&r" (prim1), [prim2] "=&r" (prim2),
          [prim3] "=&r" (prim3), [prim4] "=&r" (prim4),
          [p4] "+r" (p4), [p5] "+r" (p5), [p6] "+r" (p6), [p7] "+r" (p7),
          [sec3] "=&r" (sec3), [sec4] "=&r" (sec4)
        :
    );

    __asm__ __volatile__ (
        "precrq.qb.ph   %[prim1],   %[q7],      %[q6]       \n\t"
        "precr.qb.ph    %[prim2],   %[q7],      %[q6]       \n\t"
        "precrq.qb.ph   %[prim3],   %[q5],      %[q4]       \n\t"
        "precr.qb.ph    %[prim4],   %[q5],      %[q4]       \n\t"

        "precrq.qb.ph   %[q6],      %[prim1],   %[prim2]    \n\t"
        "precr.qb.ph    %[q4],      %[prim1],   %[prim2]    \n\t"
        "precrq.qb.ph   %[sec3],    %[prim3],   %[prim4]    \n\t"
        "precr.qb.ph    %[sec4],    %[prim3],   %[prim4]    \n\t"

        "precrq.ph.w    %[q7],      %[q6],      %[sec3]     \n\t"
        "precrq.ph.w    %[q5],      %[q4],      %[sec4]     \n\t"
        "append         %[q6],      %[sec3],    16          \n\t"
        "append         %[q4],      %[sec4],    16          \n\t"

        : [prim1] "=&r" (prim1), [prim2] "=&r" (prim2),
          [prim3] "=&r" (prim3), [prim4] "=&r" (prim4),
          [q7] "+r" (q7), [q6] "+r" (q6), [q5] "+r" (q5), [q4] "+r" (q4),
          [sec3] "=&r" (sec3), [sec4] "=&r" (sec4)
        :
    );

    filter_hev_mask_flatmask4_dspr2(limit_vec, flimit_vec, thresh_vec,
                                    p1, p0, p3, p2, q0, q1, q2, q3,
                                    &hev, &mask, &flat);

    flatmask5(p7, p6, p5, p4, p0, q0, q4, q5, q6, q7, &flat2);

    
    if (((flat2 == 0) && (flat == 0) && (mask != 0)) ||
        ((flat2 != 0) && (flat == 0) && (mask != 0))) {
      filter1_dspr2(mask, hev, p1, p0, q0, q1,
                    &p1_f0, &p0_f0, &q0_f0, &q1_f0);
      STORE_F0()
    } else if ((flat2 == 0XFFFFFFFF) && (flat == 0xFFFFFFFF) &&
               (mask == 0xFFFFFFFF)) {
      
      PACK_LEFT_0TO3()
      PACK_LEFT_4TO7()
      wide_mbfilter_dspr2(&p7_l, &p6_l, &p5_l, &p4_l,
                          &p3_l, &p2_l, &p1_l, &p0_l,
                          &q0_l, &q1_l, &q2_l, &q3_l,
                          &q4_l, &q5_l, &q6_l, &q7_l);

      PACK_RIGHT_0TO3()
      PACK_RIGHT_4TO7()
      wide_mbfilter_dspr2(&p7_r, &p6_r, &p5_r, &p4_r,
                          &p3_r, &p2_r, &p1_r, &p0_r,
                          &q0_r, &q1_r, &q2_r, &q3_r,
                          &q4_r, &q5_r, &q6_r, &q7_r);

      STORE_F2()
    } else if ((flat2 == 0) && (flat == 0xFFFFFFFF) && (mask == 0xFFFFFFFF)) {
      
      PACK_LEFT_0TO3()
      mbfilter_dspr2(&p3_l, &p2_l, &p1_l, &p0_l,
                     &q0_l, &q1_l, &q2_l, &q3_l);

      PACK_RIGHT_0TO3()
      mbfilter_dspr2(&p3_r, &p2_r, &p1_r, &p0_r,
                     &q0_r, &q1_r, &q2_r, &q3_r);

      STORE_F1()
    } else if ((flat2 == 0) && (flat != 0) && (mask != 0)) {
      
      filter1_dspr2(mask, hev, p1, p0, q0, q1,
                    &p1_f0, &p0_f0, &q0_f0, &q1_f0);

      
      PACK_LEFT_0TO3()
      mbfilter_dspr2(&p3_l, &p2_l, &p1_l, &p0_l,
                     &q0_l, &q1_l, &q2_l, &q3_l);

      
      PACK_RIGHT_0TO3()
      mbfilter_dspr2(&p3_r, &p2_r, &p1_r, &p0_r,
                     &q0_r, &q1_r, &q2_r, &q3_r);

      if (mask & flat & 0x000000FF) {
        __asm__ __volatile__ (
            "sb     %[p2_r],    -3(%[s4])    \n\t"
            "sb     %[p1_r],    -2(%[s4])    \n\t"
            "sb     %[p0_r],    -1(%[s4])    \n\t"
            "sb     %[q0_r],      (%[s4])    \n\t"
            "sb     %[q1_r],    +1(%[s4])    \n\t"
            "sb     %[q2_r],    +2(%[s4])    \n\t"

            :
            : [p2_r] "r" (p2_r), [p1_r] "r" (p1_r), [p0_r] "r" (p0_r),
              [q0_r] "r" (q0_r), [q1_r] "r" (q1_r), [q2_r] "r" (q2_r),
              [s4] "r" (s4)
        );
      } else if (mask & 0x000000FF) {
        __asm__ __volatile__ (
            "sb         %[p1_f0],  -2(%[s4])    \n\t"
            "sb         %[p0_f0],  -1(%[s4])    \n\t"
            "sb         %[q0_f0],    (%[s4])    \n\t"
            "sb         %[q1_f0],  +1(%[s4])    \n\t"

            :
            : [p1_f0] "r" (p1_f0), [p0_f0] "r" (p0_f0),
              [q0_f0] "r" (q0_f0), [q1_f0] "r" (q1_f0),
              [s4] "r" (s4)
        );
      }

      __asm__ __volatile__ (
          "srl      %[p2_r],    %[p2_r],    16      \n\t"
          "srl      %[p1_r],    %[p1_r],    16      \n\t"
          "srl      %[p0_r],    %[p0_r],    16      \n\t"
          "srl      %[q0_r],    %[q0_r],    16      \n\t"
          "srl      %[q1_r],    %[q1_r],    16      \n\t"
          "srl      %[q2_r],    %[q2_r],    16      \n\t"
          "srl      %[p1_f0],   %[p1_f0],   8       \n\t"
          "srl      %[p0_f0],   %[p0_f0],   8       \n\t"
          "srl      %[q0_f0],   %[q0_f0],   8       \n\t"
          "srl      %[q1_f0],   %[q1_f0],   8       \n\t"

          : [p2_r] "+r" (p2_r), [p1_r] "+r" (p1_r), [p0_r] "+r" (p0_r),
            [q0_r] "+r" (q0_r), [q1_r] "+r" (q1_r), [q2_r] "+r" (q2_r),
            [p1_f0] "+r" (p1_f0), [p0_f0] "+r" (p0_f0),
            [q0_f0] "+r" (q0_f0), [q1_f0] "+r" (q1_f0)
          :
      );

      if (mask & flat & 0x0000FF00) {
        __asm__ __volatile__ (
            "sb     %[p2_r],    -3(%[s3])    \n\t"
            "sb     %[p1_r],    -2(%[s3])    \n\t"
            "sb     %[p0_r],    -1(%[s3])    \n\t"
            "sb     %[q0_r],      (%[s3])    \n\t"
            "sb     %[q1_r],    +1(%[s3])    \n\t"
            "sb     %[q2_r],    +2(%[s3])    \n\t"

            :
            : [p2_r] "r" (p2_r), [p1_r] "r" (p1_r), [p0_r] "r" (p0_r),
              [q0_r] "r" (q0_r), [q1_r] "r" (q1_r), [q2_r] "r" (q2_r),
              [s3] "r" (s3)
        );
      } else if (mask & 0x0000FF00) {
        __asm__ __volatile__ (
            "sb     %[p1_f0],   -2(%[s3])    \n\t"
            "sb     %[p0_f0],   -1(%[s3])    \n\t"
            "sb     %[q0_f0],     (%[s3])    \n\t"
            "sb     %[q1_f0],   +1(%[s3])    \n\t"

            :
            : [p1_f0] "r" (p1_f0), [p0_f0] "r" (p0_f0),
              [q0_f0] "r" (q0_f0), [q1_f0] "r" (q1_f0),
              [s3] "r" (s3)
        );
      }

      __asm__ __volatile__ (
          "srl      %[p1_f0],   %[p1_f0],   8     \n\t"
          "srl      %[p0_f0],   %[p0_f0],   8     \n\t"
          "srl      %[q0_f0],   %[q0_f0],   8     \n\t"
          "srl      %[q1_f0],   %[q1_f0],   8     \n\t"

          : [p1_f0] "+r" (p1_f0), [p0_f0] "+r" (p0_f0),
            [q0_f0] "+r" (q0_f0), [q1_f0] "+r" (q1_f0)
          :
      );

      if (mask & flat & 0x00FF0000) {
        __asm__ __volatile__ (
          "sb       %[p2_l],    -3(%[s2])    \n\t"
          "sb       %[p1_l],    -2(%[s2])    \n\t"
          "sb       %[p0_l],    -1(%[s2])    \n\t"
          "sb       %[q0_l],      (%[s2])    \n\t"
          "sb       %[q1_l],    +1(%[s2])    \n\t"
          "sb       %[q2_l],    +2(%[s2])    \n\t"

          :
          : [p2_l] "r" (p2_l), [p1_l] "r" (p1_l), [p0_l] "r" (p0_l),
            [q0_l] "r" (q0_l), [q1_l] "r" (q1_l), [q2_l] "r" (q2_l),
            [s2] "r" (s2)
        );
      } else if (mask & 0x00FF0000) {
        __asm__ __volatile__ (
            "sb     %[p1_f0],   -2(%[s2])    \n\t"
            "sb     %[p0_f0],   -1(%[s2])    \n\t"
            "sb     %[q0_f0],     (%[s2])    \n\t"
            "sb     %[q1_f0],   +1(%[s2])    \n\t"

            :
            : [p1_f0] "r" (p1_f0), [p0_f0] "r" (p0_f0),
              [q0_f0] "r" (q0_f0), [q1_f0] "r" (q1_f0),
              [s2] "r" (s2)
        );
      }

      __asm__ __volatile__ (
          "srl      %[p2_l],    %[p2_l],    16      \n\t"
          "srl      %[p1_l],    %[p1_l],    16      \n\t"
          "srl      %[p0_l],    %[p0_l],    16      \n\t"
          "srl      %[q0_l],    %[q0_l],    16      \n\t"
          "srl      %[q1_l],    %[q1_l],    16      \n\t"
          "srl      %[q2_l],    %[q2_l],    16      \n\t"
          "srl      %[p1_f0],   %[p1_f0],   8       \n\t"
          "srl      %[p0_f0],   %[p0_f0],   8       \n\t"
          "srl      %[q0_f0],   %[q0_f0],   8       \n\t"
          "srl      %[q1_f0],   %[q1_f0],   8       \n\t"

          : [p2_l] "+r" (p2_l), [p1_l] "+r" (p1_l), [p0_l] "+r" (p0_l),
            [q0_l] "+r" (q0_l), [q1_l] "+r" (q1_l), [q2_l] "+r" (q2_l),
            [p1_f0] "+r" (p1_f0), [p0_f0] "+r" (p0_f0),
            [q0_f0] "+r" (q0_f0), [q1_f0] "+r" (q1_f0)
          :
      );

      if (mask & flat & 0xFF000000) {
        __asm__ __volatile__ (
            "sb     %[p2_l],    -3(%[s1])    \n\t"
            "sb     %[p1_l],    -2(%[s1])    \n\t"
            "sb     %[p0_l],    -1(%[s1])    \n\t"
            "sb     %[q0_l],      (%[s1])    \n\t"
            "sb     %[q1_l],    +1(%[s1])    \n\t"
            "sb     %[q2_l],    +2(%[s1])    \n\t"

            :
            : [p2_l] "r" (p2_l), [p1_l] "r" (p1_l), [p0_l] "r" (p0_l),
              [q0_l] "r" (q0_l), [q1_l] "r" (q1_l), [q2_l] "r" (q2_l),
              [s1] "r" (s1)
        );
      } else if (mask & 0xFF000000) {
        __asm__ __volatile__ (
            "sb     %[p1_f0],   -2(%[s1])    \n\t"
            "sb     %[p0_f0],   -1(%[s1])    \n\t"
            "sb     %[q0_f0],     (%[s1])    \n\t"
            "sb     %[q1_f0],   +1(%[s1])    \n\t"

            :
            : [p1_f0] "r" (p1_f0), [p0_f0] "r" (p0_f0),
              [q0_f0] "r" (q0_f0), [q1_f0] "r" (q1_f0),
              [s1] "r" (s1)
        );
      }
    } else if ((flat2 != 0) && (flat != 0) && (mask != 0)) {
      
      filter1_dspr2(mask, hev, p1, p0, q0, q1,
                    &p1_f0, &p0_f0, &q0_f0, &q1_f0);

      PACK_LEFT_0TO3()
      mbfilter1_dspr2(p3_l, p2_l, p1_l, p0_l,
                      q0_l, q1_l, q2_l, q3_l,
                      &p2_l_f1, &p1_l_f1, &p0_l_f1,
                      &q0_l_f1, &q1_l_f1, &q2_l_f1);

      PACK_RIGHT_0TO3()
      mbfilter1_dspr2(p3_r, p2_r, p1_r, p0_r,
                      q0_r, q1_r, q2_r, q3_r,
                      &p2_r_f1, &p1_r_f1, &p0_r_f1,
                      &q0_r_f1, &q1_r_f1, &q2_r_f1);

      PACK_LEFT_4TO7()
      wide_mbfilter_dspr2(&p7_l, &p6_l, &p5_l, &p4_l,
                          &p3_l, &p2_l, &p1_l, &p0_l,
                          &q0_l, &q1_l, &q2_l, &q3_l,
                          &q4_l, &q5_l, &q6_l, &q7_l);

      PACK_RIGHT_4TO7()
      wide_mbfilter_dspr2(&p7_r, &p6_r, &p5_r, &p4_r,
                          &p3_r, &p2_r, &p1_r, &p0_r,
                          &q0_r, &q1_r, &q2_r, &q3_r,
                          &q4_r, &q5_r, &q6_r, &q7_r);

      if (mask & flat & flat2 & 0x000000FF) {
        __asm__ __volatile__ (
            "sb     %[p6_r],    -7(%[s4])    \n\t"
            "sb     %[p5_r],    -6(%[s4])    \n\t"
            "sb     %[p4_r],    -5(%[s4])    \n\t"
            "sb     %[p3_r],    -4(%[s4])    \n\t"
            "sb     %[p2_r],    -3(%[s4])    \n\t"
            "sb     %[p1_r],    -2(%[s4])    \n\t"
            "sb     %[p0_r],    -1(%[s4])    \n\t"

            :
            : [p6_r] "r" (p6_r), [p5_r] "r" (p5_r),
              [p4_r] "r" (p4_r), [p3_r] "r" (p3_r),
              [p2_r] "r" (p2_r), [p1_r] "r" (p1_r),
              [p0_r] "r" (p0_r), [s4] "r" (s4)
        );

        __asm__ __volatile__ (
            "sb     %[q0_r],      (%[s4])    \n\t"
            "sb     %[q1_r],    +1(%[s4])    \n\t"
            "sb     %[q2_r],    +2(%[s4])    \n\t"
            "sb     %[q3_r],    +3(%[s4])    \n\t"
            "sb     %[q4_r],    +4(%[s4])    \n\t"
            "sb     %[q5_r],    +5(%[s4])    \n\t"
            "sb     %[q6_r],    +6(%[s4])    \n\t"

            :
            : [q0_r] "r" (q0_r), [q1_r] "r" (q1_r),
              [q2_r] "r" (q2_r), [q3_r] "r" (q3_r),
              [q4_r] "r" (q4_r), [q5_r] "r" (q5_r),
              [q6_r] "r" (q6_r), [s4] "r" (s4)
        );
      } else if (mask & flat & 0x000000FF) {
        __asm__ __volatile__ (
            "sb     %[p2_r_f1],     -3(%[s4])    \n\t"
            "sb     %[p1_r_f1],     -2(%[s4])    \n\t"
            "sb     %[p0_r_f1],     -1(%[s4])    \n\t"
            "sb     %[q0_r_f1],       (%[s4])    \n\t"
            "sb     %[q1_r_f1],     +1(%[s4])    \n\t"
            "sb     %[q2_r_f1],     +2(%[s4])    \n\t"

            :
            : [p2_r_f1] "r" (p2_r_f1), [p1_r_f1] "r" (p1_r_f1),
              [p0_r_f1] "r" (p0_r_f1), [q0_r_f1] "r" (q0_r_f1),
              [q1_r_f1] "r" (q1_r_f1), [q2_r_f1] "r" (q2_r_f1),
              [s4] "r" (s4)
        );
      } else if (mask & 0x000000FF) {
        __asm__ __volatile__ (
            "sb     %[p1_f0],   -2(%[s4])    \n\t"
            "sb     %[p0_f0],   -1(%[s4])    \n\t"
            "sb     %[q0_f0],     (%[s4])    \n\t"
            "sb     %[q1_f0],   +1(%[s4])    \n\t"

            :
            : [p1_f0] "r" (p1_f0), [p0_f0] "r" (p0_f0),
              [q0_f0] "r" (q0_f0), [q1_f0] "r" (q1_f0),
              [s4] "r" (s4)
        );
      }

      __asm__ __volatile__ (
          "srl      %[p6_r],        %[p6_r],        16     \n\t"
          "srl      %[p5_r],        %[p5_r],        16     \n\t"
          "srl      %[p4_r],        %[p4_r],        16     \n\t"
          "srl      %[p3_r],        %[p3_r],        16     \n\t"
          "srl      %[p2_r],        %[p2_r],        16     \n\t"
          "srl      %[p1_r],        %[p1_r],        16     \n\t"
          "srl      %[p0_r],        %[p0_r],        16     \n\t"
          "srl      %[q0_r],        %[q0_r],        16     \n\t"
          "srl      %[q1_r],        %[q1_r],        16     \n\t"
          "srl      %[q2_r],        %[q2_r],        16     \n\t"
          "srl      %[q3_r],        %[q3_r],        16     \n\t"
          "srl      %[q4_r],        %[q4_r],        16     \n\t"
          "srl      %[q5_r],        %[q5_r],        16     \n\t"
          "srl      %[q6_r],        %[q6_r],        16     \n\t"

          : [q0_r] "+r" (q0_r), [q1_r] "+r" (q1_r),
            [q2_r] "+r" (q2_r), [q3_r] "+r" (q3_r),
            [q4_r] "+r" (q4_r), [q5_r] "+r" (q5_r),
            [q6_r] "+r" (q6_r), [p6_r] "+r" (p6_r),
            [p5_r] "+r" (p5_r), [p4_r] "+r" (p4_r),
            [p3_r] "+r" (p3_r), [p2_r] "+r" (p2_r),
            [p1_r] "+r" (p1_r), [p0_r] "+r" (p0_r)
          :
      );

      __asm__ __volatile__ (
          "srl      %[p2_r_f1],     %[p2_r_f1],     16      \n\t"
          "srl      %[p1_r_f1],     %[p1_r_f1],     16      \n\t"
          "srl      %[p0_r_f1],     %[p0_r_f1],     16      \n\t"
          "srl      %[q0_r_f1],     %[q0_r_f1],     16      \n\t"
          "srl      %[q1_r_f1],     %[q1_r_f1],     16      \n\t"
          "srl      %[q2_r_f1],     %[q2_r_f1],     16      \n\t"
          "srl      %[p1_f0],       %[p1_f0],       8       \n\t"
          "srl      %[p0_f0],       %[p0_f0],       8       \n\t"
          "srl      %[q0_f0],       %[q0_f0],       8       \n\t"
          "srl      %[q1_f0],       %[q1_f0],       8       \n\t"

          : [p2_r_f1] "+r" (p2_r_f1), [p1_r_f1] "+r" (p1_r_f1),
            [p0_r_f1] "+r" (p0_r_f1), [q0_r_f1] "+r" (q0_r_f1),
            [q1_r_f1] "+r" (q1_r_f1), [q2_r_f1] "+r" (q2_r_f1),
            [p1_f0] "+r" (p1_f0), [p0_f0] "+r" (p0_f0),
            [q0_f0] "+r" (q0_f0), [q1_f0] "+r" (q1_f0)
          :
      );

      if (mask & flat & flat2 & 0x0000FF00) {
        __asm__ __volatile__ (
            "sb     %[p6_r],    -7(%[s3])    \n\t"
            "sb     %[p5_r],    -6(%[s3])    \n\t"
            "sb     %[p4_r],    -5(%[s3])    \n\t"
            "sb     %[p3_r],    -4(%[s3])    \n\t"
            "sb     %[p2_r],    -3(%[s3])    \n\t"
            "sb     %[p1_r],    -2(%[s3])    \n\t"
            "sb     %[p0_r],    -1(%[s3])    \n\t"

            :
            : [p6_r] "r" (p6_r), [p5_r] "r" (p5_r), [p4_r] "r" (p4_r),
              [p3_r] "r" (p3_r), [p2_r] "r" (p2_r), [p1_r] "r" (p1_r),
              [p0_r] "r" (p0_r), [s3] "r" (s3)
        );

        __asm__ __volatile__ (
            "sb     %[q0_r],      (%[s3])    \n\t"
            "sb     %[q1_r],    +1(%[s3])    \n\t"
            "sb     %[q2_r],    +2(%[s3])    \n\t"
            "sb     %[q3_r],    +3(%[s3])    \n\t"
            "sb     %[q4_r],    +4(%[s3])    \n\t"
            "sb     %[q5_r],    +5(%[s3])    \n\t"
            "sb     %[q6_r],    +6(%[s3])    \n\t"

            :
            : [q0_r] "r" (q0_r), [q1_r] "r" (q1_r),
              [q2_r] "r" (q2_r), [q3_r] "r" (q3_r),
              [q4_r] "r" (q4_r), [q5_r] "r" (q5_r),
              [q6_r] "r" (q6_r), [s3] "r" (s3)
        );
      } else if (mask & flat & 0x0000FF00) {
        __asm__ __volatile__ (
            "sb     %[p2_r_f1],     -3(%[s3])    \n\t"
            "sb     %[p1_r_f1],     -2(%[s3])    \n\t"
            "sb     %[p0_r_f1],     -1(%[s3])    \n\t"
            "sb     %[q0_r_f1],       (%[s3])    \n\t"
            "sb     %[q1_r_f1],     +1(%[s3])    \n\t"
            "sb     %[q2_r_f1],     +2(%[s3])    \n\t"

            :
            : [p2_r_f1] "r" (p2_r_f1), [p1_r_f1] "r" (p1_r_f1),
              [p0_r_f1] "r" (p0_r_f1), [q0_r_f1] "r" (q0_r_f1),
              [q1_r_f1] "r" (q1_r_f1), [q2_r_f1] "r" (q2_r_f1),
              [s3] "r" (s3)
        );
      } else if (mask & 0x0000FF00) {
        __asm__ __volatile__ (
            "sb     %[p1_f0],   -2(%[s3])    \n\t"
            "sb     %[p0_f0],   -1(%[s3])    \n\t"
            "sb     %[q0_f0],     (%[s3])    \n\t"
            "sb     %[q1_f0],   +1(%[s3])    \n\t"

            :
            : [p1_f0] "r" (p1_f0), [p0_f0] "r" (p0_f0),
              [q0_f0] "r" (q0_f0), [q1_f0] "r" (q1_f0),
              [s3] "r" (s3)
        );
      }

      __asm__ __volatile__ (
          "srl      %[p1_f0],   %[p1_f0],   8     \n\t"
          "srl      %[p0_f0],   %[p0_f0],   8     \n\t"
          "srl      %[q0_f0],   %[q0_f0],   8     \n\t"
          "srl      %[q1_f0],   %[q1_f0],   8     \n\t"

          : [p1_f0] "+r" (p1_f0), [p0_f0] "+r" (p0_f0),
            [q0_f0] "+r" (q0_f0), [q1_f0] "+r" (q1_f0)
          :
      );

      if (mask & flat & flat2 & 0x00FF0000) {
        __asm__ __volatile__ (
            "sb     %[p6_l],    -7(%[s2])    \n\t"
            "sb     %[p5_l],    -6(%[s2])    \n\t"
            "sb     %[p4_l],    -5(%[s2])    \n\t"
            "sb     %[p3_l],    -4(%[s2])    \n\t"
            "sb     %[p2_l],    -3(%[s2])    \n\t"
            "sb     %[p1_l],    -2(%[s2])    \n\t"
            "sb     %[p0_l],    -1(%[s2])    \n\t"

            :
            : [p6_l] "r" (p6_l), [p5_l] "r" (p5_l), [p4_l] "r" (p4_l),
              [p3_l] "r" (p3_l), [p2_l] "r" (p2_l), [p1_l] "r" (p1_l),
              [p0_l] "r" (p0_l), [s2] "r" (s2)
        );

        __asm__ __volatile__ (
            "sb     %[q0_l],      (%[s2])    \n\t"
            "sb     %[q1_l],    +1(%[s2])    \n\t"
            "sb     %[q2_l],    +2(%[s2])    \n\t"
            "sb     %[q3_l],    +3(%[s2])    \n\t"
            "sb     %[q4_l],    +4(%[s2])    \n\t"
            "sb     %[q5_l],    +5(%[s2])    \n\t"
            "sb     %[q6_l],    +6(%[s2])    \n\t"

            :
            : [q0_l] "r" (q0_l), [q1_l] "r" (q1_l), [q2_l] "r" (q2_l),
              [q3_l] "r" (q3_l), [q4_l] "r" (q4_l), [q5_l] "r" (q5_l),
              [q6_l] "r" (q6_l), [s2] "r" (s2)
        );
      } else if (mask & flat & 0x00FF0000) {
        __asm__ __volatile__ (
            "sb     %[p2_l_f1],     -3(%[s2])    \n\t"
            "sb     %[p1_l_f1],     -2(%[s2])    \n\t"
            "sb     %[p0_l_f1],     -1(%[s2])    \n\t"
            "sb     %[q0_l_f1],       (%[s2])    \n\t"
            "sb     %[q1_l_f1],     +1(%[s2])    \n\t"
            "sb     %[q2_l_f1],     +2(%[s2])    \n\t"

            :
            : [p2_l_f1] "r" (p2_l_f1), [p1_l_f1] "r" (p1_l_f1),
              [p0_l_f1] "r" (p0_l_f1), [q0_l_f1] "r" (q0_l_f1),
              [q1_l_f1] "r" (q1_l_f1), [q2_l_f1] "r" (q2_l_f1),
              [s2] "r" (s2)
        );
      } else if (mask & 0x00FF0000) {
        __asm__ __volatile__ (
            "sb     %[p1_f0],   -2(%[s2])    \n\t"
            "sb     %[p0_f0],   -1(%[s2])    \n\t"
            "sb     %[q0_f0],     (%[s2])    \n\t"
            "sb     %[q1_f0],   +1(%[s2])    \n\t"

            :
            : [p1_f0] "r" (p1_f0), [p0_f0] "r" (p0_f0),
              [q0_f0] "r" (q0_f0), [q1_f0] "r" (q1_f0),
              [s2] "r" (s2)
        );
      }

      __asm__ __volatile__ (
          "srl      %[p6_l],        %[p6_l],        16     \n\t"
          "srl      %[p5_l],        %[p5_l],        16     \n\t"
          "srl      %[p4_l],        %[p4_l],        16     \n\t"
          "srl      %[p3_l],        %[p3_l],        16     \n\t"
          "srl      %[p2_l],        %[p2_l],        16     \n\t"
          "srl      %[p1_l],        %[p1_l],        16     \n\t"
          "srl      %[p0_l],        %[p0_l],        16     \n\t"
          "srl      %[q0_l],        %[q0_l],        16     \n\t"
          "srl      %[q1_l],        %[q1_l],        16     \n\t"
          "srl      %[q2_l],        %[q2_l],        16     \n\t"
          "srl      %[q3_l],        %[q3_l],        16     \n\t"
          "srl      %[q4_l],        %[q4_l],        16     \n\t"
          "srl      %[q5_l],        %[q5_l],        16     \n\t"
          "srl      %[q6_l],        %[q6_l],        16     \n\t"

          : [q0_l] "+r" (q0_l), [q1_l] "+r" (q1_l), [q2_l] "+r" (q2_l),
            [q3_l] "+r" (q3_l), [q4_l] "+r" (q4_l), [q5_l] "+r" (q5_l),
            [q6_l] "+r" (q6_l), [p6_l] "+r" (p6_l), [p5_l] "+r" (p5_l),
            [p4_l] "+r" (p4_l), [p3_l] "+r" (p3_l), [p2_l] "+r" (p2_l),
            [p1_l] "+r" (p1_l), [p0_l] "+r" (p0_l)
          :
      );

      __asm__ __volatile__ (
          "srl      %[p2_l_f1],     %[p2_l_f1],     16      \n\t"
          "srl      %[p1_l_f1],     %[p1_l_f1],     16      \n\t"
          "srl      %[p0_l_f1],     %[p0_l_f1],     16      \n\t"
          "srl      %[q0_l_f1],     %[q0_l_f1],     16      \n\t"
          "srl      %[q1_l_f1],     %[q1_l_f1],     16      \n\t"
          "srl      %[q2_l_f1],     %[q2_l_f1],     16      \n\t"
          "srl      %[p1_f0],       %[p1_f0],       8       \n\t"
          "srl      %[p0_f0],       %[p0_f0],       8       \n\t"
          "srl      %[q0_f0],       %[q0_f0],       8       \n\t"
          "srl      %[q1_f0],       %[q1_f0],       8       \n\t"

          : [p2_l_f1] "+r" (p2_l_f1), [p1_l_f1] "+r" (p1_l_f1),
            [p0_l_f1] "+r" (p0_l_f1), [q0_l_f1] "+r" (q0_l_f1),
            [q1_l_f1] "+r" (q1_l_f1), [q2_l_f1] "+r" (q2_l_f1),
            [p1_f0] "+r" (p1_f0), [p0_f0] "+r" (p0_f0),
            [q0_f0] "+r" (q0_f0), [q1_f0] "+r" (q1_f0)
          :
      );

      if (mask & flat & flat2 & 0xFF000000) {
        __asm__ __volatile__ (
            "sb     %[p6_l],    -7(%[s1])    \n\t"
            "sb     %[p5_l],    -6(%[s1])    \n\t"
            "sb     %[p4_l],    -5(%[s1])    \n\t"
            "sb     %[p3_l],    -4(%[s1])    \n\t"
            "sb     %[p2_l],    -3(%[s1])    \n\t"
            "sb     %[p1_l],    -2(%[s1])    \n\t"
            "sb     %[p0_l],    -1(%[s1])    \n\t"

            :
            : [p6_l] "r" (p6_l), [p5_l] "r" (p5_l), [p4_l] "r" (p4_l),
              [p3_l] "r" (p3_l), [p2_l] "r" (p2_l), [p1_l] "r" (p1_l),
              [p0_l] "r" (p0_l),
              [s1] "r" (s1)
        );

        __asm__ __volatile__ (
            "sb     %[q0_l],     (%[s1])    \n\t"
            "sb     %[q1_l],    1(%[s1])    \n\t"
            "sb     %[q2_l],    2(%[s1])    \n\t"
            "sb     %[q3_l],    3(%[s1])    \n\t"
            "sb     %[q4_l],    4(%[s1])    \n\t"
            "sb     %[q5_l],    5(%[s1])    \n\t"
            "sb     %[q6_l],    6(%[s1])    \n\t"

            :
            : [q0_l] "r" (q0_l), [q1_l] "r" (q1_l), [q2_l] "r" (q2_l),
              [q3_l] "r" (q3_l), [q4_l] "r" (q4_l), [q5_l] "r" (q5_l),
              [q6_l] "r" (q6_l),
              [s1] "r" (s1)
        );
      } else if (mask & flat & 0xFF000000) {
        __asm__ __volatile__ (
            "sb     %[p2_l_f1],     -3(%[s1])    \n\t"
            "sb     %[p1_l_f1],     -2(%[s1])    \n\t"
            "sb     %[p0_l_f1],     -1(%[s1])    \n\t"
            "sb     %[q0_l_f1],       (%[s1])    \n\t"
            "sb     %[q1_l_f1],     +1(%[s1])    \n\t"
            "sb     %[q2_l_f1],     +2(%[s1])    \n\t"

            :
            : [p2_l_f1] "r" (p2_l_f1), [p1_l_f1] "r" (p1_l_f1),
              [p0_l_f1] "r" (p0_l_f1), [q0_l_f1] "r" (q0_l_f1),
              [q1_l_f1] "r" (q1_l_f1), [q2_l_f1] "r" (q2_l_f1),
              [s1] "r" (s1)
        );
      } else if (mask & 0xFF000000) {
        __asm__ __volatile__ (
            "sb     %[p1_f0],   -2(%[s1])    \n\t"
            "sb     %[p0_f0],   -1(%[s1])    \n\t"
            "sb     %[q0_f0],     (%[s1])    \n\t"
            "sb     %[q1_f0],   +1(%[s1])    \n\t"

            :
            : [p1_f0] "r" (p1_f0), [p0_f0] "r" (p0_f0),
              [q0_f0] "r" (q0_f0), [q1_f0] "r" (q1_f0),
              [s1] "r" (s1)
        );
      }
    }
  }
}
#endif  

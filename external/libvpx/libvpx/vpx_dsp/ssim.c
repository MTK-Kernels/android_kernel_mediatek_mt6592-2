/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include "./vpx_dsp_rtcd.h"
#include "vpx_dsp/ssim.h"
#include "vpx_ports/mem.h"
#include "vpx_ports/system_state.h"

void vpx_ssim_parms_16x16_c(const uint8_t *s, int sp, const uint8_t *r,
                            int rp, uint32_t *sum_s, uint32_t *sum_r,
                            uint32_t *sum_sq_s, uint32_t *sum_sq_r,
                            uint32_t *sum_sxr) {
  int i, j;
  for (i = 0; i < 16; i++, s += sp, r += rp) {
    for (j = 0; j < 16; j++) {
      *sum_s += s[j];
      *sum_r += r[j];
      *sum_sq_s += s[j] * s[j];
      *sum_sq_r += r[j] * r[j];
      *sum_sxr += s[j] * r[j];
    }
  }
}
void vpx_ssim_parms_8x8_c(const uint8_t *s, int sp, const uint8_t *r, int rp,
                          uint32_t *sum_s, uint32_t *sum_r,
                          uint32_t *sum_sq_s, uint32_t *sum_sq_r,
                          uint32_t *sum_sxr) {
  int i, j;
  for (i = 0; i < 8; i++, s += sp, r += rp) {
    for (j = 0; j < 8; j++) {
      *sum_s += s[j];
      *sum_r += r[j];
      *sum_sq_s += s[j] * s[j];
      *sum_sq_r += r[j] * r[j];
      *sum_sxr += s[j] * r[j];
    }
  }
}

#if CONFIG_VP9_HIGHBITDEPTH
void vpx_highbd_ssim_parms_8x8_c(const uint16_t *s, int sp,
                                 const uint16_t *r, int rp,
                                 uint32_t *sum_s, uint32_t *sum_r,
                                 uint32_t *sum_sq_s, uint32_t *sum_sq_r,
                                 uint32_t *sum_sxr) {
  int i, j;
  for (i = 0; i < 8; i++, s += sp, r += rp) {
    for (j = 0; j < 8; j++) {
      *sum_s += s[j];
      *sum_r += r[j];
      *sum_sq_s += s[j] * s[j];
      *sum_sq_r += r[j] * r[j];
      *sum_sxr += s[j] * r[j];
    }
  }
}
#endif  

static const int64_t cc1 =  26634;  
static const int64_t cc2 = 239708;  

static double similarity(uint32_t sum_s, uint32_t sum_r,
                         uint32_t sum_sq_s, uint32_t sum_sq_r,
                         uint32_t sum_sxr, int count) {
  int64_t ssim_n, ssim_d;
  int64_t c1, c2;

  
  c1 = (cc1 * count * count) >> 12;
  c2 = (cc2 * count * count) >> 12;

  ssim_n = (2 * sum_s * sum_r + c1) * ((int64_t) 2 * count * sum_sxr -
                                       (int64_t) 2 * sum_s * sum_r + c2);

  ssim_d = (sum_s * sum_s + sum_r * sum_r + c1) *
           ((int64_t)count * sum_sq_s - (int64_t)sum_s * sum_s +
            (int64_t)count * sum_sq_r - (int64_t) sum_r * sum_r + c2);

  return ssim_n * 1.0 / ssim_d;
}

static double ssim_8x8(const uint8_t *s, int sp, const uint8_t *r, int rp) {
  uint32_t sum_s = 0, sum_r = 0, sum_sq_s = 0, sum_sq_r = 0, sum_sxr = 0;
  vpx_ssim_parms_8x8(s, sp, r, rp, &sum_s, &sum_r, &sum_sq_s, &sum_sq_r,
                     &sum_sxr);
  return similarity(sum_s, sum_r, sum_sq_s, sum_sq_r, sum_sxr, 64);
}

#if CONFIG_VP9_HIGHBITDEPTH
static double highbd_ssim_8x8(const uint16_t *s, int sp, const uint16_t *r,
                              int rp, unsigned int bd) {
  uint32_t sum_s = 0, sum_r = 0, sum_sq_s = 0, sum_sq_r = 0, sum_sxr = 0;
  const int oshift = bd - 8;
  vpx_highbd_ssim_parms_8x8(s, sp, r, rp, &sum_s, &sum_r, &sum_sq_s, &sum_sq_r,
                            &sum_sxr);
  return similarity(sum_s >> oshift,
                    sum_r >> oshift,
                    sum_sq_s >> (2 * oshift),
                    sum_sq_r >> (2 * oshift),
                    sum_sxr >> (2 * oshift),
                    64);
}
#endif  

static double vpx_ssim2(const uint8_t *img1, const uint8_t *img2,
                        int stride_img1, int stride_img2, int width,
                        int height) {
  int i, j;
  int samples = 0;
  double ssim_total = 0;

  
  for (i = 0; i <= height - 8;
       i += 4, img1 += stride_img1 * 4, img2 += stride_img2 * 4) {
    for (j = 0; j <= width - 8; j += 4) {
      double v = ssim_8x8(img1 + j, stride_img1, img2 + j, stride_img2);
      ssim_total += v;
      samples++;
    }
  }
  ssim_total /= samples;
  return ssim_total;
}

#if CONFIG_VP9_HIGHBITDEPTH
static double vpx_highbd_ssim2(const uint8_t *img1, const uint8_t *img2,
                               int stride_img1, int stride_img2, int width,
                               int height, unsigned int bd) {
  int i, j;
  int samples = 0;
  double ssim_total = 0;

  
  for (i = 0; i <= height - 8;
       i += 4, img1 += stride_img1 * 4, img2 += stride_img2 * 4) {
    for (j = 0; j <= width - 8; j += 4) {
      double v = highbd_ssim_8x8(CONVERT_TO_SHORTPTR(img1 + j), stride_img1,
                                 CONVERT_TO_SHORTPTR(img2 + j), stride_img2,
                                 bd);
      ssim_total += v;
      samples++;
    }
  }
  ssim_total /= samples;
  return ssim_total;
}
#endif  

double vpx_calc_ssim(const YV12_BUFFER_CONFIG *source,
                     const YV12_BUFFER_CONFIG *dest,
                     double *weight) {
  double a, b, c;
  double ssimv;

  a = vpx_ssim2(source->y_buffer, dest->y_buffer,
                source->y_stride, dest->y_stride,
                source->y_crop_width, source->y_crop_height);

  b = vpx_ssim2(source->u_buffer, dest->u_buffer,
                source->uv_stride, dest->uv_stride,
                source->uv_crop_width, source->uv_crop_height);

  c = vpx_ssim2(source->v_buffer, dest->v_buffer,
                source->uv_stride, dest->uv_stride,
                source->uv_crop_width, source->uv_crop_height);

  ssimv = a * .8 + .1 * (b + c);

  *weight = 1;

  return ssimv;
}

double vpx_calc_ssimg(const YV12_BUFFER_CONFIG *source,
                      const YV12_BUFFER_CONFIG *dest,
                      double *ssim_y, double *ssim_u, double *ssim_v) {
  double ssim_all = 0;
  double a, b, c;

  a = vpx_ssim2(source->y_buffer, dest->y_buffer,
                source->y_stride, dest->y_stride,
                source->y_crop_width, source->y_crop_height);

  b = vpx_ssim2(source->u_buffer, dest->u_buffer,
                source->uv_stride, dest->uv_stride,
                source->uv_crop_width, source->uv_crop_height);

  c = vpx_ssim2(source->v_buffer, dest->v_buffer,
                source->uv_stride, dest->uv_stride,
                source->uv_crop_width, source->uv_crop_height);
  *ssim_y = a;
  *ssim_u = b;
  *ssim_v = c;
  ssim_all = (a * 4 + b + c) / 6;

  return ssim_all;
}


static double ssimv_similarity(const Ssimv *sv, int64_t n) {
  
  const int64_t c1 = (cc1 * n * n) >> 12;
  const int64_t c2 = (cc2 * n * n) >> 12;

  const double l = 1.0 * (2 * sv->sum_s * sv->sum_r + c1) /
      (sv->sum_s * sv->sum_s + sv->sum_r * sv->sum_r + c1);

  
  
  const double v = (2.0 * n * sv->sum_sxr - 2 * sv->sum_s * sv->sum_r + c2)
      / (n * sv->sum_sq_s - sv->sum_s * sv->sum_s + n * sv->sum_sq_r
         - sv->sum_r * sv->sum_r + c2);

  return l * v;
}

static double ssimv_similarity2(const Ssimv *sv, int64_t n) {
  
  const int64_t c1 = (cc1 * n * n) >> 12;
  const int64_t c2 = (cc2 * n * n) >> 12;

  const double mean_diff = (1.0 * sv->sum_s - sv->sum_r) / n;
  const double l = (255 * 255 - mean_diff * mean_diff + c1) / (255 * 255 + c1);

  
  
  const double v = (2.0 * n * sv->sum_sxr - 2 * sv->sum_s * sv->sum_r + c2)
      / (n * sv->sum_sq_s - sv->sum_s * sv->sum_s +
         n * sv->sum_sq_r - sv->sum_r * sv->sum_r + c2);

  return l * v;
}
static void ssimv_parms(uint8_t *img1, int img1_pitch, uint8_t *img2,
                        int img2_pitch, Ssimv *sv) {
  vpx_ssim_parms_8x8(img1, img1_pitch, img2, img2_pitch,
                     &sv->sum_s, &sv->sum_r, &sv->sum_sq_s, &sv->sum_sq_r,
                     &sv->sum_sxr);
}

double vpx_get_ssim_metrics(uint8_t *img1, int img1_pitch,
                            uint8_t *img2, int img2_pitch,
                            int width, int height,
                            Ssimv *sv2, Metrics *m,
                            int do_inconsistency) {
  double dssim_total = 0;
  double ssim_total = 0;
  double ssim2_total = 0;
  double inconsistency_total = 0;
  int i, j;
  int c = 0;
  double norm;
  double old_ssim_total = 0;
  vpx_clear_system_state();
  
  for (i = 0; i < height; i += 4,
       img1 += img1_pitch * 4, img2 += img2_pitch * 4) {
    for (j = 0; j < width; j += 4, ++c) {
      Ssimv sv = {0};
      double ssim;
      double ssim2;
      double dssim;
      uint32_t var_new;
      uint32_t var_old;
      uint32_t mean_new;
      uint32_t mean_old;
      double ssim_new;
      double ssim_old;

      
      
      
      
      if (j + 8 <= width && i + 8 <= height) {
        ssimv_parms(img1 + j, img1_pitch, img2 + j, img2_pitch, &sv);
      }

      ssim = ssimv_similarity(&sv, 64);
      ssim2 = ssimv_similarity2(&sv, 64);

      sv.ssim = ssim2;

      
      
      
      
      dssim = 255 * 255 * (1 - ssim2) / 2;

      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      
      var_new = sv.sum_sq_s - sv.sum_s * sv.sum_s / 64;
      var_old = sv2[c].sum_sq_s - sv2[c].sum_s * sv2[c].sum_s / 64;
      mean_new = sv.sum_s;
      mean_old = sv2[c].sum_s;
      ssim_new = sv.ssim;
      ssim_old = sv2[c].ssim;

      if (do_inconsistency) {
        
        
        
        static const double kScaling = 4. * 4 * 255 * 255;

        
        
        
        
        static const double c1 = 1, c2 = 1, c3 = 1;

        
        
        const double variance_term = (2.0 * var_old * var_new + c1) /
            (1.0 * var_old * var_old + 1.0 * var_new * var_new + c1);

        
        
        const double mean_term = (2.0 * mean_old * mean_new + c2) /
            (1.0 * mean_old * mean_old + 1.0 * mean_new * mean_new + c2);

        
        
        double ssim_term = pow((2.0 * ssim_old * ssim_new + c3) /
                               (ssim_old * ssim_old + ssim_new * ssim_new + c3),
                               5);

        double this_inconsistency;

        
        
        
        if (ssim_term > 1)
          ssim_term = 1;

        
        
        
        
        
        
        this_inconsistency = (1 - ssim_term) * variance_term * mean_term;

        this_inconsistency *= kScaling;
        inconsistency_total += this_inconsistency;
      }
      sv2[c] = sv;
      ssim_total += ssim;
      ssim2_total += ssim2;
      dssim_total += dssim;

      old_ssim_total += ssim_old;
    }
    old_ssim_total += 0;
  }

  norm = 1. / (width / 4) / (height / 4);
  ssim_total *= norm;
  ssim2_total *= norm;
  m->ssim2 = ssim2_total;
  m->ssim = ssim_total;
  if (old_ssim_total == 0)
    inconsistency_total = 0;

  m->ssimc = inconsistency_total;

  m->dssim = dssim_total;
  return inconsistency_total;
}


#if CONFIG_VP9_HIGHBITDEPTH
double vpx_highbd_calc_ssim(const YV12_BUFFER_CONFIG *source,
                            const YV12_BUFFER_CONFIG *dest,
                            double *weight, unsigned int bd) {
  double a, b, c;
  double ssimv;

  a = vpx_highbd_ssim2(source->y_buffer, dest->y_buffer,
                       source->y_stride, dest->y_stride,
                       source->y_crop_width, source->y_crop_height, bd);

  b = vpx_highbd_ssim2(source->u_buffer, dest->u_buffer,
                       source->uv_stride, dest->uv_stride,
                       source->uv_crop_width, source->uv_crop_height, bd);

  c = vpx_highbd_ssim2(source->v_buffer, dest->v_buffer,
                       source->uv_stride, dest->uv_stride,
                       source->uv_crop_width, source->uv_crop_height, bd);

  ssimv = a * .8 + .1 * (b + c);

  *weight = 1;

  return ssimv;
}

double vpx_highbd_calc_ssimg(const YV12_BUFFER_CONFIG *source,
                             const YV12_BUFFER_CONFIG *dest, double *ssim_y,
                             double *ssim_u, double *ssim_v, unsigned int bd) {
  double ssim_all = 0;
  double a, b, c;

  a = vpx_highbd_ssim2(source->y_buffer, dest->y_buffer,
                       source->y_stride, dest->y_stride,
                       source->y_crop_width, source->y_crop_height, bd);

  b = vpx_highbd_ssim2(source->u_buffer, dest->u_buffer,
                       source->uv_stride, dest->uv_stride,
                       source->uv_crop_width, source->uv_crop_height, bd);

  c = vpx_highbd_ssim2(source->v_buffer, dest->v_buffer,
                       source->uv_stride, dest->uv_stride,
                       source->uv_crop_width, source->uv_crop_height, bd);
  *ssim_y = a;
  *ssim_u = b;
  *ssim_v = c;
  ssim_all = (a * 4 + b + c) / 6;

  return ssim_all;
}
#endif  

/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_INTERNAL_VPX_PSNR_H_
#define VPX_INTERNAL_VPX_PSNR_H_

#ifdef __cplusplus
extern "C" {
#endif


double vpx_sse_to_psnr(double samples, double peak, double sse);

#ifdef __cplusplus
}  
#endif

#endif  

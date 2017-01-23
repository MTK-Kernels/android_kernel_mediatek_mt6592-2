/* Copyright (c) 2011 The WebM project authors. All Rights Reserved. */
/* Use of this source code is governed by a BSD-style license */
/* that can be found in the LICENSE file in the root of the source */
#include "vpx/vpx_codec.h"
static const char* const cfg = "--target=mips32-linux-gcc --disable-dspr2 --enable-external-build --enable-realtime-only --enable-pic --disable-runtime-cpu-detect";
const char *vpx_codec_build_config(void) {return cfg;}

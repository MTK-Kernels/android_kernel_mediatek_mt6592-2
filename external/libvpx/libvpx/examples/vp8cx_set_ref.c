/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"

#include "../tools_common.h"
#include "../video_writer.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <width> <height> <infile> <outfile> <frame>\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static int encode_frame(vpx_codec_ctx_t *codec,
                        vpx_image_t *img,
                        int frame_index,
                        VpxVideoWriter *writer) {
  int got_pkts = 0;
  vpx_codec_iter_t iter = NULL;
  const vpx_codec_cx_pkt_t *pkt = NULL;
  const vpx_codec_err_t res = vpx_codec_encode(codec, img, frame_index, 1, 0,
                                               VPX_DL_GOOD_QUALITY);
  if (res != VPX_CODEC_OK)
    die_codec(codec, "Failed to encode frame");

  while ((pkt = vpx_codec_get_cx_data(codec, &iter)) != NULL) {
    got_pkts = 1;

    if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
      const int keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;
      if (!vpx_video_writer_write_frame(writer,
                                        pkt->data.frame.buf,
                                        pkt->data.frame.sz,
                                        pkt->data.frame.pts)) {
        die_codec(codec, "Failed to write compressed frame");
      }

      printf(keyframe ? "K" : ".");
      fflush(stdout);
    }
  }

  return got_pkts;
}

int main(int argc, char **argv) {
  FILE *infile = NULL;
  vpx_codec_ctx_t codec = {0};
  vpx_codec_enc_cfg_t cfg = {0};
  int frame_count = 0;
  vpx_image_t raw;
  vpx_codec_err_t res;
  VpxVideoInfo info = {0};
  VpxVideoWriter *writer = NULL;
  const VpxInterface *encoder = NULL;
  int update_frame_num = 0;
  const int fps = 30;        
  const int bitrate = 200;   

  exec_name = argv[0];

  if (argc != 6)
    die("Invalid number of arguments");

  
  encoder = get_vpx_encoder_by_name("vp8");
  if (!encoder)
    die("Unsupported codec.");

  update_frame_num = atoi(argv[5]);
  if (!update_frame_num)
    die("Couldn't parse frame number '%s'\n", argv[5]);

  info.codec_fourcc = encoder->fourcc;
  info.frame_width = strtol(argv[1], NULL, 0);
  info.frame_height = strtol(argv[2], NULL, 0);
  info.time_base.numerator = 1;
  info.time_base.denominator = fps;

  if (info.frame_width <= 0 ||
      info.frame_height <= 0 ||
      (info.frame_width % 2) != 0 ||
      (info.frame_height % 2) != 0) {
    die("Invalid frame size: %dx%d", info.frame_width, info.frame_height);
  }

  if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, info.frame_width,
                                             info.frame_height, 1)) {
    die("Failed to allocate image.");
  }

  printf("Using %s\n", vpx_codec_iface_name(encoder->codec_interface()));

  res = vpx_codec_enc_config_default(encoder->codec_interface(), &cfg, 0);
  if (res)
    die_codec(&codec, "Failed to get default codec config.");

  cfg.g_w = info.frame_width;
  cfg.g_h = info.frame_height;
  cfg.g_timebase.num = info.time_base.numerator;
  cfg.g_timebase.den = info.time_base.denominator;
  cfg.rc_target_bitrate = bitrate;

  writer = vpx_video_writer_open(argv[4], kContainerIVF, &info);
  if (!writer)
    die("Failed to open %s for writing.", argv[4]);

  if (!(infile = fopen(argv[3], "rb")))
    die("Failed to open %s for reading.", argv[3]);

  if (vpx_codec_enc_init(&codec, encoder->codec_interface(), &cfg, 0))
    die_codec(&codec, "Failed to initialize encoder");

  
  while (vpx_img_read(&raw, infile)) {
    if (frame_count + 1 == update_frame_num) {
      vpx_ref_frame_t ref;
      ref.frame_type = VP8_LAST_FRAME;
      ref.img = raw;
      if (vpx_codec_control(&codec, VP8_SET_REFERENCE, &ref))
        die_codec(&codec, "Failed to set reference frame");
    }

    encode_frame(&codec, &raw, frame_count++, writer);
  }

  
  while (encode_frame(&codec, NULL, -1, writer)) {}

  printf("\n");
  fclose(infile);
  printf("Processed %d frames.\n", frame_count);

  vpx_img_free(&raw);
  if (vpx_codec_destroy(&codec))
    die_codec(&codec, "Failed to destroy codec.");

  vpx_video_writer_close(writer);

  return EXIT_SUCCESS;
}

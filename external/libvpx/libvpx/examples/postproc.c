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

#include "vpx/vp8dx.h"
#include "vpx/vpx_decoder.h"

#include "../tools_common.h"
#include "../video_reader.h"
#include "./vpx_config.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr, "Usage: %s <infile> <outfile>\n", exec_name);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  int frame_cnt = 0;
  FILE *outfile = NULL;
  vpx_codec_ctx_t codec;
  vpx_codec_err_t res;
  VpxVideoReader *reader = NULL;
  const VpxInterface *decoder = NULL;
  const VpxVideoInfo *info = NULL;

  exec_name = argv[0];

  if (argc != 3)
    die("Invalid number of arguments.");

  reader = vpx_video_reader_open(argv[1]);
  if (!reader)
    die("Failed to open %s for reading.", argv[1]);

  if (!(outfile = fopen(argv[2], "wb")))
    die("Failed to open %s for writing", argv[2]);

  info = vpx_video_reader_get_info(reader);

  decoder = get_vpx_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder)
    die("Unknown input codec.");

  printf("Using %s\n", vpx_codec_iface_name(decoder->codec_interface()));

  res = vpx_codec_dec_init(&codec, decoder->codec_interface(), NULL,
                           VPX_CODEC_USE_POSTPROC);
  if (res == VPX_CODEC_INCAPABLE)
    die_codec(&codec, "Postproc not supported by this decoder.");

  if (res)
    die_codec(&codec, "Failed to initialize decoder.");

  while (vpx_video_reader_read_frame(reader)) {
    vpx_codec_iter_t iter = NULL;
    vpx_image_t *img = NULL;
    size_t frame_size = 0;
    const unsigned char *frame = vpx_video_reader_get_frame(reader,
                                                            &frame_size);

    ++frame_cnt;

    if (frame_cnt % 30 == 1) {
      vp8_postproc_cfg_t pp = {0, 0, 0};

    if (vpx_codec_control(&codec, VP8_SET_POSTPROC, &pp))
      die_codec(&codec, "Failed to turn off postproc.");
    } else if (frame_cnt % 30 == 16) {
      vp8_postproc_cfg_t pp = {VP8_DEBLOCK | VP8_DEMACROBLOCK | VP8_MFQE,
                               4, 0};
      if (vpx_codec_control(&codec, VP8_SET_POSTPROC, &pp))
        die_codec(&codec, "Failed to turn on postproc.");
    };

    
    if (vpx_codec_decode(&codec, frame, (unsigned int)frame_size, NULL, 15000))
      die_codec(&codec, "Failed to decode frame");

    while ((img = vpx_codec_get_frame(&codec, &iter)) != NULL) {
      vpx_img_write(img, outfile);
    }
  }

  printf("Processed %d frames.\n", frame_cnt);
  if (vpx_codec_destroy(&codec))
    die_codec(&codec, "Failed to destroy codec");

  printf("Play: ffplay -f rawvideo -pix_fmt yuv420p -s %dx%d %s\n",
         info->frame_width, info->frame_height, argv[2]);

  vpx_video_reader_close(reader);

  fclose(outfile);
  return EXIT_SUCCESS;
}

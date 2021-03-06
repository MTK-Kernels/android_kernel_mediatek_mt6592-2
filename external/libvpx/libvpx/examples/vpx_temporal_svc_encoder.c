/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./vpx_config.h"
#include "../vpx_ports/vpx_timer.h"
#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"

#include "../tools_common.h"
#include "../video_writer.h"

static const char *exec_name;

void usage_exit(void) {
  exit(EXIT_FAILURE);
}

enum denoiserState {
  kDenoiserOff,
  kDenoiserOnYOnly,
  kDenoiserOnYUV,
  kDenoiserOnYUVAggressive,
  kDenoiserOnAdaptive
};

static int mode_to_num_layers[12] = {1, 2, 2, 3, 3, 3, 3, 5, 2, 3, 3, 3};

struct RateControlMetrics {
  
  int layer_input_frames[VPX_TS_MAX_LAYERS];
  
  int layer_tot_enc_frames[VPX_TS_MAX_LAYERS];
  
  int layer_enc_frames[VPX_TS_MAX_LAYERS];
  
  double layer_framerate[VPX_TS_MAX_LAYERS];
  
  double layer_pfb[VPX_TS_MAX_LAYERS];
  
  double layer_avg_frame_size[VPX_TS_MAX_LAYERS];
  
  double layer_avg_rate_mismatch[VPX_TS_MAX_LAYERS];
  
  double layer_encoding_bitrate[VPX_TS_MAX_LAYERS];
  
  
  double avg_st_encoding_bitrate;
  
  double variance_st_encoding_bitrate;
  
  int window_size;
  
  int window_count;
  int layer_target_bitrate[VPX_MAX_LAYERS];
};

static void set_rate_control_metrics(struct RateControlMetrics *rc,
                                     vpx_codec_enc_cfg_t *cfg) {
  unsigned int i = 0;
  
  
  const double framerate = cfg->g_timebase.den / cfg->g_timebase.num;
  rc->layer_framerate[0] = framerate / cfg->ts_rate_decimator[0];
  rc->layer_pfb[0] = 1000.0 * rc->layer_target_bitrate[0] /
      rc->layer_framerate[0];
  for (i = 0; i < cfg->ts_number_layers; ++i) {
    if (i > 0) {
      rc->layer_framerate[i] = framerate / cfg->ts_rate_decimator[i];
      rc->layer_pfb[i] = 1000.0 *
          (rc->layer_target_bitrate[i] - rc->layer_target_bitrate[i - 1]) /
          (rc->layer_framerate[i] - rc->layer_framerate[i - 1]);
    }
    rc->layer_input_frames[i] = 0;
    rc->layer_enc_frames[i] = 0;
    rc->layer_tot_enc_frames[i] = 0;
    rc->layer_encoding_bitrate[i] = 0.0;
    rc->layer_avg_frame_size[i] = 0.0;
    rc->layer_avg_rate_mismatch[i] = 0.0;
  }
  rc->window_count = 0;
  rc->window_size = 15;
  rc->avg_st_encoding_bitrate = 0.0;
  rc->variance_st_encoding_bitrate = 0.0;
}

static void printout_rate_control_summary(struct RateControlMetrics *rc,
                                          vpx_codec_enc_cfg_t *cfg,
                                          int frame_cnt) {
  unsigned int i = 0;
  int tot_num_frames = 0;
  double perc_fluctuation = 0.0;
  printf("Total number of processed frames: %d\n\n", frame_cnt -1);
  printf("Rate control layer stats for %d layer(s):\n\n",
      cfg->ts_number_layers);
  for (i = 0; i < cfg->ts_number_layers; ++i) {
    const int num_dropped = (i > 0) ?
        (rc->layer_input_frames[i] - rc->layer_enc_frames[i]) :
        (rc->layer_input_frames[i] - rc->layer_enc_frames[i] - 1);
    tot_num_frames += rc->layer_input_frames[i];
    rc->layer_encoding_bitrate[i] = 0.001 * rc->layer_framerate[i] *
        rc->layer_encoding_bitrate[i] / tot_num_frames;
    rc->layer_avg_frame_size[i] = rc->layer_avg_frame_size[i] /
        rc->layer_enc_frames[i];
    rc->layer_avg_rate_mismatch[i] = 100.0 * rc->layer_avg_rate_mismatch[i] /
        rc->layer_enc_frames[i];
    printf("For layer#: %d \n", i);
    printf("Bitrate (target vs actual): %d %f \n", rc->layer_target_bitrate[i],
           rc->layer_encoding_bitrate[i]);
    printf("Average frame size (target vs actual): %f %f \n", rc->layer_pfb[i],
           rc->layer_avg_frame_size[i]);
    printf("Average rate_mismatch: %f \n", rc->layer_avg_rate_mismatch[i]);
    printf("Number of input frames, encoded (non-key) frames, "
        "and perc dropped frames: %d %d %f \n", rc->layer_input_frames[i],
        rc->layer_enc_frames[i],
        100.0 * num_dropped / rc->layer_input_frames[i]);
    printf("\n");
  }
  rc->avg_st_encoding_bitrate = rc->avg_st_encoding_bitrate / rc->window_count;
  rc->variance_st_encoding_bitrate =
      rc->variance_st_encoding_bitrate / rc->window_count -
      (rc->avg_st_encoding_bitrate * rc->avg_st_encoding_bitrate);
  perc_fluctuation = 100.0 * sqrt(rc->variance_st_encoding_bitrate) /
      rc->avg_st_encoding_bitrate;
  printf("Short-time stats, for window of %d frames: \n",rc->window_size);
  printf("Average, rms-variance, and percent-fluct: %f %f %f \n",
         rc->avg_st_encoding_bitrate,
         sqrt(rc->variance_st_encoding_bitrate),
         perc_fluctuation);
  if ((frame_cnt - 1) != tot_num_frames)
    die("Error: Number of input frames not equal to output! \n");
}

static void set_temporal_layer_pattern(int layering_mode,
                                       vpx_codec_enc_cfg_t *cfg,
                                       int *layer_flags,
                                       int *flag_periodicity) {
  switch (layering_mode) {
    case 0: {
      
      int ids[1] = {0};
      cfg->ts_periodicity = 1;
      *flag_periodicity = 1;
      cfg->ts_number_layers = 1;
      cfg->ts_rate_decimator[0] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      
      layer_flags[0] = VPX_EFLAG_FORCE_KF  | VP8_EFLAG_NO_UPD_GF |
          VP8_EFLAG_NO_UPD_ARF;
      break;
    }
    case 1: {
      
      int ids[2] = {0, 1};
      cfg->ts_periodicity = 2;
      *flag_periodicity = 2;
      cfg->ts_number_layers = 2;
      cfg->ts_rate_decimator[0] = 2;
      cfg->ts_rate_decimator[1] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
#if 1
      
      layer_flags[0] = VPX_EFLAG_FORCE_KF  | VP8_EFLAG_NO_UPD_GF |
          VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF;
      layer_flags[1] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST |
          VP8_EFLAG_NO_REF_ARF;
#else
       
      layer_flags[0] = VPX_EFLAG_FORCE_KF  | VP8_EFLAG_NO_UPD_GF |
          VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF;
      layer_flags[1] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST |
          VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_REF_LAST;
#endif
      break;
    }
    case 2: {
      
      int ids[3] = {0, 1, 1};
      cfg->ts_periodicity = 3;
      *flag_periodicity = 3;
      cfg->ts_number_layers = 2;
      cfg->ts_rate_decimator[0] = 3;
      cfg->ts_rate_decimator[1] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      
      layer_flags[0] = VPX_EFLAG_FORCE_KF  | VP8_EFLAG_NO_REF_GF |
          VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      layer_flags[1] =
      layer_flags[2] = VP8_EFLAG_NO_REF_GF  | VP8_EFLAG_NO_REF_ARF |
          VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
      break;
    }
    case 3: {
      
      int ids[6] = {0, 2, 2, 1, 2, 2};
      cfg->ts_periodicity = 6;
      *flag_periodicity = 6;
      cfg->ts_number_layers = 3;
      cfg->ts_rate_decimator[0] = 6;
      cfg->ts_rate_decimator[1] = 3;
      cfg->ts_rate_decimator[2] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      
      layer_flags[0] = VPX_EFLAG_FORCE_KF  | VP8_EFLAG_NO_REF_GF |
          VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      layer_flags[3] = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_ARF |
          VP8_EFLAG_NO_UPD_LAST;
      layer_flags[1] =
      layer_flags[2] =
      layer_flags[4] =
      layer_flags[5] = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_LAST;
      break;
    }
    case 4: {
      
      int ids[4] = {0, 2, 1, 2};
      cfg->ts_periodicity = 4;
      *flag_periodicity = 4;
      cfg->ts_number_layers = 3;
      cfg->ts_rate_decimator[0] = 4;
      cfg->ts_rate_decimator[1] = 2;
      cfg->ts_rate_decimator[2] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      
      layer_flags[0] = VPX_EFLAG_FORCE_KF  | VP8_EFLAG_NO_REF_GF |
          VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      layer_flags[2] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
          VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
      layer_flags[1] =
      layer_flags[3] = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_LAST |
          VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      break;
    }
    case 5: {
      
      int ids[4] = {0, 2, 1, 2};
      cfg->ts_periodicity = 4;
      *flag_periodicity = 4;
      cfg->ts_number_layers     = 3;
      cfg->ts_rate_decimator[0] = 4;
      cfg->ts_rate_decimator[1] = 2;
      cfg->ts_rate_decimator[2] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      
      
      layer_flags[0] = VPX_EFLAG_FORCE_KF  | VP8_EFLAG_NO_REF_GF |
          VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      layer_flags[2] = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_LAST |
          VP8_EFLAG_NO_UPD_ARF;
      layer_flags[1] =
      layer_flags[3] = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_LAST |
          VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      break;
    }
    case 6: {
      
      int ids[4] = {0, 2, 1, 2};
      cfg->ts_periodicity = 4;
      *flag_periodicity = 4;
      cfg->ts_number_layers = 3;
      cfg->ts_rate_decimator[0] = 4;
      cfg->ts_rate_decimator[1] = 2;
      cfg->ts_rate_decimator[2] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      
      layer_flags[0] = VPX_EFLAG_FORCE_KF  | VP8_EFLAG_NO_REF_GF |
          VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      layer_flags[2] = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_LAST |
          VP8_EFLAG_NO_UPD_ARF;
      layer_flags[1] =
      layer_flags[3] = VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF;
      break;
    }
    case 7: {
      
      
      int ids[16] = {0, 4, 3, 4, 2, 4, 3, 4, 1, 4, 3, 4, 2, 4, 3, 4};
      cfg->ts_periodicity = 16;
      *flag_periodicity = 16;
      cfg->ts_number_layers = 5;
      cfg->ts_rate_decimator[0] = 16;
      cfg->ts_rate_decimator[1] = 8;
      cfg->ts_rate_decimator[2] = 4;
      cfg->ts_rate_decimator[3] = 2;
      cfg->ts_rate_decimator[4] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      layer_flags[0]  = VPX_EFLAG_FORCE_KF;
      layer_flags[1]  =
      layer_flags[3]  =
      layer_flags[5]  =
      layer_flags[7]  =
      layer_flags[9]  =
      layer_flags[11] =
      layer_flags[13] =
      layer_flags[15] = VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF |
          VP8_EFLAG_NO_UPD_ARF;
      layer_flags[2]  =
      layer_flags[6]  =
      layer_flags[10] =
      layer_flags[14] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_GF;
      layer_flags[4] =
      layer_flags[12] = VP8_EFLAG_NO_REF_LAST | VP8_EFLAG_NO_UPD_ARF;
      layer_flags[8]  = VP8_EFLAG_NO_REF_LAST | VP8_EFLAG_NO_REF_GF;
      break;
    }
    case 8: {
      
      int ids[2] = {0, 1};
      cfg->ts_periodicity = 2;
      *flag_periodicity = 8;
      cfg->ts_number_layers = 2;
      cfg->ts_rate_decimator[0] = 2;
      cfg->ts_rate_decimator[1] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      
      
      

      
      layer_flags[0] = VPX_EFLAG_FORCE_KF  | VP8_EFLAG_NO_REF_GF |
          VP8_EFLAG_NO_UPD_ARF;
      
      layer_flags[1] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_LAST |
          VP8_EFLAG_NO_UPD_ARF;
      
      layer_flags[2] = VP8_EFLAG_NO_REF_GF  | VP8_EFLAG_NO_UPD_GF |
          VP8_EFLAG_NO_UPD_ARF;
      
      layer_flags[3] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST |
          VP8_EFLAG_NO_UPD_ENTROPY;
      
      layer_flags[4] = layer_flags[2];
      
      layer_flags[5] = layer_flags[3];
      
      layer_flags[6] = layer_flags[4];
      
      layer_flags[7] = layer_flags[5];
     break;
    }
    case 9: {
      
      int ids[4] = {0, 2, 1, 2};
      cfg->ts_periodicity = 4;
      *flag_periodicity = 8;
      cfg->ts_number_layers = 3;
      cfg->ts_rate_decimator[0] = 4;
      cfg->ts_rate_decimator[1] = 2;
      cfg->ts_rate_decimator[2] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      
      layer_flags[0] = VPX_EFLAG_FORCE_KF  | VP8_EFLAG_NO_REF_GF |
          VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      layer_flags[1] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
          VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF;
      layer_flags[2] = VP8_EFLAG_NO_REF_GF   | VP8_EFLAG_NO_REF_ARF |
          VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ARF;
      layer_flags[3] =
      layer_flags[5] = VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF;
      layer_flags[4] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_ARF |
          VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
      layer_flags[6] = VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_LAST |
          VP8_EFLAG_NO_UPD_ARF;
      layer_flags[7] = VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_GF |
          VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_ENTROPY;
      break;
    }
    case 10: {
      
      
      

      int ids[4] = {0, 2, 1, 2};
      cfg->ts_periodicity = 4;
      *flag_periodicity = 8;
      cfg->ts_number_layers = 3;
      cfg->ts_rate_decimator[0] = 4;
      cfg->ts_rate_decimator[1] = 2;
      cfg->ts_rate_decimator[2] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      
      
      layer_flags[0] = VPX_EFLAG_FORCE_KF | VP8_EFLAG_NO_UPD_ARF |
          VP8_EFLAG_NO_REF_GF;
      
      layer_flags[1] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF |
          VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST |
          VP8_EFLAG_NO_UPD_ENTROPY;
      
      layer_flags[2] = VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_ARF |
          VP8_EFLAG_NO_UPD_LAST;
      
      layer_flags[3] = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
          VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY;
      
      layer_flags[4] = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
          VP8_EFLAG_NO_REF_GF;
      
      layer_flags[5] = layer_flags[3];
      
      layer_flags[6] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
      
      layer_flags[7] = layer_flags[3];
      break;
    }
    case 11:
    default: {
      
      
      int ids[4] = {0, 2, 1, 2};
      cfg->ts_periodicity = 4;
      *flag_periodicity = 8;
      cfg->ts_number_layers = 3;
      cfg->ts_rate_decimator[0] = 4;
      cfg->ts_rate_decimator[1] = 2;
      cfg->ts_rate_decimator[2] = 1;
      memcpy(cfg->ts_layer_id, ids, sizeof(ids));
      
      
      layer_flags[0] = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
          VP8_EFLAG_NO_REF_GF;
      layer_flags[4] = layer_flags[0];
      
      layer_flags[2] = VP8_EFLAG_NO_UPD_ARF | VP8_EFLAG_NO_UPD_LAST;
      layer_flags[6] = layer_flags[2];
      
      layer_flags[1] = VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF |
          VP8_EFLAG_NO_UPD_LAST | VP8_EFLAG_NO_UPD_ENTROPY;
      layer_flags[3] = layer_flags[1];
      layer_flags[5] = layer_flags[1];
      layer_flags[7] = layer_flags[1];
      break;
    }
  }
}

int main(int argc, char **argv) {
  VpxVideoWriter *outfile[VPX_TS_MAX_LAYERS] = {NULL};
  vpx_codec_ctx_t codec;
  vpx_codec_enc_cfg_t cfg;
  int frame_cnt = 0;
  vpx_image_t raw;
  vpx_codec_err_t res;
  unsigned int width;
  unsigned int height;
  int speed;
  int frame_avail;
  int got_data;
  int flags = 0;
  unsigned int i;
  int pts = 0;  
  int frame_duration = 1;  
  int layering_mode = 0;
  int layer_flags[VPX_TS_MAX_PERIODICITY] = {0};
  int flag_periodicity = 1;
#if VPX_ENCODER_ABI_VERSION > (4 + VPX_CODEC_ABI_VERSION)
  vpx_svc_layer_id_t layer_id = {0, 0};
#else
  vpx_svc_layer_id_t layer_id = {0};
#endif
  const VpxInterface *encoder = NULL;
  FILE *infile = NULL;
  struct RateControlMetrics rc;
  int64_t cx_time = 0;
  const int min_args_base = 11;
#if CONFIG_VP9_HIGHBITDEPTH
  vpx_bit_depth_t bit_depth = VPX_BITS_8;
  int input_bit_depth = 8;
  const int min_args = min_args_base + 1;
#else
  const int min_args = min_args_base;
#endif  
  double sum_bitrate = 0.0;
  double sum_bitrate2 = 0.0;
  double framerate  = 30.0;

  exec_name = argv[0];
  
  if (argc < min_args) {
#if CONFIG_VP9_HIGHBITDEPTH
    die("Usage: %s <infile> <outfile> <codec_type(vp8/vp9)> <width> <height> "
        "<rate_num> <rate_den> <speed> <frame_drop_threshold> <mode> "
        "<Rate_0> ... <Rate_nlayers-1> <bit-depth> \n", argv[0]);
#else
    die("Usage: %s <infile> <outfile> <codec_type(vp8/vp9)> <width> <height> "
        "<rate_num> <rate_den> <speed> <frame_drop_threshold> <mode> "
        "<Rate_0> ... <Rate_nlayers-1> \n", argv[0]);
#endif  
  }

  encoder = get_vpx_encoder_by_name(argv[3]);
  if (!encoder)
    die("Unsupported codec.");

  printf("Using %s\n", vpx_codec_iface_name(encoder->codec_interface()));

  width = strtol(argv[4], NULL, 0);
  height = strtol(argv[5], NULL, 0);
  if (width < 16 || width % 2 || height < 16 || height % 2) {
    die("Invalid resolution: %d x %d", width, height);
  }

  layering_mode = strtol(argv[10], NULL, 0);
  if (layering_mode < 0 || layering_mode > 12) {
    die("Invalid layering mode (0..12) %s", argv[10]);
  }

  if (argc != min_args + mode_to_num_layers[layering_mode]) {
    die("Invalid number of arguments");
  }

#if CONFIG_VP9_HIGHBITDEPTH
  switch (strtol(argv[argc-1], NULL, 0)) {
    case 8:
      bit_depth = VPX_BITS_8;
      input_bit_depth = 8;
      break;
    case 10:
      bit_depth = VPX_BITS_10;
      input_bit_depth = 10;
      break;
    case 12:
      bit_depth = VPX_BITS_12;
      input_bit_depth = 12;
      break;
    default:
      die("Invalid bit depth (8, 10, 12) %s", argv[argc-1]);
  }
  if (!vpx_img_alloc(&raw,
                     bit_depth == VPX_BITS_8 ? VPX_IMG_FMT_I420 :
                                               VPX_IMG_FMT_I42016,
                     width, height, 32)) {
    die("Failed to allocate image", width, height);
  }
#else
  if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, width, height, 32)) {
    die("Failed to allocate image", width, height);
  }
#endif  

  
  res = vpx_codec_enc_config_default(encoder->codec_interface(), &cfg, 0);
  if (res) {
    printf("Failed to get config: %s\n", vpx_codec_err_to_string(res));
    return EXIT_FAILURE;
  }

  
  cfg.g_w = width;
  cfg.g_h = height;

#if CONFIG_VP9_HIGHBITDEPTH
  if (bit_depth != VPX_BITS_8) {
    cfg.g_bit_depth = bit_depth;
    cfg.g_input_bit_depth = input_bit_depth;
    cfg.g_profile = 2;
  }
#endif  

  
  cfg.g_timebase.num = strtol(argv[6], NULL, 0);
  cfg.g_timebase.den = strtol(argv[7], NULL, 0);

  speed = strtol(argv[8], NULL, 0);
  if (speed < 0) {
    die("Invalid speed setting: must be positive");
  }

  for (i = min_args_base;
       (int)i < min_args_base + mode_to_num_layers[layering_mode];
       ++i) {
    rc.layer_target_bitrate[i - 11] = strtol(argv[i], NULL, 0);
    if (strncmp(encoder->name, "vp8", 3) == 0)
      cfg.ts_target_bitrate[i - 11] = rc.layer_target_bitrate[i - 11];
    else if (strncmp(encoder->name, "vp9", 3) == 0)
      cfg.layer_target_bitrate[i - 11] = rc.layer_target_bitrate[i - 11];
  }

  
  cfg.rc_dropframe_thresh = strtol(argv[9], NULL, 0);
  cfg.rc_end_usage = VPX_CBR;
  cfg.rc_min_quantizer = 2;
  cfg.rc_max_quantizer = 56;
  if (strncmp(encoder->name, "vp9", 3) == 0)
    cfg.rc_max_quantizer = 52;
  cfg.rc_undershoot_pct = 50;
  cfg.rc_overshoot_pct = 50;
  cfg.rc_buf_initial_sz = 500;
  cfg.rc_buf_optimal_sz = 600;
  cfg.rc_buf_sz = 1000;

  
  cfg.rc_resize_allowed = 0;

  
  cfg.g_threads = 1;

  
  cfg.g_error_resilient = 1;
  cfg.g_lag_in_frames   = 0;
  cfg.kf_mode = VPX_KF_AUTO;

  
  cfg.kf_min_dist = cfg.kf_max_dist = 3000;

  cfg.temporal_layering_mode = VP9E_TEMPORAL_LAYERING_MODE_BYPASS;

  set_temporal_layer_pattern(layering_mode,
                             &cfg,
                             layer_flags,
                             &flag_periodicity);

  set_rate_control_metrics(&rc, &cfg);

  
  
  cfg.rc_target_bitrate = rc.layer_target_bitrate[cfg.ts_number_layers - 1];

  
  if (!(infile = fopen(argv[1], "rb"))) {
    die("Failed to open %s for reading", argv[1]);
  }

  framerate = cfg.g_timebase.den / cfg.g_timebase.num;
  
  for (i = 0; i < cfg.ts_number_layers; ++i) {
    char file_name[PATH_MAX];
    VpxVideoInfo info;
    info.codec_fourcc = encoder->fourcc;
    info.frame_width = cfg.g_w;
    info.frame_height = cfg.g_h;
    info.time_base.numerator = cfg.g_timebase.num;
    info.time_base.denominator = cfg.g_timebase.den;

    snprintf(file_name, sizeof(file_name), "%s_%d.ivf", argv[2], i);
    outfile[i] = vpx_video_writer_open(file_name, kContainerIVF, &info);
    if (!outfile[i])
      die("Failed to open %s for writing", file_name);

    assert(outfile[i] != NULL);
  }
  
  cfg.ss_number_layers = 1;

  
#if CONFIG_VP9_HIGHBITDEPTH
  if (vpx_codec_enc_init(
          &codec, encoder->codec_interface(), &cfg,
          bit_depth == VPX_BITS_8 ? 0 : VPX_CODEC_USE_HIGHBITDEPTH))
#else
  if (vpx_codec_enc_init(&codec, encoder->codec_interface(), &cfg, 0))
#endif  
    die_codec(&codec, "Failed to initialize encoder");

  if (strncmp(encoder->name, "vp8", 3) == 0) {
    vpx_codec_control(&codec, VP8E_SET_CPUUSED, -speed);
    vpx_codec_control(&codec, VP8E_SET_NOISE_SENSITIVITY, kDenoiserOff);
    vpx_codec_control(&codec, VP8E_SET_STATIC_THRESHOLD, 0);
  } else if (strncmp(encoder->name, "vp9", 3) == 0) {
    vpx_svc_extra_cfg_t svc_params;
    vpx_codec_control(&codec, VP8E_SET_CPUUSED, speed);
    vpx_codec_control(&codec, VP9E_SET_AQ_MODE, 3);
    vpx_codec_control(&codec, VP9E_SET_FRAME_PERIODIC_BOOST, 0);
    vpx_codec_control(&codec, VP9E_SET_NOISE_SENSITIVITY, 0);
    vpx_codec_control(&codec, VP8E_SET_STATIC_THRESHOLD, 0);
    vpx_codec_control(&codec, VP9E_SET_TUNE_CONTENT, 0);
    vpx_codec_control(&codec, VP9E_SET_TILE_COLUMNS, (cfg.g_threads >> 1));
    if (vpx_codec_control(&codec, VP9E_SET_SVC, layering_mode > 0 ? 1: 0))
      die_codec(&codec, "Failed to set SVC");
    for (i = 0; i < cfg.ts_number_layers; ++i) {
      svc_params.max_quantizers[i] = cfg.rc_max_quantizer;
      svc_params.min_quantizers[i] = cfg.rc_min_quantizer;
    }
    svc_params.scaling_factor_num[0] = cfg.g_h;
    svc_params.scaling_factor_den[0] = cfg.g_h;
    vpx_codec_control(&codec, VP9E_SET_SVC_PARAMETERS, &svc_params);
  }
  if (strncmp(encoder->name, "vp8", 3) == 0) {
    vpx_codec_control(&codec, VP8E_SET_SCREEN_CONTENT_MODE, 0);
  }
  vpx_codec_control(&codec, VP8E_SET_TOKEN_PARTITIONS, 1);
  
  
  
  {
    const int max_intra_size_pct = 900;
    vpx_codec_control(&codec, VP8E_SET_MAX_INTRA_BITRATE_PCT,
                      max_intra_size_pct);
  }

  frame_avail = 1;
  while (frame_avail || got_data) {
    struct vpx_usec_timer timer;
    vpx_codec_iter_t iter = NULL;
    const vpx_codec_cx_pkt_t *pkt;
#if VPX_ENCODER_ABI_VERSION > (4 + VPX_CODEC_ABI_VERSION)
    
    layer_id.spatial_layer_id = 0;
#endif
    layer_id.temporal_layer_id =
        cfg.ts_layer_id[frame_cnt % cfg.ts_periodicity];
    if (strncmp(encoder->name, "vp9", 3) == 0) {
      vpx_codec_control(&codec, VP9E_SET_SVC_LAYER_ID, &layer_id);
    } else if (strncmp(encoder->name, "vp8", 3) == 0) {
      vpx_codec_control(&codec, VP8E_SET_TEMPORAL_LAYER_ID,
                        layer_id.temporal_layer_id);
    }
    flags = layer_flags[frame_cnt % flag_periodicity];
    if (layering_mode == 0)
      flags = 0;
    frame_avail = vpx_img_read(&raw, infile);
    if (frame_avail)
      ++rc.layer_input_frames[layer_id.temporal_layer_id];
    vpx_usec_timer_start(&timer);
    if (vpx_codec_encode(&codec, frame_avail? &raw : NULL, pts, 1, flags,
        VPX_DL_REALTIME)) {
      die_codec(&codec, "Failed to encode frame");
    }
    vpx_usec_timer_mark(&timer);
    cx_time += vpx_usec_timer_elapsed(&timer);
    
    if (layering_mode != 7) {
      layer_flags[0] &= ~VPX_EFLAG_FORCE_KF;
    }
    got_data = 0;
    while ( (pkt = vpx_codec_get_cx_data(&codec, &iter)) ) {
      got_data = 1;
      switch (pkt->kind) {
        case VPX_CODEC_CX_FRAME_PKT:
          for (i = cfg.ts_layer_id[frame_cnt % cfg.ts_periodicity];
              i < cfg.ts_number_layers; ++i) {
            vpx_video_writer_write_frame(outfile[i], pkt->data.frame.buf,
                                         pkt->data.frame.sz, pts);
            ++rc.layer_tot_enc_frames[i];
            rc.layer_encoding_bitrate[i] += 8.0 * pkt->data.frame.sz;
            
            if (i == cfg.ts_layer_id[frame_cnt % cfg.ts_periodicity] &&
                !(pkt->data.frame.flags & VPX_FRAME_IS_KEY)) {
              rc.layer_avg_frame_size[i] += 8.0 * pkt->data.frame.sz;
              rc.layer_avg_rate_mismatch[i] +=
                  fabs(8.0 * pkt->data.frame.sz - rc.layer_pfb[i]) /
                  rc.layer_pfb[i];
              ++rc.layer_enc_frames[i];
            }
          }
          
          
          
          if (frame_cnt > rc.window_size) {
            sum_bitrate += 0.001 * 8.0 * pkt->data.frame.sz * framerate;
            if (frame_cnt % rc.window_size == 0) {
              rc.window_count += 1;
              rc.avg_st_encoding_bitrate += sum_bitrate / rc.window_size;
              rc.variance_st_encoding_bitrate +=
                  (sum_bitrate / rc.window_size) *
                  (sum_bitrate / rc.window_size);
              sum_bitrate = 0.0;
            }
          }
          
          if (frame_cnt > rc.window_size + rc.window_size / 2) {
            sum_bitrate2 += 0.001 * 8.0 * pkt->data.frame.sz * framerate;
            if (frame_cnt > 2 * rc.window_size &&
                frame_cnt % rc.window_size == 0) {
              rc.window_count += 1;
              rc.avg_st_encoding_bitrate += sum_bitrate2 / rc.window_size;
              rc.variance_st_encoding_bitrate +=
                  (sum_bitrate2 / rc.window_size) *
                  (sum_bitrate2 / rc.window_size);
              sum_bitrate2 = 0.0;
            }
          }
          break;
          default:
            break;
      }
    }
    ++frame_cnt;
    pts += frame_duration;
  }
  fclose(infile);
  printout_rate_control_summary(&rc, &cfg, frame_cnt);
  printf("\n");
  printf("Frame cnt and encoding time/FPS stats for encoding: %d %f %f \n",
          frame_cnt,
          1000 * (float)cx_time / (double)(frame_cnt * 1000000),
          1000000 * (double)frame_cnt / (double)cx_time);

  if (vpx_codec_destroy(&codec))
    die_codec(&codec, "Failed to destroy codec");

  
  for (i = 0; i < cfg.ts_number_layers; ++i)
    vpx_video_writer_close(outfile[i]);

  vpx_img_free(&raw);
  return EXIT_SUCCESS;
}

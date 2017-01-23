/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VPX_VPX_ENCODER_H_
#define VPX_VPX_ENCODER_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "./vpx_codec.h"

#define VPX_TS_MAX_PERIODICITY 16

  
#define VPX_TS_MAX_LAYERS       5

  
#define MAX_PERIODICITY VPX_TS_MAX_PERIODICITY

#define VPX_MAX_LAYERS  12  

#define MAX_LAYERS    VPX_MAX_LAYERS  

#define VPX_SS_MAX_LAYERS       5

#define VPX_SS_DEFAULT_LAYERS       1

#define VPX_ENCODER_ABI_VERSION (5 + VPX_CODEC_ABI_VERSION) 


#define VPX_CODEC_CAP_PSNR  0x10000 

#define VPX_CODEC_CAP_OUTPUT_PARTITION  0x20000

#define VPX_CODEC_CAP_HIGHBITDEPTH  0x40000

#define VPX_CODEC_USE_PSNR  0x10000 
#define VPX_CODEC_USE_OUTPUT_PARTITION  0x20000 
#define VPX_CODEC_USE_HIGHBITDEPTH 0x40000 


  typedef struct vpx_fixed_buf {
    void          *buf; 
    size_t         sz;  
  } vpx_fixed_buf_t; 


  typedef int64_t vpx_codec_pts_t;


  typedef uint32_t vpx_codec_frame_flags_t;
#define VPX_FRAME_IS_KEY       0x1 
#define VPX_FRAME_IS_DROPPABLE 0x2 
#define VPX_FRAME_IS_INVISIBLE 0x4 
#define VPX_FRAME_IS_FRAGMENT  0x8 

  typedef uint32_t vpx_codec_er_flags_t;
#define VPX_ERROR_RESILIENT_DEFAULT     0x1 
#define VPX_ERROR_RESILIENT_PARTITIONS  0x2 

  enum vpx_codec_cx_pkt_kind {
    VPX_CODEC_CX_FRAME_PKT,    
    VPX_CODEC_STATS_PKT,       
    VPX_CODEC_FPMB_STATS_PKT,  
    VPX_CODEC_PSNR_PKT,        
    
    
#if VPX_ENCODER_ABI_VERSION > (5 + VPX_CODEC_ABI_VERSION)
    VPX_CODEC_SPATIAL_SVC_LAYER_SIZES, 
    VPX_CODEC_SPATIAL_SVC_LAYER_PSNR, 
#endif
    VPX_CODEC_CUSTOM_PKT = 256 
  };


  typedef struct vpx_codec_cx_pkt {
    enum vpx_codec_cx_pkt_kind  kind; 
    union {
      struct {
        void                    *buf;      
        size_t                   sz;       
        vpx_codec_pts_t          pts;      
        unsigned long            duration; 
        vpx_codec_frame_flags_t  flags;    
        int                      partition_id; 

      } frame;  
      vpx_fixed_buf_t twopass_stats;  
      vpx_fixed_buf_t firstpass_mb_stats; 
      struct vpx_psnr_pkt {
        unsigned int samples[4];  
        uint64_t     sse[4];      
        double       psnr[4];     
      } psnr;                       
      vpx_fixed_buf_t raw;     
      
      
#if VPX_ENCODER_ABI_VERSION > (5 + VPX_CODEC_ABI_VERSION)
      size_t layer_sizes[VPX_SS_MAX_LAYERS];
      struct vpx_psnr_pkt layer_psnr[VPX_SS_MAX_LAYERS];
#endif

      char pad[128 - sizeof(enum vpx_codec_cx_pkt_kind)]; 
    } data; 
  } vpx_codec_cx_pkt_t; 


  
  
  typedef void (* vpx_codec_enc_output_cx_pkt_cb_fn_t)(vpx_codec_cx_pkt_t *pkt,
                                                       void *user_data);

  
  typedef struct vpx_codec_enc_output_cx_cb_pair {
    vpx_codec_enc_output_cx_pkt_cb_fn_t output_cx_pkt; 
    void                            *user_priv; 
  } vpx_codec_priv_output_cx_pkt_cb_pair_t;

  typedef struct vpx_rational {
    int num; 
    int den; 
  } vpx_rational_t; 


  
  enum vpx_enc_pass {
    VPX_RC_ONE_PASS,   
    VPX_RC_FIRST_PASS, 
    VPX_RC_LAST_PASS   
  };


  
  enum vpx_rc_mode {
    VPX_VBR,  
    VPX_CBR,  
    VPX_CQ,   
    VPX_Q,    
  };


  enum vpx_kf_mode {
    VPX_KF_FIXED, 
    VPX_KF_AUTO,  
    VPX_KF_DISABLED = 0 
  };


  typedef long vpx_enc_frame_flags_t;
#define VPX_EFLAG_FORCE_KF (1<<0)  


  typedef struct vpx_codec_enc_cfg {

    unsigned int           g_usage;


    unsigned int           g_threads;


    unsigned int           g_profile;  



    unsigned int           g_w;


    unsigned int           g_h;

    vpx_bit_depth_t        g_bit_depth;

    unsigned int           g_input_bit_depth;

    struct vpx_rational    g_timebase;


    vpx_codec_er_flags_t   g_error_resilient;


    enum vpx_enc_pass      g_pass;


    unsigned int           g_lag_in_frames;



    unsigned int           rc_dropframe_thresh;


    unsigned int           rc_resize_allowed;

    unsigned int           rc_scaled_width;

    unsigned int           rc_scaled_height;

    unsigned int           rc_resize_up_thresh;


    unsigned int           rc_resize_down_thresh;


    enum vpx_rc_mode       rc_end_usage;


    vpx_fixed_buf_t   rc_twopass_stats_in;

    vpx_fixed_buf_t   rc_firstpass_mb_stats_in;

    unsigned int           rc_target_bitrate;




    unsigned int           rc_min_quantizer;


    unsigned int           rc_max_quantizer;




    unsigned int           rc_undershoot_pct;


    unsigned int           rc_overshoot_pct;




    unsigned int           rc_buf_sz;


    unsigned int           rc_buf_initial_sz;


    unsigned int           rc_buf_optimal_sz;




    unsigned int           rc_2pass_vbr_bias_pct;       


    unsigned int           rc_2pass_vbr_minsection_pct;


    unsigned int           rc_2pass_vbr_maxsection_pct;



    enum vpx_kf_mode       kf_mode;


    unsigned int           kf_min_dist;


    unsigned int           kf_max_dist;


    unsigned int           ss_number_layers;

    int                    ss_enable_auto_alt_ref[VPX_SS_MAX_LAYERS];

    unsigned int           ss_target_bitrate[VPX_SS_MAX_LAYERS];

    unsigned int           ts_number_layers;

    unsigned int           ts_target_bitrate[VPX_TS_MAX_LAYERS];

    unsigned int           ts_rate_decimator[VPX_TS_MAX_LAYERS];

    unsigned int           ts_periodicity;

    unsigned int           ts_layer_id[VPX_TS_MAX_PERIODICITY];

    unsigned int           layer_target_bitrate[VPX_MAX_LAYERS];

    int                    temporal_layering_mode;
  } vpx_codec_enc_cfg_t; 

  typedef struct vpx_svc_parameters {
    int max_quantizers[VPX_MAX_LAYERS]; 
    int min_quantizers[VPX_MAX_LAYERS]; 
    int scaling_factor_num[VPX_MAX_LAYERS]; 
    int scaling_factor_den[VPX_MAX_LAYERS]; 
    int temporal_layering_mode; 
  } vpx_svc_extra_cfg_t;


  vpx_codec_err_t vpx_codec_enc_init_ver(vpx_codec_ctx_t      *ctx,
                                         vpx_codec_iface_t    *iface,
                                         const vpx_codec_enc_cfg_t *cfg,
                                         vpx_codec_flags_t     flags,
                                         int                   ver);


#define vpx_codec_enc_init(ctx, iface, cfg, flags) \
  vpx_codec_enc_init_ver(ctx, iface, cfg, flags, VPX_ENCODER_ABI_VERSION)


  vpx_codec_err_t vpx_codec_enc_init_multi_ver(vpx_codec_ctx_t      *ctx,
                                               vpx_codec_iface_t    *iface,
                                               vpx_codec_enc_cfg_t  *cfg,
                                               int                   num_enc,
                                               vpx_codec_flags_t     flags,
                                               vpx_rational_t       *dsf,
                                               int                   ver);


#define vpx_codec_enc_init_multi(ctx, iface, cfg, num_enc, flags, dsf) \
  vpx_codec_enc_init_multi_ver(ctx, iface, cfg, num_enc, flags, dsf, \
                               VPX_ENCODER_ABI_VERSION)


  vpx_codec_err_t  vpx_codec_enc_config_default(vpx_codec_iface_t    *iface,
                                                vpx_codec_enc_cfg_t  *cfg,
                                                unsigned int          reserved);


  vpx_codec_err_t  vpx_codec_enc_config_set(vpx_codec_ctx_t            *ctx,
                                            const vpx_codec_enc_cfg_t  *cfg);


  vpx_fixed_buf_t *vpx_codec_get_global_headers(vpx_codec_ctx_t   *ctx);


#define VPX_DL_REALTIME     (1)        
#define VPX_DL_GOOD_QUALITY (1000000)  
#define VPX_DL_BEST_QUALITY (0)        
  vpx_codec_err_t  vpx_codec_encode(vpx_codec_ctx_t            *ctx,
                                    const vpx_image_t          *img,
                                    vpx_codec_pts_t             pts,
                                    unsigned long               duration,
                                    vpx_enc_frame_flags_t       flags,
                                    unsigned long               deadline);

  /*!\brief Set compressed data output buffer
   *
   * Sets the buffer that the codec should output the compressed data
   * into. This call effectively sets the buffer pointer returned in the
   * next VPX_CODEC_CX_FRAME_PKT packet. Subsequent packets will be
   * appended into this buffer. The buffer is preserved across frames,
   * so applications must periodically call this function after flushing
   * the accumulated compressed data to disk or to the network to reset
   * the pointer to the buffer's head.
   *
   * `pad_before` bytes will be skipped before writing the compressed
   * data, and `pad_after` bytes will be appended to the packet. The size
   * of the packet will be the sum of the size of the actual compressed
   * data, pad_before, and pad_after. The padding bytes will be preserved
   * (not overwritten).
   *
   * Note that calling this function does not guarantee that the returned
   * compressed data will be placed into the specified buffer. In the
   * event that the encoded data will not fit into the buffer provided,
   * the returned packet \ref MAY point to an internal buffer, as it would
   * if this call were never used. In this event, the output packet will
   * NOT have any padding, and the application must free space and copy it
   * to the proper place. This is of particular note in configurations
   * that may output multiple packets for a single encoded frame (e.g., lagged
   * encoding) or if the application does not reset the buffer periodically.
   *
   * Applications may restore the default behavior of the codec providing
   * the compressed data buffer by calling this function with a NULL
   * buffer.
   *
   * Applications \ref MUSTNOT call this function during iteration of
   * vpx_codec_get_cx_data().
   *
   * \param[in]    ctx         Pointer to this instance's context
   * \param[in]    buf         Buffer to store compressed data into
   * \param[in]    pad_before  Bytes to skip before writing compressed data
   * \param[in]    pad_after   Bytes to skip after writing compressed data
   *
   * \retval #VPX_CODEC_OK
   *     The buffer was set successfully.
   * \retval #VPX_CODEC_INVALID_PARAM
   *     A parameter was NULL, the image format is unsupported, etc.
   */
  vpx_codec_err_t vpx_codec_set_cx_data_buf(vpx_codec_ctx_t       *ctx,
                                            const vpx_fixed_buf_t *buf,
                                            unsigned int           pad_before,
                                            unsigned int           pad_after);


  const vpx_codec_cx_pkt_t *vpx_codec_get_cx_data(vpx_codec_ctx_t   *ctx,
                                                  vpx_codec_iter_t  *iter);


  const vpx_image_t *vpx_codec_get_preview_frame(vpx_codec_ctx_t   *ctx);


  
#ifdef __cplusplus
}
#endif
#endif  


/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef VPX_INTERNAL_VPX_CODEC_INTERNAL_H_
#define VPX_INTERNAL_VPX_CODEC_INTERNAL_H_
#include "../vpx_decoder.h"
#include "../vpx_encoder.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VPX_CODEC_INTERNAL_ABI_VERSION (5) 

typedef struct vpx_codec_alg_priv  vpx_codec_alg_priv_t;
typedef struct vpx_codec_priv_enc_mr_cfg vpx_codec_priv_enc_mr_cfg_t;

typedef vpx_codec_err_t (*vpx_codec_init_fn_t)(vpx_codec_ctx_t *ctx,
                                               vpx_codec_priv_enc_mr_cfg_t *data);

typedef vpx_codec_err_t (*vpx_codec_destroy_fn_t)(vpx_codec_alg_priv_t *ctx);

typedef vpx_codec_err_t (*vpx_codec_peek_si_fn_t)(const uint8_t         *data,
                                                  unsigned int           data_sz,
                                                  vpx_codec_stream_info_t *si);

typedef vpx_codec_err_t (*vpx_codec_get_si_fn_t)(vpx_codec_alg_priv_t    *ctx,
                                                 vpx_codec_stream_info_t *si);

typedef vpx_codec_err_t (*vpx_codec_control_fn_t)(vpx_codec_alg_priv_t *ctx,
                                                  va_list ap);

typedef const struct vpx_codec_ctrl_fn_map {
  int ctrl_id;
  vpx_codec_control_fn_t fn;
} vpx_codec_ctrl_fn_map_t;

typedef vpx_codec_err_t (*vpx_codec_decode_fn_t)(vpx_codec_alg_priv_t  *ctx,
                                                 const uint8_t         *data,
                                                 unsigned int     data_sz,
                                                 void        *user_priv,
                                                 long         deadline);

typedef vpx_image_t *(*vpx_codec_get_frame_fn_t)(vpx_codec_alg_priv_t *ctx,
                                                 vpx_codec_iter_t     *iter);

typedef vpx_codec_err_t (*vpx_codec_set_fb_fn_t)(
    vpx_codec_alg_priv_t *ctx,
    vpx_get_frame_buffer_cb_fn_t cb_get,
    vpx_release_frame_buffer_cb_fn_t cb_release, void *cb_priv);


typedef vpx_codec_err_t (*vpx_codec_encode_fn_t)(vpx_codec_alg_priv_t  *ctx,
                                                 const vpx_image_t     *img,
                                                 vpx_codec_pts_t        pts,
                                                 unsigned long          duration,
                                                 vpx_enc_frame_flags_t  flags,
                                                 unsigned long          deadline);
typedef const vpx_codec_cx_pkt_t *(*vpx_codec_get_cx_data_fn_t)(vpx_codec_alg_priv_t *ctx,
                                                                vpx_codec_iter_t     *iter);

typedef vpx_codec_err_t
(*vpx_codec_enc_config_set_fn_t)(vpx_codec_alg_priv_t       *ctx,
                                 const vpx_codec_enc_cfg_t  *cfg);
typedef vpx_fixed_buf_t *
(*vpx_codec_get_global_headers_fn_t)(vpx_codec_alg_priv_t   *ctx);

typedef vpx_image_t *
(*vpx_codec_get_preview_frame_fn_t)(vpx_codec_alg_priv_t   *ctx);

typedef vpx_codec_err_t
(*vpx_codec_enc_mr_get_mem_loc_fn_t)(const vpx_codec_enc_cfg_t     *cfg,
                                     void **mem_loc);

typedef const struct vpx_codec_enc_cfg_map {
  int                 usage;
  vpx_codec_enc_cfg_t cfg;
} vpx_codec_enc_cfg_map_t;

struct vpx_codec_iface {
  const char               *name;        
  int                       abi_version; 
  vpx_codec_caps_t          caps;    
  vpx_codec_init_fn_t       init;    
  vpx_codec_destroy_fn_t    destroy;     
  vpx_codec_ctrl_fn_map_t  *ctrl_maps;   
  struct vpx_codec_dec_iface {
    vpx_codec_peek_si_fn_t    peek_si;     
    vpx_codec_get_si_fn_t     get_si;      
    vpx_codec_decode_fn_t     decode;      
    vpx_codec_get_frame_fn_t  get_frame;   
    vpx_codec_set_fb_fn_t     set_fb_fn;   
  } dec;
  struct vpx_codec_enc_iface {
    int                                cfg_map_count;
    vpx_codec_enc_cfg_map_t           *cfg_maps;      
    vpx_codec_encode_fn_t              encode;        
    vpx_codec_get_cx_data_fn_t         get_cx_data;   
    vpx_codec_enc_config_set_fn_t      cfg_set;       
    vpx_codec_get_global_headers_fn_t  get_glob_hdrs; 
    vpx_codec_get_preview_frame_fn_t   get_preview;   
    vpx_codec_enc_mr_get_mem_loc_fn_t  mr_get_mem_loc;   
  } enc;
};

typedef struct vpx_codec_priv_cb_pair {
  union {
    vpx_codec_put_frame_cb_fn_t    put_frame;
    vpx_codec_put_slice_cb_fn_t    put_slice;
  } u;
  void                            *user_priv;
} vpx_codec_priv_cb_pair_t;


struct vpx_codec_priv {
  const char                     *err_detail;
  vpx_codec_flags_t               init_flags;
  struct {
    vpx_codec_priv_cb_pair_t    put_frame_cb;
    vpx_codec_priv_cb_pair_t    put_slice_cb;
  } dec;
  struct {
    vpx_fixed_buf_t             cx_data_dst_buf;
    unsigned int                cx_data_pad_before;
    unsigned int                cx_data_pad_after;
    vpx_codec_cx_pkt_t          cx_data_pkt;
    unsigned int                total_encoders;
  } enc;
};

struct vpx_codec_priv_enc_mr_cfg
{
    unsigned int           mr_total_resolutions;
    unsigned int           mr_encoder_id;
    struct vpx_rational    mr_down_sampling_factor;
    void*                  mr_low_res_mode_info;
};

#undef VPX_CTRL_USE_TYPE
#define VPX_CTRL_USE_TYPE(id, typ) \
  static VPX_INLINE typ id##__value(va_list args) {return va_arg(args, typ);}

#undef VPX_CTRL_USE_TYPE_DEPRECATED
#define VPX_CTRL_USE_TYPE_DEPRECATED(id, typ) \
  static VPX_INLINE typ id##__value(va_list args) {return va_arg(args, typ);}

#define CAST(id, arg) id##__value(arg)

#define CODEC_INTERFACE(id)\
  vpx_codec_iface_t* id(void) { return &id##_algo; }\
  vpx_codec_iface_t  id##_algo


struct vpx_codec_pkt_list {
  unsigned int            cnt;
  unsigned int            max;
  struct vpx_codec_cx_pkt pkts[1];
};

#define vpx_codec_pkt_list_decl(n)\
  union {struct vpx_codec_pkt_list head;\
    struct {struct vpx_codec_pkt_list head;\
      struct vpx_codec_cx_pkt    pkts[n];} alloc;}

#define vpx_codec_pkt_list_init(m)\
  (m)->alloc.head.cnt = 0,\
                        (m)->alloc.head.max = sizeof((m)->alloc.pkts) / sizeof((m)->alloc.pkts[0])

int
vpx_codec_pkt_list_add(struct vpx_codec_pkt_list *,
                       const struct vpx_codec_cx_pkt *);

const vpx_codec_cx_pkt_t *
vpx_codec_pkt_list_get(struct vpx_codec_pkt_list *list,
                       vpx_codec_iter_t           *iter);


#include <stdio.h>
#include <setjmp.h>

struct vpx_internal_error_info {
  vpx_codec_err_t  error_code;
  int              has_detail;
  char             detail[80];
  int              setjmp;
  jmp_buf          jmp;
};

#define CLANG_ANALYZER_NORETURN
#if defined(__has_feature)
#if __has_feature(attribute_analyzer_noreturn)
#undef CLANG_ANALYZER_NORETURN
#define CLANG_ANALYZER_NORETURN __attribute__((analyzer_noreturn))
#endif
#endif

void vpx_internal_error(struct vpx_internal_error_info *info,
                        vpx_codec_err_t                 error,
                        const char                     *fmt,
                        ...) CLANG_ANALYZER_NORETURN;

#ifdef __cplusplus
}  
#endif

#endif  

/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_ENCODE_TEST_DRIVER_H_
#define TEST_ENCODE_TEST_DRIVER_H_

#include <string>
#include <vector>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#if CONFIG_VP8_ENCODER || CONFIG_VP9_ENCODER || CONFIG_VP10_ENCODER
#include "vpx/vp8cx.h"
#endif
#include "vpx/vpx_encoder.h"

namespace libvpx_test {

class CodecFactory;
class VideoSource;

enum TestMode {
  kRealTime,
  kOnePassGood,
  kOnePassBest,
  kTwoPassGood,
  kTwoPassBest
};
#define ALL_TEST_MODES ::testing::Values(::libvpx_test::kRealTime, \
                                         ::libvpx_test::kOnePassGood, \
                                         ::libvpx_test::kOnePassBest, \
                                         ::libvpx_test::kTwoPassGood, \
                                         ::libvpx_test::kTwoPassBest)

#define ONE_PASS_TEST_MODES ::testing::Values(::libvpx_test::kRealTime, \
                                              ::libvpx_test::kOnePassGood, \
                                              ::libvpx_test::kOnePassBest)

#define TWO_PASS_TEST_MODES ::testing::Values(::libvpx_test::kTwoPassGood, \
                                              ::libvpx_test::kTwoPassBest)


class CxDataIterator {
 public:
  explicit CxDataIterator(vpx_codec_ctx_t *encoder)
      : encoder_(encoder), iter_(NULL) {}

  const vpx_codec_cx_pkt_t *Next() {
    return vpx_codec_get_cx_data(encoder_, &iter_);
  }

 private:
  vpx_codec_ctx_t  *encoder_;
  vpx_codec_iter_t  iter_;
};

class TwopassStatsStore {
 public:
  void Append(const vpx_codec_cx_pkt_t &pkt) {
    buffer_.append(reinterpret_cast<char *>(pkt.data.twopass_stats.buf),
                   pkt.data.twopass_stats.sz);
  }

  vpx_fixed_buf_t buf() {
    const vpx_fixed_buf_t buf = { &buffer_[0], buffer_.size() };
    return buf;
  }

  void Reset() {
    buffer_.clear();
  }

 protected:
  std::string  buffer_;
};


// level of abstraction will be fleshed out as more tests are written.
class Encoder {
 public:
  Encoder(vpx_codec_enc_cfg_t cfg, unsigned long deadline,
          const unsigned long init_flags, TwopassStatsStore *stats)
      : cfg_(cfg), deadline_(deadline), init_flags_(init_flags), stats_(stats) {
    memset(&encoder_, 0, sizeof(encoder_));
  }

  virtual ~Encoder() {
    vpx_codec_destroy(&encoder_);
  }

  CxDataIterator GetCxData() {
    return CxDataIterator(&encoder_);
  }

  void InitEncoder(VideoSource *video);

  const vpx_image_t *GetPreviewFrame() {
    return vpx_codec_get_preview_frame(&encoder_);
  }
  
  
  void EncodeFrame(VideoSource *video, const unsigned long frame_flags);

  
  void EncodeFrame(VideoSource *video) {
    EncodeFrame(video, 0);
  }

  void Control(int ctrl_id, int arg) {
    const vpx_codec_err_t res = vpx_codec_control_(&encoder_, ctrl_id, arg);
    ASSERT_EQ(VPX_CODEC_OK, res) << EncoderError();
  }

  void Control(int ctrl_id, struct vpx_scaling_mode *arg) {
    const vpx_codec_err_t res = vpx_codec_control_(&encoder_, ctrl_id, arg);
    ASSERT_EQ(VPX_CODEC_OK, res) << EncoderError();
  }

  void Control(int ctrl_id, struct vpx_svc_layer_id *arg) {
    const vpx_codec_err_t res = vpx_codec_control_(&encoder_, ctrl_id, arg);
    ASSERT_EQ(VPX_CODEC_OK, res) << EncoderError();
  }

  void Control(int ctrl_id, struct vpx_svc_parameters *arg) {
    const vpx_codec_err_t res = vpx_codec_control_(&encoder_, ctrl_id, arg);
    ASSERT_EQ(VPX_CODEC_OK, res) << EncoderError();
  }
#if CONFIG_VP8_ENCODER || CONFIG_VP9_ENCODER || CONFIG_VP10_ENCODER
  void Control(int ctrl_id, vpx_active_map_t *arg) {
    const vpx_codec_err_t res = vpx_codec_control_(&encoder_, ctrl_id, arg);
    ASSERT_EQ(VPX_CODEC_OK, res) << EncoderError();
  }
#endif

  void Config(const vpx_codec_enc_cfg_t *cfg) {
    const vpx_codec_err_t res = vpx_codec_enc_config_set(&encoder_, cfg);
    ASSERT_EQ(VPX_CODEC_OK, res) << EncoderError();
    cfg_ = *cfg;
  }

  void set_deadline(unsigned long deadline) {
    deadline_ = deadline;
  }

 protected:
  virtual vpx_codec_iface_t* CodecInterface() const = 0;

  const char *EncoderError() {
    const char *detail = vpx_codec_error_detail(&encoder_);
    return detail ? detail : vpx_codec_error(&encoder_);
  }

  
  void EncodeFrameInternal(const VideoSource &video,
                           const unsigned long frame_flags);

  
  void Flush();

  vpx_codec_ctx_t      encoder_;
  vpx_codec_enc_cfg_t  cfg_;
  unsigned long        deadline_;
  unsigned long        init_flags_;
  TwopassStatsStore   *stats_;
};

class EncoderTest {
 protected:
  explicit EncoderTest(const CodecFactory *codec)
      : codec_(codec), abort_(false), init_flags_(0), frame_flags_(0),
        last_pts_(0) {
    
    cfg_.g_threads = 1;
  }

  virtual ~EncoderTest() {}

  
  void InitializeConfig();

  
  void SetMode(TestMode mode);

  
  void set_init_flags(unsigned long flag) {  
    init_flags_ = flag;
  }

  
  virtual void RunLoop(VideoSource *video);

  
  virtual void BeginPassHook(unsigned int ) {}

  
  virtual void EndPassHook() {}

  
  virtual void PreEncodeFrameHook(VideoSource* ) {}
  virtual void PreEncodeFrameHook(VideoSource* ,
                                  Encoder* ) {}

  
  virtual void FramePktHook(const vpx_codec_cx_pkt_t* ) {}

  
  virtual void PSNRPktHook(const vpx_codec_cx_pkt_t* ) {}

  
  virtual bool Continue() const {
    return !(::testing::Test::HasFatalFailure() || abort_);
  }

  const CodecFactory   *codec_;
  
  virtual bool DoDecode() const { return 1; }

  
  virtual void MismatchHook(const vpx_image_t *img1,
                            const vpx_image_t *img2);

  
  virtual void DecompressedFrameHook(const vpx_image_t& ,
                                     vpx_codec_pts_t ) {}

  
  virtual bool HandleDecodeResult(const vpx_codec_err_t res_dec,
                                  const VideoSource& ,
                                  Decoder *decoder) {
    EXPECT_EQ(VPX_CODEC_OK, res_dec) << decoder->DecodeError();
    return VPX_CODEC_OK == res_dec;
  }

  
  virtual const vpx_codec_cx_pkt_t *MutateEncoderOutputHook(
      const vpx_codec_cx_pkt_t *pkt) {
    return pkt;
  }

  bool                 abort_;
  vpx_codec_enc_cfg_t  cfg_;
  vpx_codec_dec_cfg_t  dec_cfg_;
  unsigned int         passes_;
  unsigned long        deadline_;
  TwopassStatsStore    stats_;
  unsigned long        init_flags_;
  unsigned long        frame_flags_;
  vpx_codec_pts_t      last_pts_;
};

}  

#endif  

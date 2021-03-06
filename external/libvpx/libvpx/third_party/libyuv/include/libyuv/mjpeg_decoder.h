/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_MJPEG_DECODER_H_  
#define INCLUDE_LIBYUV_MJPEG_DECODER_H_

#include "libyuv/basic_types.h"

#ifdef __cplusplus

struct jpeg_common_struct;
struct jpeg_decompress_struct;
struct jpeg_source_mgr;

namespace libyuv {

#ifdef __cplusplus
extern "C" {
#endif

LIBYUV_BOOL ValidateJpeg(const uint8* sample, size_t sample_size);

#ifdef __cplusplus
}  
#endif

static const uint32 kUnknownDataSize = 0xFFFFFFFF;

enum JpegSubsamplingType {
  kJpegYuv420,
  kJpegYuv422,
  kJpegYuv411,
  kJpegYuv444,
  kJpegYuv400,
  kJpegUnknown
};

struct Buffer {
  const uint8* data;
  int len;
};

struct BufferVector {
  Buffer* buffers;
  int len;
  int pos;
};

struct SetJmpErrorMgr;

class LIBYUV_API MJpegDecoder {
 public:
  typedef void (*CallbackFunction)(void* opaque,
                                   const uint8* const* data,
                                   const int* strides,
                                   int rows);

  static const int kColorSpaceUnknown;
  static const int kColorSpaceGrayscale;
  static const int kColorSpaceRgb;
  static const int kColorSpaceYCbCr;
  static const int kColorSpaceCMYK;
  static const int kColorSpaceYCCK;

  MJpegDecoder();
  ~MJpegDecoder();

  
  
  
  
  
  
  LIBYUV_BOOL LoadFrame(const uint8* src, size_t src_len);

  
  int GetWidth();

  
  int GetHeight();

  
  
  int GetColorSpace();

  
  int GetNumComponents();

  
  int GetHorizSampFactor(int component);

  int GetVertSampFactor(int component);

  int GetHorizSubSampFactor(int component);

  int GetVertSubSampFactor(int component);

  
  int GetImageScanlinesPerImcuRow();

  
  int GetComponentScanlinesPerImcuRow(int component);

  
  int GetComponentWidth(int component);

  
  int GetComponentHeight(int component);

  
  int GetComponentStride(int component);

  
  int GetComponentSize(int component);

  
  
  LIBYUV_BOOL UnloadFrame();

  
  
  
  
  
  // to point to after the end of the written data.
  
  LIBYUV_BOOL DecodeToBuffers(uint8** planes, int dst_width, int dst_height);

  
  
  
  
  LIBYUV_BOOL DecodeToCallback(CallbackFunction fn, void* opaque,
                        int dst_width, int dst_height);

  
  static JpegSubsamplingType JpegSubsamplingTypeHelper(
     int* subsample_x, int* subsample_y, int number_of_components);

 private:
  void AllocOutputBuffers(int num_outbufs);
  void DestroyOutputBuffers();

  LIBYUV_BOOL StartDecode();
  LIBYUV_BOOL FinishDecode();

  void SetScanlinePointers(uint8** data);
  LIBYUV_BOOL DecodeImcuRow();

  int GetComponentScanlinePadding(int component);

  
  Buffer buf_;
  BufferVector buf_vec_;

  jpeg_decompress_struct* decompress_struct_;
  jpeg_source_mgr* source_mgr_;
  SetJmpErrorMgr* error_mgr_;

  
  
  LIBYUV_BOOL has_scanline_padding_;

  
  int num_outbufs_;  
  uint8*** scanlines_;
  int* scanlines_sizes_;
  
  
  uint8** databuf_;
  int* databuf_strides_;
};

}  

#endif  
#endif  

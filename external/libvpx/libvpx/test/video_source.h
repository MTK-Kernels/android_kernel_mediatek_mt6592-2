/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_VIDEO_SOURCE_H_
#define TEST_VIDEO_SOURCE_H_

#if defined(_WIN32)
#include <windows.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <string>
#include "test/acm_random.h"
#include "vpx/vpx_encoder.h"

namespace libvpx_test {

#define TO_STRING(S) #S
#define STRINGIFY(S) TO_STRING(S)

static std::string GetDataPath() {
  const char *const data_path = getenv("LIBVPX_TEST_DATA_PATH");
  if (data_path == NULL) {
#ifdef LIBVPX_TEST_DATA_PATH
    
    
    
    return STRINGIFY(LIBVPX_TEST_DATA_PATH);
#else
    return ".";
#endif
  }
  return data_path;
}

#undef TO_STRING
#undef STRINGIFY

static FILE *OpenTestDataFile(const std::string& file_name) {
  const std::string path_to_source = GetDataPath() + "/" + file_name;
  return fopen(path_to_source.c_str(), "rb");
}

static FILE *GetTempOutFile(std::string *file_name) {
  file_name->clear();
#if defined(_WIN32)
  char fname[MAX_PATH];
  char tmppath[MAX_PATH];
  if (GetTempPathA(MAX_PATH, tmppath)) {
    
    if (GetTempFileNameA(tmppath, "lvx", 0, fname)) {
      file_name->assign(fname);
      return fopen(fname, "wb+");
    }
  }
  return NULL;
#else
  return tmpfile();
#endif
}

class TempOutFile {
 public:
  TempOutFile() {
    file_ = GetTempOutFile(&file_name_);
  }
  ~TempOutFile() {
    CloseFile();
    if (!file_name_.empty()) {
      EXPECT_EQ(0, remove(file_name_.c_str()));
    }
  }
  FILE *file() {
    return file_;
  }
  const std::string& file_name() {
    return file_name_;
  }

 protected:
  void CloseFile() {
    if (file_) {
      fclose(file_);
      file_ = NULL;
    }
  }
  FILE *file_;
  std::string file_name_;
};

class VideoSource {
 public:
  virtual ~VideoSource() {}

  
  virtual void Begin() = 0;

  
  virtual void Next() = 0;

  
  virtual vpx_image_t *img() const = 0;

  
  virtual vpx_codec_pts_t pts() const = 0;

  
  virtual unsigned long duration() const = 0;

  
  virtual vpx_rational_t timebase() const = 0;

  
  virtual unsigned int frame() const = 0;

  
  virtual unsigned int limit() const = 0;
};


class DummyVideoSource : public VideoSource {
 public:
  DummyVideoSource()
      : img_(NULL),
        limit_(100),
        width_(80),
        height_(64),
        format_(VPX_IMG_FMT_I420) {
    ReallocImage();
  }

  virtual ~DummyVideoSource() { vpx_img_free(img_); }

  virtual void Begin() {
    frame_ = 0;
    FillFrame();
  }

  virtual void Next() {
    ++frame_;
    FillFrame();
  }

  virtual vpx_image_t *img() const {
    return (frame_ < limit_) ? img_ : NULL;
  }

  
  virtual vpx_codec_pts_t pts() const { return frame_; }

  virtual unsigned long duration() const { return 1; }

  virtual vpx_rational_t timebase() const {
    const vpx_rational_t t = {1, 30};
    return t;
  }

  virtual unsigned int frame() const { return frame_; }

  virtual unsigned int limit() const { return limit_; }

  void set_limit(unsigned int limit) {
    limit_ = limit;
  }

  void SetSize(unsigned int width, unsigned int height) {
    if (width != width_ || height != height_) {
      width_ = width;
      height_ = height;
      ReallocImage();
    }
  }

  void SetImageFormat(vpx_img_fmt_t format) {
    if (format_ != format) {
      format_ = format;
      ReallocImage();
    }
  }

 protected:
  virtual void FillFrame() { if (img_) memset(img_->img_data, 0, raw_sz_); }

  void ReallocImage() {
    vpx_img_free(img_);
    img_ = vpx_img_alloc(NULL, format_, width_, height_, 32);
    raw_sz_ = ((img_->w + 31) & ~31) * img_->h * img_->bps / 8;
  }

  vpx_image_t *img_;
  size_t       raw_sz_;
  unsigned int limit_;
  unsigned int frame_;
  unsigned int width_;
  unsigned int height_;
  vpx_img_fmt_t format_;
};


class RandomVideoSource : public DummyVideoSource {
 public:
  RandomVideoSource(int seed = ACMRandom::DeterministicSeed())
      : rnd_(seed),
        seed_(seed) { }

 protected:
  
  virtual void Begin() {
    frame_ = 0;
    rnd_.Reset(seed_);
    FillFrame();
  }

  
  
  virtual void FillFrame() {
    if (img_) {
      if (frame_ % 30 < 15)
        for (size_t i = 0; i < raw_sz_; ++i)
          img_->img_data[i] = rnd_.Rand8();
      else
        memset(img_->img_data, 0, raw_sz_);
    }
  }

  ACMRandom rnd_;
  int seed_;
};

class CompressedVideoSource {
 public:
  virtual ~CompressedVideoSource() {}

  virtual void Init() = 0;

  
  virtual void Begin() = 0;

  
  virtual void Next() = 0;

  virtual const uint8_t *cxdata() const = 0;

  virtual size_t frame_size() const = 0;

  virtual unsigned int frame_number() const = 0;
};

}  

#endif  

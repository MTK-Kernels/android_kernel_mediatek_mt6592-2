/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Based on code from the OggTheora software codec source code,
 *  Copyright (C) 2002-2010 The Xiph.Org Foundation and contributors.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vpx/vpx_integer.h"
#include "y4minput.h"

static int file_read(void *buf, size_t size, FILE *file) {
  const int kMaxRetries = 5;
  int retry_count = 0;
  int file_error;
  size_t len = 0;
  do {
    const size_t n = fread((uint8_t*)buf + len, 1, size - len, file);
    len += n;
    file_error = ferror(file);
    if (file_error) {
      if (errno == EINTR || errno == EAGAIN) {
        clearerr(file);
        continue;
      } else {
        fprintf(stderr, "Error reading file: %u of %u bytes read, %d: %s\n",
                (uint32_t)len, (uint32_t)size, errno, strerror(errno));
        return 0;
      }
    }
  } while (!feof(file) && len < size && ++retry_count < kMaxRetries);

  if (!feof(file) && len != size) {
    fprintf(stderr, "Error reading file: %u of %u bytes read,"
                    " error: %d, retries: %d, %d: %s\n",
            (uint32_t)len, (uint32_t)size, file_error, retry_count,
            errno, strerror(errno));
  }
  return len == size;
}

static int y4m_parse_tags(y4m_input *_y4m, char *_tags) {
  int   got_w;
  int   got_h;
  int   got_fps;
  int   got_interlace;
  int   got_par;
  int   got_chroma;
  char *p;
  char *q;
  got_w = got_h = got_fps = got_interlace = got_par = got_chroma = 0;
  for (p = _tags;; p = q) {
    
    while (*p == ' ')p++;
    
    if (p[0] == '\0')break;
    
    for (q = p + 1; *q != '\0' && *q != ' '; q++);
    
    switch (p[0]) {
      case 'W': {
        if (sscanf(p + 1, "%d", &_y4m->pic_w) != 1)return -1;
        got_w = 1;
      }
      break;
      case 'H': {
        if (sscanf(p + 1, "%d", &_y4m->pic_h) != 1)return -1;
        got_h = 1;
      }
      break;
      case 'F': {
        if (sscanf(p + 1, "%d:%d", &_y4m->fps_n, &_y4m->fps_d) != 2) {
          return -1;
        }
        got_fps = 1;
      }
      break;
      case 'I': {
        _y4m->interlace = p[1];
        got_interlace = 1;
      }
      break;
      case 'A': {
        if (sscanf(p + 1, "%d:%d", &_y4m->par_n, &_y4m->par_d) != 2) {
          return -1;
        }
        got_par = 1;
      }
      break;
      case 'C': {
        if (q - p > 16)return -1;
        memcpy(_y4m->chroma_type, p + 1, q - p - 1);
        _y4m->chroma_type[q - p - 1] = '\0';
        got_chroma = 1;
      }
      break;
      
    }
  }
  if (!got_w || !got_h || !got_fps)return -1;
  if (!got_interlace)_y4m->interlace = '?';
  if (!got_par)_y4m->par_n = _y4m->par_d = 0;
  if (!got_chroma)strcpy(_y4m->chroma_type, "420");
  return 0;
}




#define OC_MINI(_a,_b)      ((_a)>(_b)?(_b):(_a))
#define OC_MAXI(_a,_b)      ((_a)<(_b)?(_b):(_a))
#define OC_CLAMPI(_a,_b,_c) (OC_MAXI(_a,OC_MINI(_b,_c)))

static void y4m_42xmpeg2_42xjpeg_helper(unsigned char *_dst,
                                        const unsigned char *_src, int _c_w, int _c_h) {
  int y;
  int x;
  for (y = 0; y < _c_h; y++) {
    for (x = 0; x < OC_MINI(_c_w, 2); x++) {
      _dst[x] = (unsigned char)OC_CLAMPI(0, (4 * _src[0] - 17 * _src[OC_MAXI(x - 1, 0)] +
                                             114 * _src[x] + 35 * _src[OC_MINI(x + 1, _c_w - 1)] - 9 * _src[OC_MINI(x + 2, _c_w - 1)] +
                                             _src[OC_MINI(x + 3, _c_w - 1)] + 64) >> 7, 255);
    }
    for (; x < _c_w - 3; x++) {
      _dst[x] = (unsigned char)OC_CLAMPI(0, (4 * _src[x - 2] - 17 * _src[x - 1] +
                                             114 * _src[x] + 35 * _src[x + 1] - 9 * _src[x + 2] + _src[x + 3] + 64) >> 7, 255);
    }
    for (; x < _c_w; x++) {
      _dst[x] = (unsigned char)OC_CLAMPI(0, (4 * _src[x - 2] - 17 * _src[x - 1] +
                                             114 * _src[x] + 35 * _src[OC_MINI(x + 1, _c_w - 1)] - 9 * _src[OC_MINI(x + 2, _c_w - 1)] +
                                             _src[_c_w - 1] + 64) >> 7, 255);
    }
    _dst += _c_w;
    _src += _c_w;
  }
}

static void y4m_convert_42xmpeg2_42xjpeg(y4m_input *_y4m, unsigned char *_dst,
                                         unsigned char *_aux) {
  int c_w;
  int c_h;
  int c_sz;
  int pli;
  
  _dst += _y4m->pic_w * _y4m->pic_h;
  
  c_w = (_y4m->pic_w + _y4m->dst_c_dec_h - 1) / _y4m->dst_c_dec_h;
  c_h = (_y4m->pic_h + _y4m->dst_c_dec_v - 1) / _y4m->dst_c_dec_v;
  c_sz = c_w * c_h;
  for (pli = 1; pli < 3; pli++) {
    y4m_42xmpeg2_42xjpeg_helper(_dst, _aux, c_w, c_h);
    _dst += c_sz;
    _aux += c_sz;
  }
}

static void y4m_convert_42xpaldv_42xjpeg(y4m_input *_y4m, unsigned char *_dst,
                                         unsigned char *_aux) {
  unsigned char *tmp;
  int            c_w;
  int            c_h;
  int            c_sz;
  int            pli;
  int            y;
  int            x;
  
  _dst += _y4m->pic_w * _y4m->pic_h;
  
  c_w = (_y4m->pic_w + 1) / 2;
  c_h = (_y4m->pic_h + _y4m->dst_c_dec_h - 1) / _y4m->dst_c_dec_h;
  c_sz = c_w * c_h;
  tmp = _aux + 2 * c_sz;
  for (pli = 1; pli < 3; pli++) {
    y4m_42xmpeg2_42xjpeg_helper(tmp, _aux, c_w, c_h);
    _aux += c_sz;
    switch (pli) {
      case 1: {
        for (x = 0; x < c_w; x++) {
          for (y = 0; y < OC_MINI(c_h, 3); y++) {
            _dst[y * c_w] = (unsigned char)OC_CLAMPI(0, (tmp[0]
                                                         - 9 * tmp[OC_MAXI(y - 2, 0) * c_w] + 35 * tmp[OC_MAXI(y - 1, 0) * c_w]
                                                         + 114 * tmp[y * c_w] - 17 * tmp[OC_MINI(y + 1, c_h - 1) * c_w]
                                                         + 4 * tmp[OC_MINI(y + 2, c_h - 1) * c_w] + 64) >> 7, 255);
          }
          for (; y < c_h - 2; y++) {
            _dst[y * c_w] = (unsigned char)OC_CLAMPI(0, (tmp[(y - 3) * c_w]
                                                         - 9 * tmp[(y - 2) * c_w] + 35 * tmp[(y - 1) * c_w] + 114 * tmp[y * c_w]
                                                         - 17 * tmp[(y + 1) * c_w] + 4 * tmp[(y + 2) * c_w] + 64) >> 7, 255);
          }
          for (; y < c_h; y++) {
            _dst[y * c_w] = (unsigned char)OC_CLAMPI(0, (tmp[(y - 3) * c_w]
                                                         - 9 * tmp[(y - 2) * c_w] + 35 * tmp[(y - 1) * c_w] + 114 * tmp[y * c_w]
                                                         - 17 * tmp[OC_MINI(y + 1, c_h - 1) * c_w] + 4 * tmp[(c_h - 1) * c_w] + 64) >> 7, 255);
          }
          _dst++;
          tmp++;
        }
        _dst += c_sz - c_w;
        tmp -= c_w;
      }
      break;
      case 2: {
        for (x = 0; x < c_w; x++) {
          for (y = 0; y < OC_MINI(c_h, 2); y++) {
            _dst[y * c_w] = (unsigned char)OC_CLAMPI(0, (4 * tmp[0]
                                                         - 17 * tmp[OC_MAXI(y - 1, 0) * c_w] + 114 * tmp[y * c_w]
                                                         + 35 * tmp[OC_MINI(y + 1, c_h - 1) * c_w] - 9 * tmp[OC_MINI(y + 2, c_h - 1) * c_w]
                                                         + tmp[OC_MINI(y + 3, c_h - 1) * c_w] + 64) >> 7, 255);
          }
          for (; y < c_h - 3; y++) {
            _dst[y * c_w] = (unsigned char)OC_CLAMPI(0, (4 * tmp[(y - 2) * c_w]
                                                         - 17 * tmp[(y - 1) * c_w] + 114 * tmp[y * c_w] + 35 * tmp[(y + 1) * c_w]
                                                         - 9 * tmp[(y + 2) * c_w] + tmp[(y + 3) * c_w] + 64) >> 7, 255);
          }
          for (; y < c_h; y++) {
            _dst[y * c_w] = (unsigned char)OC_CLAMPI(0, (4 * tmp[(y - 2) * c_w]
                                                         - 17 * tmp[(y - 1) * c_w] + 114 * tmp[y * c_w] + 35 * tmp[OC_MINI(y + 1, c_h - 1) * c_w]
                                                         - 9 * tmp[OC_MINI(y + 2, c_h - 1) * c_w] + tmp[(c_h - 1) * c_w] + 64) >> 7, 255);
          }
          _dst++;
          tmp++;
        }
      }
      break;
    }
  }
}

static void y4m_422jpeg_420jpeg_helper(unsigned char *_dst,
                                       const unsigned char *_src, int _c_w, int _c_h) {
  int y;
  int x;
  
  for (x = 0; x < _c_w; x++) {
    for (y = 0; y < OC_MINI(_c_h, 2); y += 2) {
      _dst[(y >> 1)*_c_w] = OC_CLAMPI(0, (64 * _src[0]
                                          + 78 * _src[OC_MINI(1, _c_h - 1) * _c_w]
                                          - 17 * _src[OC_MINI(2, _c_h - 1) * _c_w]
                                          + 3 * _src[OC_MINI(3, _c_h - 1) * _c_w] + 64) >> 7, 255);
    }
    for (; y < _c_h - 3; y += 2) {
      _dst[(y >> 1)*_c_w] = OC_CLAMPI(0, (3 * (_src[(y - 2) * _c_w] + _src[(y + 3) * _c_w])
                                          - 17 * (_src[(y - 1) * _c_w] + _src[(y + 2) * _c_w])
                                          + 78 * (_src[y * _c_w] + _src[(y + 1) * _c_w]) + 64) >> 7, 255);
    }
    for (; y < _c_h; y += 2) {
      _dst[(y >> 1)*_c_w] = OC_CLAMPI(0, (3 * (_src[(y - 2) * _c_w]
                                               + _src[(_c_h - 1) * _c_w]) - 17 * (_src[(y - 1) * _c_w]
                                                                                  + _src[OC_MINI(y + 2, _c_h - 1) * _c_w])
                                          + 78 * (_src[y * _c_w] + _src[OC_MINI(y + 1, _c_h - 1) * _c_w]) + 64) >> 7, 255);
    }
    _src++;
    _dst++;
  }
}

static void y4m_convert_422jpeg_420jpeg(y4m_input *_y4m, unsigned char *_dst,
                                        unsigned char *_aux) {
  int c_w;
  int c_h;
  int c_sz;
  int dst_c_w;
  int dst_c_h;
  int dst_c_sz;
  int pli;
  
  _dst += _y4m->pic_w * _y4m->pic_h;
  
  c_w = (_y4m->pic_w + _y4m->src_c_dec_h - 1) / _y4m->src_c_dec_h;
  c_h = _y4m->pic_h;
  dst_c_w = (_y4m->pic_w + _y4m->dst_c_dec_h - 1) / _y4m->dst_c_dec_h;
  dst_c_h = (_y4m->pic_h + _y4m->dst_c_dec_v - 1) / _y4m->dst_c_dec_v;
  c_sz = c_w * c_h;
  dst_c_sz = dst_c_w * dst_c_h;
  for (pli = 1; pli < 3; pli++) {
    y4m_422jpeg_420jpeg_helper(_dst, _aux, c_w, c_h);
    _aux += c_sz;
    _dst += dst_c_sz;
  }
}

static void y4m_convert_422_420jpeg(y4m_input *_y4m, unsigned char *_dst,
                                    unsigned char *_aux) {
  unsigned char *tmp;
  int            c_w;
  int            c_h;
  int            c_sz;
  int            dst_c_h;
  int            dst_c_sz;
  int            pli;
  
  _dst += _y4m->pic_w * _y4m->pic_h;
  
  c_w = (_y4m->pic_w + _y4m->src_c_dec_h - 1) / _y4m->src_c_dec_h;
  c_h = _y4m->pic_h;
  dst_c_h = (_y4m->pic_h + _y4m->dst_c_dec_v - 1) / _y4m->dst_c_dec_v;
  c_sz = c_w * c_h;
  dst_c_sz = c_w * dst_c_h;
  tmp = _aux + 2 * c_sz;
  for (pli = 1; pli < 3; pli++) {
    
    y4m_42xmpeg2_42xjpeg_helper(tmp, _aux, c_w, c_h);
    
    y4m_422jpeg_420jpeg_helper(_dst, tmp, c_w, c_h);
    _aux += c_sz;
    _dst += dst_c_sz;
  }
}

static void y4m_convert_411_420jpeg(y4m_input *_y4m, unsigned char *_dst,
                                    unsigned char *_aux) {
  unsigned char *tmp;
  int            c_w;
  int            c_h;
  int            c_sz;
  int            dst_c_w;
  int            dst_c_h;
  int            dst_c_sz;
  int            tmp_sz;
  int            pli;
  int            y;
  int            x;
  
  _dst += _y4m->pic_w * _y4m->pic_h;
  
  c_w = (_y4m->pic_w + _y4m->src_c_dec_h - 1) / _y4m->src_c_dec_h;
  c_h = _y4m->pic_h;
  dst_c_w = (_y4m->pic_w + _y4m->dst_c_dec_h - 1) / _y4m->dst_c_dec_h;
  dst_c_h = (_y4m->pic_h + _y4m->dst_c_dec_v - 1) / _y4m->dst_c_dec_v;
  c_sz = c_w * c_h;
  dst_c_sz = dst_c_w * dst_c_h;
  tmp_sz = dst_c_w * c_h;
  tmp = _aux + 2 * c_sz;
  for (pli = 1; pli < 3; pli++) {
    
    for (y = 0; y < c_h; y++) {
      for (x = 0; x < OC_MINI(c_w, 1); x++) {
        tmp[x << 1] = (unsigned char)OC_CLAMPI(0, (111 * _aux[0]
                                                   + 18 * _aux[OC_MINI(1, c_w - 1)] - _aux[OC_MINI(2, c_w - 1)] + 64) >> 7, 255);
        tmp[x << 1 | 1] = (unsigned char)OC_CLAMPI(0, (47 * _aux[0]
                                                       + 86 * _aux[OC_MINI(1, c_w - 1)] - 5 * _aux[OC_MINI(2, c_w - 1)] + 64) >> 7, 255);
      }
      for (; x < c_w - 2; x++) {
        tmp[x << 1] = (unsigned char)OC_CLAMPI(0, (_aux[x - 1] + 110 * _aux[x]
                                                   + 18 * _aux[x + 1] - _aux[x + 2] + 64) >> 7, 255);
        tmp[x << 1 | 1] = (unsigned char)OC_CLAMPI(0, (-3 * _aux[x - 1] + 50 * _aux[x]
                                                       + 86 * _aux[x + 1] - 5 * _aux[x + 2] + 64) >> 7, 255);
      }
      for (; x < c_w; x++) {
        tmp[x << 1] = (unsigned char)OC_CLAMPI(0, (_aux[x - 1] + 110 * _aux[x]
                                                   + 18 * _aux[OC_MINI(x + 1, c_w - 1)] - _aux[c_w - 1] + 64) >> 7, 255);
        if ((x << 1 | 1) < dst_c_w) {
          tmp[x << 1 | 1] = (unsigned char)OC_CLAMPI(0, (-3 * _aux[x - 1] + 50 * _aux[x]
                                                         + 86 * _aux[OC_MINI(x + 1, c_w - 1)] - 5 * _aux[c_w - 1] + 64) >> 7, 255);
        }
      }
      tmp += dst_c_w;
      _aux += c_w;
    }
    tmp -= tmp_sz;
    
    y4m_422jpeg_420jpeg_helper(_dst, tmp, dst_c_w, c_h);
    _dst += dst_c_sz;
  }
}

static void y4m_convert_444_420jpeg(y4m_input *_y4m, unsigned char *_dst,
                                    unsigned char *_aux) {
  unsigned char *tmp;
  int            c_w;
  int            c_h;
  int            c_sz;
  int            dst_c_w;
  int            dst_c_h;
  int            dst_c_sz;
  int            tmp_sz;
  int            pli;
  int            y;
  int            x;
  
  _dst += _y4m->pic_w * _y4m->pic_h;
  
  c_w = (_y4m->pic_w + _y4m->src_c_dec_h - 1) / _y4m->src_c_dec_h;
  c_h = _y4m->pic_h;
  dst_c_w = (_y4m->pic_w + _y4m->dst_c_dec_h - 1) / _y4m->dst_c_dec_h;
  dst_c_h = (_y4m->pic_h + _y4m->dst_c_dec_v - 1) / _y4m->dst_c_dec_v;
  c_sz = c_w * c_h;
  dst_c_sz = dst_c_w * dst_c_h;
  tmp_sz = dst_c_w * c_h;
  tmp = _aux + 2 * c_sz;
  for (pli = 1; pli < 3; pli++) {
    
    for (y = 0; y < c_h; y++) {
      for (x = 0; x < OC_MINI(c_w, 2); x += 2) {
        tmp[x >> 1] = OC_CLAMPI(0, (64 * _aux[0] + 78 * _aux[OC_MINI(1, c_w - 1)]
                                    - 17 * _aux[OC_MINI(2, c_w - 1)]
                                    + 3 * _aux[OC_MINI(3, c_w - 1)] + 64) >> 7, 255);
      }
      for (; x < c_w - 3; x += 2) {
        tmp[x >> 1] = OC_CLAMPI(0, (3 * (_aux[x - 2] + _aux[x + 3])
                                    - 17 * (_aux[x - 1] + _aux[x + 2]) + 78 * (_aux[x] + _aux[x + 1]) + 64) >> 7, 255);
      }
      for (; x < c_w; x += 2) {
        tmp[x >> 1] = OC_CLAMPI(0, (3 * (_aux[x - 2] + _aux[c_w - 1]) -
                                    17 * (_aux[x - 1] + _aux[OC_MINI(x + 2, c_w - 1)]) +
                                    78 * (_aux[x] + _aux[OC_MINI(x + 1, c_w - 1)]) + 64) >> 7, 255);
      }
      tmp += dst_c_w;
      _aux += c_w;
    }
    tmp -= tmp_sz;
    
    y4m_422jpeg_420jpeg_helper(_dst, tmp, dst_c_w, c_h);
    _dst += dst_c_sz;
  }
}

static void y4m_convert_mono_420jpeg(y4m_input *_y4m, unsigned char *_dst,
                                     unsigned char *_aux) {
  int c_sz;
  (void)_aux;
  _dst += _y4m->pic_w * _y4m->pic_h;
  c_sz = ((_y4m->pic_w + _y4m->dst_c_dec_h - 1) / _y4m->dst_c_dec_h) *
         ((_y4m->pic_h + _y4m->dst_c_dec_v - 1) / _y4m->dst_c_dec_v);
  memset(_dst, 128, c_sz * 2);
}

static void y4m_convert_null(y4m_input *_y4m, unsigned char *_dst,
                             unsigned char *_aux) {
  (void)_y4m;
  (void)_dst;
  (void)_aux;
}

int y4m_input_open(y4m_input *_y4m, FILE *_fin, char *_skip, int _nskip,
                   int only_420) {
  char buffer[80] = {0};
  int  ret;
  int  i;
  
  for (i = 0; i < 79; i++) {
    if (_nskip > 0) {
      buffer[i] = *_skip++;
      _nskip--;
    } else {
      if (!file_read(buffer + i, 1, _fin)) return -1;
    }
    if (buffer[i] == '\n')break;
  }
  
  if (_nskip > 0)return -1;
  if (i == 79) {
    fprintf(stderr, "Error parsing header; not a YUV2MPEG2 file?\n");
    return -1;
  }
  buffer[i] = '\0';
  if (memcmp(buffer, "YUV4MPEG", 8)) {
    fprintf(stderr, "Incomplete magic for YUV4MPEG file.\n");
    return -1;
  }
  if (buffer[8] != '2') {
    fprintf(stderr, "Incorrect YUV input file version; YUV4MPEG2 required.\n");
  }
  ret = y4m_parse_tags(_y4m, buffer + 5);
  if (ret < 0) {
    fprintf(stderr, "Error parsing YUV4MPEG2 header.\n");
    return ret;
  }
  if (_y4m->interlace == '?') {
    fprintf(stderr, "Warning: Input video interlacing format unknown; "
            "assuming progressive scan.\n");
  } else if (_y4m->interlace != 'p') {
    fprintf(stderr, "Input video is interlaced; "
            "Only progressive scan handled.\n");
    return -1;
  }
  _y4m->vpx_fmt = VPX_IMG_FMT_I420;
  _y4m->bps = 12;
  _y4m->bit_depth = 8;
  if (strcmp(_y4m->chroma_type, "420") == 0 ||
      strcmp(_y4m->chroma_type, "420jpeg") == 0) {
    _y4m->src_c_dec_h = _y4m->dst_c_dec_h = _y4m->src_c_dec_v = _y4m->dst_c_dec_v = 2;
    _y4m->dst_buf_read_sz = _y4m->pic_w * _y4m->pic_h
                            + 2 * ((_y4m->pic_w + 1) / 2) * ((_y4m->pic_h + 1) / 2);
    
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
    _y4m->convert = y4m_convert_null;
  } else if (strcmp(_y4m->chroma_type, "420p10") == 0) {
    _y4m->src_c_dec_h = 2;
    _y4m->dst_c_dec_h = 2;
    _y4m->src_c_dec_v = 2;
    _y4m->dst_c_dec_v = 2;
    _y4m->dst_buf_read_sz = 2 * (_y4m->pic_w * _y4m->pic_h +
                                 2 * ((_y4m->pic_w + 1) / 2) *
                                 ((_y4m->pic_h + 1) / 2));
    
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
    _y4m->convert = y4m_convert_null;
    _y4m->bit_depth = 10;
    _y4m->bps = 15;
    _y4m->vpx_fmt = VPX_IMG_FMT_I42016;
    if (only_420) {
      fprintf(stderr, "Unsupported conversion from 420p10 to 420jpeg\n");
      return -1;
    }
  } else if (strcmp(_y4m->chroma_type, "420p12") == 0) {
    _y4m->src_c_dec_h = 2;
    _y4m->dst_c_dec_h = 2;
    _y4m->src_c_dec_v = 2;
    _y4m->dst_c_dec_v = 2;
    _y4m->dst_buf_read_sz = 2 * (_y4m->pic_w * _y4m->pic_h +
                                 2 * ((_y4m->pic_w + 1) / 2) *
                                 ((_y4m->pic_h + 1) / 2));
    
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
    _y4m->convert = y4m_convert_null;
    _y4m->bit_depth = 12;
    _y4m->bps = 18;
    _y4m->vpx_fmt = VPX_IMG_FMT_I42016;
    if (only_420) {
      fprintf(stderr, "Unsupported conversion from 420p12 to 420jpeg\n");
      return -1;
    }
  } else if (strcmp(_y4m->chroma_type, "420mpeg2") == 0) {
    _y4m->src_c_dec_h = _y4m->dst_c_dec_h = _y4m->src_c_dec_v = _y4m->dst_c_dec_v = 2;
    _y4m->dst_buf_read_sz = _y4m->pic_w * _y4m->pic_h;
    
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz =
                         2 * ((_y4m->pic_w + 1) / 2) * ((_y4m->pic_h + 1) / 2);
    _y4m->convert = y4m_convert_42xmpeg2_42xjpeg;
  } else if (strcmp(_y4m->chroma_type, "420paldv") == 0) {
    _y4m->src_c_dec_h = _y4m->dst_c_dec_h = _y4m->src_c_dec_v = _y4m->dst_c_dec_v = 2;
    _y4m->dst_buf_read_sz = _y4m->pic_w * _y4m->pic_h;
    _y4m->aux_buf_sz = 3 * ((_y4m->pic_w + 1) / 2) * ((_y4m->pic_h + 1) / 2);
    _y4m->aux_buf_read_sz = 2 * ((_y4m->pic_w + 1) / 2) * ((_y4m->pic_h + 1) / 2);
    _y4m->convert = y4m_convert_42xpaldv_42xjpeg;
  } else if (strcmp(_y4m->chroma_type, "422jpeg") == 0) {
    _y4m->src_c_dec_h = _y4m->dst_c_dec_h = 2;
    _y4m->src_c_dec_v = 1;
    _y4m->dst_c_dec_v = 2;
    _y4m->dst_buf_read_sz = _y4m->pic_w * _y4m->pic_h;
    
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 2 * ((_y4m->pic_w + 1) / 2) * _y4m->pic_h;
    _y4m->convert = y4m_convert_422jpeg_420jpeg;
  } else if (strcmp(_y4m->chroma_type, "422") == 0) {
    _y4m->src_c_dec_h = 2;
    _y4m->src_c_dec_v = 1;
    if (only_420) {
      _y4m->dst_c_dec_h = 2;
      _y4m->dst_c_dec_v = 2;
      _y4m->dst_buf_read_sz = _y4m->pic_w * _y4m->pic_h;
      _y4m->aux_buf_read_sz = 2 * ((_y4m->pic_w + 1) / 2) * _y4m->pic_h;
      _y4m->aux_buf_sz = _y4m->aux_buf_read_sz +
          ((_y4m->pic_w + 1) / 2) * _y4m->pic_h;
      _y4m->convert = y4m_convert_422_420jpeg;
    } else {
      _y4m->vpx_fmt = VPX_IMG_FMT_I422;
      _y4m->bps = 16;
      _y4m->dst_c_dec_h = _y4m->src_c_dec_h;
      _y4m->dst_c_dec_v = _y4m->src_c_dec_v;
      _y4m->dst_buf_read_sz = _y4m->pic_w * _y4m->pic_h
                              + 2 * ((_y4m->pic_w + 1) / 2) * _y4m->pic_h;
      
      _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
      _y4m->convert = y4m_convert_null;
    }
  } else if (strcmp(_y4m->chroma_type, "422p10") == 0) {
    _y4m->src_c_dec_h = 2;
    _y4m->src_c_dec_v = 1;
    _y4m->vpx_fmt = VPX_IMG_FMT_I42216;
    _y4m->bps = 20;
    _y4m->bit_depth = 10;
    _y4m->dst_c_dec_h = _y4m->src_c_dec_h;
    _y4m->dst_c_dec_v = _y4m->src_c_dec_v;
    _y4m->dst_buf_read_sz = 2 * (_y4m->pic_w * _y4m->pic_h +
                                 2 * ((_y4m->pic_w + 1) / 2) * _y4m->pic_h);
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
    _y4m->convert = y4m_convert_null;
    if (only_420) {
      fprintf(stderr, "Unsupported conversion from 422p10 to 420jpeg\n");
      return -1;
    }
  } else if (strcmp(_y4m->chroma_type, "422p12") == 0) {
    _y4m->src_c_dec_h = 2;
    _y4m->src_c_dec_v = 1;
    _y4m->vpx_fmt = VPX_IMG_FMT_I42216;
    _y4m->bps = 24;
    _y4m->bit_depth = 12;
    _y4m->dst_c_dec_h = _y4m->src_c_dec_h;
    _y4m->dst_c_dec_v = _y4m->src_c_dec_v;
    _y4m->dst_buf_read_sz = 2 * (_y4m->pic_w * _y4m->pic_h +
                                 2 * ((_y4m->pic_w + 1) / 2) * _y4m->pic_h);
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
    _y4m->convert = y4m_convert_null;
    if (only_420) {
      fprintf(stderr, "Unsupported conversion from 422p12 to 420jpeg\n");
      return -1;
    }
  } else if (strcmp(_y4m->chroma_type, "411") == 0) {
    _y4m->src_c_dec_h = 4;
    _y4m->dst_c_dec_h = 2;
    _y4m->src_c_dec_v = 1;
    _y4m->dst_c_dec_v = 2;
    _y4m->dst_buf_read_sz = _y4m->pic_w * _y4m->pic_h;
    _y4m->aux_buf_read_sz = 2 * ((_y4m->pic_w + 3) / 4) * _y4m->pic_h;
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz + ((_y4m->pic_w + 1) / 2) * _y4m->pic_h;
    _y4m->convert = y4m_convert_411_420jpeg;
  } else if (strcmp(_y4m->chroma_type, "444") == 0) {
    _y4m->src_c_dec_h = 1;
    _y4m->src_c_dec_v = 1;
    if (only_420) {
      _y4m->dst_c_dec_h = 2;
      _y4m->dst_c_dec_v = 2;
      _y4m->dst_buf_read_sz = _y4m->pic_w * _y4m->pic_h;
      _y4m->aux_buf_read_sz = 2 * _y4m->pic_w * _y4m->pic_h;
      _y4m->aux_buf_sz = _y4m->aux_buf_read_sz +
          ((_y4m->pic_w + 1) / 2) * _y4m->pic_h;
      _y4m->convert = y4m_convert_444_420jpeg;
    } else {
      _y4m->vpx_fmt = VPX_IMG_FMT_I444;
      _y4m->bps = 24;
      _y4m->dst_c_dec_h = _y4m->src_c_dec_h;
      _y4m->dst_c_dec_v = _y4m->src_c_dec_v;
      _y4m->dst_buf_read_sz = 3 * _y4m->pic_w * _y4m->pic_h;
      
      _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
      _y4m->convert = y4m_convert_null;
    }
  } else if (strcmp(_y4m->chroma_type, "444p10") == 0) {
    _y4m->src_c_dec_h = 1;
    _y4m->src_c_dec_v = 1;
    _y4m->vpx_fmt = VPX_IMG_FMT_I44416;
    _y4m->bps = 30;
    _y4m->bit_depth = 10;
    _y4m->dst_c_dec_h = _y4m->src_c_dec_h;
    _y4m->dst_c_dec_v = _y4m->src_c_dec_v;
    _y4m->dst_buf_read_sz = 2 * 3 * _y4m->pic_w * _y4m->pic_h;
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
    _y4m->convert = y4m_convert_null;
    if (only_420) {
      fprintf(stderr, "Unsupported conversion from 444p10 to 420jpeg\n");
      return -1;
    }
  } else if (strcmp(_y4m->chroma_type, "444p12") == 0) {
    _y4m->src_c_dec_h = 1;
    _y4m->src_c_dec_v = 1;
    _y4m->vpx_fmt = VPX_IMG_FMT_I44416;
    _y4m->bps = 36;
    _y4m->bit_depth = 12;
    _y4m->dst_c_dec_h = _y4m->src_c_dec_h;
    _y4m->dst_c_dec_v = _y4m->src_c_dec_v;
    _y4m->dst_buf_read_sz = 2 * 3 * _y4m->pic_w * _y4m->pic_h;
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
    _y4m->convert = y4m_convert_null;
    if (only_420) {
      fprintf(stderr, "Unsupported conversion from 444p12 to 420jpeg\n");
      return -1;
    }
  } else if (strcmp(_y4m->chroma_type, "444alpha") == 0) {
    _y4m->src_c_dec_h = 1;
    _y4m->src_c_dec_v = 1;
    if (only_420) {
      _y4m->dst_c_dec_h = 2;
      _y4m->dst_c_dec_v = 2;
      _y4m->dst_buf_read_sz = _y4m->pic_w * _y4m->pic_h;
      _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 3 * _y4m->pic_w * _y4m->pic_h;
      _y4m->convert = y4m_convert_444_420jpeg;
    } else {
      _y4m->vpx_fmt = VPX_IMG_FMT_444A;
      _y4m->bps = 32;
      _y4m->dst_c_dec_h = _y4m->src_c_dec_h;
      _y4m->dst_c_dec_v = _y4m->src_c_dec_v;
      _y4m->dst_buf_read_sz = 4 * _y4m->pic_w * _y4m->pic_h;
      
      _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
      _y4m->convert = y4m_convert_null;
    }
  } else if (strcmp(_y4m->chroma_type, "mono") == 0) {
    _y4m->src_c_dec_h = _y4m->src_c_dec_v = 0;
    _y4m->dst_c_dec_h = _y4m->dst_c_dec_v = 2;
    _y4m->dst_buf_read_sz = _y4m->pic_w * _y4m->pic_h;
    
    _y4m->aux_buf_sz = _y4m->aux_buf_read_sz = 0;
    _y4m->convert = y4m_convert_mono_420jpeg;
  } else {
    fprintf(stderr, "Unknown chroma sampling type: %s\n", _y4m->chroma_type);
    return -1;
  }
  _y4m->dst_buf_sz = _y4m->pic_w * _y4m->pic_h
                     + 2 * ((_y4m->pic_w + _y4m->dst_c_dec_h - 1) / _y4m->dst_c_dec_h) *
                     ((_y4m->pic_h + _y4m->dst_c_dec_v - 1) / _y4m->dst_c_dec_v);
  if (_y4m->bit_depth == 8)
    _y4m->dst_buf = (unsigned char *)malloc(_y4m->dst_buf_sz);
  else
    _y4m->dst_buf = (unsigned char *)malloc(2 * _y4m->dst_buf_sz);

  if (_y4m->aux_buf_sz > 0)
    _y4m->aux_buf = (unsigned char *)malloc(_y4m->aux_buf_sz);
  return 0;
}

void y4m_input_close(y4m_input *_y4m) {
  free(_y4m->dst_buf);
  free(_y4m->aux_buf);
}

int y4m_input_fetch_frame(y4m_input *_y4m, FILE *_fin, vpx_image_t *_img) {
  char frame[6];
  int  pic_sz;
  int  c_w;
  int  c_h;
  int  c_sz;
  int  bytes_per_sample = _y4m->bit_depth > 8 ? 2 : 1;
  
  if (!file_read(frame, 6, _fin)) return 0;
  if (memcmp(frame, "FRAME", 5)) {
    fprintf(stderr, "Loss of framing in Y4M input data\n");
    return -1;
  }
  if (frame[5] != '\n') {
    char c;
    int  j;
    for (j = 0; j < 79 && file_read(&c, 1, _fin) && c != '\n'; j++) {}
    if (j == 79) {
      fprintf(stderr, "Error parsing Y4M frame header\n");
      return -1;
    }
  }
  
  if (!file_read(_y4m->dst_buf, _y4m->dst_buf_read_sz, _fin)) {
    fprintf(stderr, "Error reading Y4M frame data.\n");
    return -1;
  }
  
  if (!file_read(_y4m->aux_buf, _y4m->aux_buf_read_sz, _fin)) {
    fprintf(stderr, "Error reading Y4M frame data.\n");
    return -1;
  }
  
  (*_y4m->convert)(_y4m, _y4m->dst_buf, _y4m->aux_buf);
  memset(_img, 0, sizeof(*_img));
  
  _img->fmt = _y4m->vpx_fmt;
  _img->w = _img->d_w = _y4m->pic_w;
  _img->h = _img->d_h = _y4m->pic_h;
  _img->x_chroma_shift = _y4m->dst_c_dec_h >> 1;
  _img->y_chroma_shift = _y4m->dst_c_dec_v >> 1;
  _img->bps = _y4m->bps;

  
  pic_sz = _y4m->pic_w * _y4m->pic_h * bytes_per_sample;
  c_w = (_y4m->pic_w + _y4m->dst_c_dec_h - 1) / _y4m->dst_c_dec_h;
  c_w *= bytes_per_sample;
  c_h = (_y4m->pic_h + _y4m->dst_c_dec_v - 1) / _y4m->dst_c_dec_v;
  c_sz = c_w * c_h;
  _img->stride[VPX_PLANE_Y] = _img->stride[VPX_PLANE_ALPHA] =
      _y4m->pic_w * bytes_per_sample;
  _img->stride[VPX_PLANE_U] = _img->stride[VPX_PLANE_V] = c_w;
  _img->planes[VPX_PLANE_Y] = _y4m->dst_buf;
  _img->planes[VPX_PLANE_U] = _y4m->dst_buf + pic_sz;
  _img->planes[VPX_PLANE_V] = _y4m->dst_buf + pic_sz + c_sz;
  _img->planes[VPX_PLANE_ALPHA] = _y4m->dst_buf + pic_sz + 2 * c_sz;
  return 1;
}

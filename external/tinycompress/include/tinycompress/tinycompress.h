/*
 * BSD LICENSE
 *
 * Copyright (c) 2011-2012, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * LGPL LICENSE
 *
 * tinycompress library for compress audio offload in alsa
 * Copyright (c) 2011-2012, Intel Corporation.
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to
 * the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef __TINYCOMPRESS_H
#define __TINYCOMPRESS_H

#if defined(__cplusplus)
extern "C" {
#endif
struct compr_config {
	__u32 fragment_size;
	__u32 fragments;
	struct snd_codec *codec;
};

struct compr_gapless_mdata {
	__u32 encoder_delay;
	__u32 encoder_padding;
};

#define COMPRESS_OUT        0x20000000
#define COMPRESS_IN         0x10000000

struct compress;
struct snd_compr_tstamp;
union snd_codec_options;

struct compress *compress_open(unsigned int card, unsigned int device,
		unsigned int flags, struct compr_config *config);

void compress_close(struct compress *compress);

int compress_get_hpointer(struct compress *compress,
		unsigned int *avail, struct timespec *tstamp);


int compress_get_tstamp(struct compress *compress,
		unsigned long *samples, unsigned int *sampling_rate);

/*
 * compress_write: write data to the compress stream
 * return bytes written on success, negative on error
 * By default this is a blocking call and will not return
 * until all bytes have been written or there was a
 * write error.
 * If non-blocking mode has been enabled with compress_nonblock(),
 * this function will write all bytes that can be written without
 * blocking and will then return the number of bytes successfully
 * written. If the return value is not an error and is < size
 * the caller can use compress_wait() to block until the driver
 * is ready for more data.
 *
 * @compress: compress stream to be written to
 * @buf: pointer to data
 * @size: number of bytes to be written
 */
int compress_write(struct compress *compress, const void *buf, unsigned int size);

/*
 * compress_read: read data from the compress stream
 * return bytes read on success, negative on error
 * By default this is a blocking call and will block until
 * size bytes have been written or there was a read error.
 * If non-blocking mode was enabled using compress_nonblock()
 * the behaviour will change to read only as many bytes as
 * are currently available (if no bytes are available it
 * will return immediately). The caller can then use
 * compress_wait() to block until more bytes are available.
 *
 * @compress: compress stream from where data is to be read
 * @buf: pointer to data buffer
 * @size: size of given buffer
 */
int compress_read(struct compress *compress, void *buf, unsigned int size);

int compress_start(struct compress *compress);

int compress_stop(struct compress *compress);

int compress_pause(struct compress *compress);

int compress_resume(struct compress *compress);

int compress_drain(struct compress *compress);

int compress_next_track(struct compress *compress);

/*
 * compress_partial_drain: drain will return after the last frame is decoded
 * by DSP and will play the , All the data written into compressed
 * ring buffer is decoded
 *
 * return 0 on success, negative on error
 *
 * @compress: compress stream to be drain
 */
int compress_partial_drain(struct compress *compress);


int compress_set_gapless_metadata(struct compress *compress,
			struct compr_gapless_mdata *mdata);


int compress_set_next_track_param(struct compress *compress,
			union snd_codec_options *codec_options);

bool is_codec_supported(unsigned int card, unsigned int device,
	       unsigned int flags, struct snd_codec *codec);

void compress_set_max_poll_wait(struct compress *compress, int milliseconds);

void compress_nonblock(struct compress *compress, int nonblock);

int compress_wait(struct compress *compress, int timeout_ms);

int is_compress_running(struct compress *compress);

int is_compress_ready(struct compress *compress);

const char *compress_get_error(struct compress *compress);
#define SNDRV_PCM_RATE_5512		(1<<0)		
#define SNDRV_PCM_RATE_8000		(1<<1)		
#define SNDRV_PCM_RATE_11025		(1<<2)		
#define SNDRV_PCM_RATE_16000		(1<<3)		
#define SNDRV_PCM_RATE_22050		(1<<4)		
#define SNDRV_PCM_RATE_32000		(1<<5)		
#define SNDRV_PCM_RATE_44100		(1<<6)		
#define SNDRV_PCM_RATE_48000		(1<<7)		
#define SNDRV_PCM_RATE_64000		(1<<8)		
#define SNDRV_PCM_RATE_88200		(1<<9)		
#define SNDRV_PCM_RATE_96000		(1<<10)		
#define SNDRV_PCM_RATE_176400		(1<<11)		
#define SNDRV_PCM_RATE_192000		(1<<12)		

unsigned int compress_get_alsa_rate(unsigned int rate);

int compress_fd(struct compress *compress);

#if defined(__cplusplus)
}
#endif

#endif

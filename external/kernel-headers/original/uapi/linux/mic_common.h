/*
 * Intel MIC Platform Software Stack (MPSS)
 *
 * Copyright(c) 2013 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Intel MIC driver.
 *
 */
#ifndef __MIC_COMMON_H_
#define __MIC_COMMON_H_

#include <linux/virtio_ring.h>

#define __mic_align(a, x) (((a) + (x) - 1) & ~((x) - 1))

/**
 * struct mic_device_desc: Virtio device information shared between the
 * virtio driver and userspace backend
 *
 * @type: Device type: console/network/disk etc.  Type 0/-1 terminates.
 * @num_vq: Number of virtqueues.
 * @feature_len: Number of bytes of feature bits.  Multiply by 2: one for
   host features and one for guest acknowledgements.
 * @config_len: Number of bytes of the config array after virtqueues.
 * @status: A status byte, written by the Guest.
 * @config: Start of the following variable length config.
 */
struct mic_device_desc {
	__s8 type;
	__u8 num_vq;
	__u8 feature_len;
	__u8 config_len;
	__u8 status;
	__le64 config[0];
} __attribute__ ((aligned(8)));

struct mic_device_ctrl {
	__le64 vdev;
	__u8 config_change;
	__u8 vdev_reset;
	__u8 guest_ack;
	__u8 host_ack;
	__u8 used_address_updated;
	__s8 c2h_vdev_db;
	__s8 h2c_vdev_db;
} __attribute__ ((aligned(8)));

struct mic_bootparam {
	__le32 magic;
	__s8 c2h_shutdown_db;
	__s8 h2c_shutdown_db;
	__s8 h2c_config_db;
	__u8 shutdown_status;
	__u8 shutdown_card;
} __attribute__ ((aligned(8)));

struct mic_device_page {
	struct mic_bootparam bootparam;
	struct mic_device_desc desc[0];
};
struct mic_vqconfig {
	__le64 address;
	__le64 used_address;
	__le16 num;
} __attribute__ ((aligned(8)));

#define MIC_VIRTIO_RING_ALIGN		4096

#define MIC_MAX_VRINGS			4
#define MIC_VRING_ENTRIES		128

#define MIC_MAX_VRING_ENTRIES		128

#define MIC_MAX_DESC_BLK_SIZE		256

struct _mic_vring_info {
	__u16 avail_idx;
	__le32 magic;
};

struct mic_vring {
	struct vring vr;
	struct _mic_vring_info *info;
	void *va;
	int len;
};

#define mic_aligned_desc_size(d) __mic_align(mic_desc_size(d), 8)

#ifndef INTEL_MIC_CARD
static inline unsigned mic_desc_size(const struct mic_device_desc *desc)
{
	return sizeof(*desc) + desc->num_vq * sizeof(struct mic_vqconfig)
		+ desc->feature_len * 2 + desc->config_len;
}

static inline struct mic_vqconfig *
mic_vq_config(const struct mic_device_desc *desc)
{
	return (struct mic_vqconfig *)(desc + 1);
}

static inline __u8 *mic_vq_features(const struct mic_device_desc *desc)
{
	return (__u8 *)(mic_vq_config(desc) + desc->num_vq);
}

static inline __u8 *mic_vq_configspace(const struct mic_device_desc *desc)
{
	return mic_vq_features(desc) + desc->feature_len * 2;
}
static inline unsigned mic_total_desc_size(struct mic_device_desc *desc)
{
	return mic_aligned_desc_size(desc) + sizeof(struct mic_device_ctrl);
}
#endif

#define MIC_DP_SIZE 4096

#define MIC_MAGIC 0xc0ffee00

enum mic_states {
	MIC_OFFLINE = 0,
	MIC_ONLINE,
	MIC_SHUTTING_DOWN,
	MIC_RESET_FAILED,
	MIC_SUSPENDING,
	MIC_SUSPENDED,
	MIC_LAST
};

enum mic_status {
	MIC_NOP = 0,
	MIC_CRASHED,
	MIC_HALTED,
	MIC_POWER_OFF,
	MIC_RESTART,
	MIC_STATUS_LAST
};

#endif

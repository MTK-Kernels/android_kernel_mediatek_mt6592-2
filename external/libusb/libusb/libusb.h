/*
 * Public libusb header file
 * Copyright (C) 2007-2008 Daniel Drake <dsd@gentoo.org>
 * Copyright (c) 2001 Johannes Erdfelt <johannes@erdfelt.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __LIBUSB_H__
#define __LIBUSB_H__

#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#define libusb_cpu_to_le16(x) ({ \
	union { \
		uint8_t  b8[2]; \
		uint16_t b16; \
	} _tmp; \
	uint16_t _tmp2 = (uint16_t)(x); \
	_tmp.b8[1] = _tmp2 >> 8; \
	_tmp.b8[0] = _tmp2 & 0xff; \
	_tmp.b16; \
})

#define libusb_le16_to_cpu libusb_cpu_to_le16


enum libusb_class_code {
	LIBUSB_CLASS_PER_INTERFACE = 0,

	
	LIBUSB_CLASS_AUDIO = 1,

	
	LIBUSB_CLASS_COMM = 2,

	
	LIBUSB_CLASS_HID = 3,

	
	LIBUSB_CLASS_PRINTER = 7,

	
	LIBUSB_CLASS_PTP = 6,

	
	LIBUSB_CLASS_MASS_STORAGE = 8,

	
	LIBUSB_CLASS_HUB = 9,

	
	LIBUSB_CLASS_DATA = 10,

	
	LIBUSB_CLASS_WIRELESS = 0xe0,

	
	LIBUSB_CLASS_APPLICATION = 0xfe,

	
	LIBUSB_CLASS_VENDOR_SPEC = 0xff
};

enum libusb_descriptor_type {
	
	LIBUSB_DT_DEVICE = 0x01,

	
	LIBUSB_DT_CONFIG = 0x02,

	
	LIBUSB_DT_STRING = 0x03,

	
	LIBUSB_DT_INTERFACE = 0x04,

	
	LIBUSB_DT_ENDPOINT = 0x05,

	
	LIBUSB_DT_HID = 0x21,

	
	LIBUSB_DT_REPORT = 0x22,

	
	LIBUSB_DT_PHYSICAL = 0x23,

	
	LIBUSB_DT_HUB = 0x29
};

#define LIBUSB_DT_DEVICE_SIZE			18
#define LIBUSB_DT_CONFIG_SIZE			9
#define LIBUSB_DT_INTERFACE_SIZE		9
#define LIBUSB_DT_ENDPOINT_SIZE		7
#define LIBUSB_DT_ENDPOINT_AUDIO_SIZE	9	
#define LIBUSB_DT_HUB_NONVAR_SIZE		7

#define LIBUSB_ENDPOINT_ADDRESS_MASK	0x0f    
#define LIBUSB_ENDPOINT_DIR_MASK		0x80

enum libusb_endpoint_direction {
	
	LIBUSB_ENDPOINT_IN = 0x80,

	
	LIBUSB_ENDPOINT_OUT = 0x00
};

#define LIBUSB_TRANSFER_TYPE_MASK			0x03    

enum libusb_transfer_type {
	
	LIBUSB_TRANSFER_TYPE_CONTROL = 0,

	
	LIBUSB_TRANSFER_TYPE_ISOCHRONOUS = 1,

	
	LIBUSB_TRANSFER_TYPE_BULK = 2,

	
	LIBUSB_TRANSFER_TYPE_INTERRUPT = 3
};

enum libusb_standard_request {
	
	LIBUSB_REQUEST_GET_STATUS = 0x00,

	
	LIBUSB_REQUEST_CLEAR_FEATURE = 0x01,

	

	
	LIBUSB_REQUEST_SET_FEATURE = 0x03,

	

	
	LIBUSB_REQUEST_SET_ADDRESS = 0x05,

	
	LIBUSB_REQUEST_GET_DESCRIPTOR = 0x06,

	
	LIBUSB_REQUEST_SET_DESCRIPTOR = 0x07,

	
	LIBUSB_REQUEST_GET_CONFIGURATION = 0x08,

	
	LIBUSB_REQUEST_SET_CONFIGURATION = 0x09,

	
	LIBUSB_REQUEST_GET_INTERFACE = 0x0A,

	
	LIBUSB_REQUEST_SET_INTERFACE = 0x0B,

	
	LIBUSB_REQUEST_SYNCH_FRAME = 0x0C
};

enum libusb_request_type {
	
	LIBUSB_REQUEST_TYPE_STANDARD = (0x00 << 5),

	
	LIBUSB_REQUEST_TYPE_CLASS = (0x01 << 5),

	
	LIBUSB_REQUEST_TYPE_VENDOR = (0x02 << 5),

	
	LIBUSB_REQUEST_TYPE_RESERVED = (0x03 << 5)
};

enum libusb_request_recipient {
	
	LIBUSB_RECIPIENT_DEVICE = 0x00,

	
	LIBUSB_RECIPIENT_INTERFACE = 0x01,

	
	LIBUSB_RECIPIENT_ENDPOINT = 0x02,

	
	LIBUSB_RECIPIENT_OTHER = 0x03
};

#define LIBUSB_ISO_SYNC_TYPE_MASK		0x0C

enum libusb_iso_sync_type {
	
	LIBUSB_ISO_SYNC_TYPE_NONE = 0,

	
	LIBUSB_ISO_SYNC_TYPE_ASYNC = 1,

	
	LIBUSB_ISO_SYNC_TYPE_ADAPTIVE = 2,

	
	LIBUSB_ISO_SYNC_TYPE_SYNC = 3
};

#define LIBUSB_ISO_USAGE_TYPE_MASK 0x30

enum libusb_iso_usage_type {
	
	LIBUSB_ISO_USAGE_TYPE_DATA = 0,

	
	LIBUSB_ISO_USAGE_TYPE_FEEDBACK = 1,

	
	LIBUSB_ISO_USAGE_TYPE_IMPLICIT = 2
};

struct libusb_device_descriptor {
	
	uint8_t  bLength;

	uint8_t  bDescriptorType;

	uint16_t bcdUSB;

	
	uint8_t  bDeviceClass;

	uint8_t  bDeviceSubClass;

	uint8_t  bDeviceProtocol;

	
	uint8_t  bMaxPacketSize0;

	
	uint16_t idVendor;

	
	uint16_t idProduct;

	
	uint16_t bcdDevice;

	
	uint8_t  iManufacturer;

	
	uint8_t  iProduct;

	
	uint8_t  iSerialNumber;

	
	uint8_t  bNumConfigurations;
};

struct libusb_endpoint_descriptor {
	
	uint8_t  bLength;

	uint8_t  bDescriptorType;

	uint8_t  bEndpointAddress;

	uint8_t  bmAttributes;

	
	uint16_t wMaxPacketSize;

	
	uint8_t  bInterval;

	uint8_t  bRefresh;

	
	uint8_t  bSynchAddress;

	const unsigned char *extra;

	
	int extra_length;
};

struct libusb_interface_descriptor {
	
	uint8_t  bLength;

	uint8_t  bDescriptorType;

	
	uint8_t  bInterfaceNumber;

	
	uint8_t  bAlternateSetting;

	uint8_t  bNumEndpoints;

	
	uint8_t  bInterfaceClass;

	uint8_t  bInterfaceSubClass;

	uint8_t  bInterfaceProtocol;

	
	uint8_t  iInterface;

	const struct libusb_endpoint_descriptor *endpoint;

	const unsigned char *extra;

	
	int extra_length;
};

struct libusb_interface {
	const struct libusb_interface_descriptor *altsetting;

	
	int num_altsetting;
};

struct libusb_config_descriptor {
	
	uint8_t  bLength;

	uint8_t  bDescriptorType;

	
	uint16_t wTotalLength;

	
	uint8_t  bNumInterfaces;

	
	uint8_t  bConfigurationValue;

	
	uint8_t  iConfiguration;

	
	uint8_t  bmAttributes;

	uint8_t  MaxPower;

	const struct libusb_interface *interface;

	const unsigned char *extra;

	
	int extra_length;
};

struct libusb_control_setup {
	uint8_t  bmRequestType;

	uint8_t  bRequest;

	
	uint16_t wValue;

	uint16_t wIndex;

	
	uint16_t wLength;
};

#define LIBUSB_CONTROL_SETUP_SIZE (sizeof(struct libusb_control_setup))


struct libusb_context;
struct libusb_device;
struct libusb_device_handle;

typedef struct libusb_context libusb_context;

typedef struct libusb_device libusb_device;


typedef struct libusb_device_handle libusb_device_handle;

enum libusb_error {
	
	LIBUSB_SUCCESS = 0,

	
	LIBUSB_ERROR_IO = -1,

	
	LIBUSB_ERROR_INVALID_PARAM = -2,

	
	LIBUSB_ERROR_ACCESS = -3,

	
	LIBUSB_ERROR_NO_DEVICE = -4,

	
	LIBUSB_ERROR_NOT_FOUND = -5,

	
	LIBUSB_ERROR_BUSY = -6,

	
	LIBUSB_ERROR_TIMEOUT = -7,

	
	LIBUSB_ERROR_OVERFLOW = -8,

	
	LIBUSB_ERROR_PIPE = -9,

	
	LIBUSB_ERROR_INTERRUPTED = -10,

	
	LIBUSB_ERROR_NO_MEM = -11,

	
	LIBUSB_ERROR_NOT_SUPPORTED = -12,

	
	LIBUSB_ERROR_OTHER = -99
};

enum libusb_transfer_status {
	LIBUSB_TRANSFER_COMPLETED,

	
	LIBUSB_TRANSFER_ERROR,

	
	LIBUSB_TRANSFER_TIMED_OUT,

	
	LIBUSB_TRANSFER_CANCELLED,

	LIBUSB_TRANSFER_STALL,

	
	LIBUSB_TRANSFER_NO_DEVICE,

	
	LIBUSB_TRANSFER_OVERFLOW
};

enum libusb_transfer_flags {
	
	LIBUSB_TRANSFER_SHORT_NOT_OK = 1<<0,

	
	LIBUSB_TRANSFER_FREE_BUFFER = 1<<1,

	LIBUSB_TRANSFER_FREE_TRANSFER = 1<<2
};

struct libusb_iso_packet_descriptor {
	
	unsigned int length;

	
	unsigned int actual_length;

	
	enum libusb_transfer_status status;
};

struct libusb_transfer;

typedef void (*libusb_transfer_cb_fn)(struct libusb_transfer *transfer);

struct libusb_transfer {
	
	libusb_device_handle *dev_handle;

	
	uint8_t flags;

	
	unsigned char endpoint;

	
	unsigned char type;

	unsigned int timeout;

	enum libusb_transfer_status status;

	
	int length;

	int actual_length;

	libusb_transfer_cb_fn callback;

	
	void *user_data;

	
	unsigned char *buffer;

	int num_iso_packets;

	
	struct libusb_iso_packet_descriptor iso_packet_desc
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
	[] 
#else
	[0] 
#endif
	;
};

int libusb_init(libusb_context **ctx);
void libusb_exit(libusb_context *ctx);
void libusb_set_debug(libusb_context *ctx, int level);

ssize_t libusb_get_device_list(libusb_context *ctx,
	libusb_device ***list);
void libusb_free_device_list(libusb_device **list, int unref_devices);
libusb_device *libusb_ref_device(libusb_device *dev);
void libusb_unref_device(libusb_device *dev);

int libusb_get_configuration(libusb_device_handle *dev, int *config);
int libusb_get_device_descriptor(libusb_device *dev,
	struct libusb_device_descriptor *desc);
int libusb_get_active_config_descriptor(libusb_device *dev,
	struct libusb_config_descriptor **config);
int libusb_get_config_descriptor(libusb_device *dev, uint8_t config_index,
	struct libusb_config_descriptor **config);
int libusb_get_config_descriptor_by_value(libusb_device *dev,
	uint8_t bConfigurationValue, struct libusb_config_descriptor **config);
void libusb_free_config_descriptor(struct libusb_config_descriptor *config);
uint8_t libusb_get_bus_number(libusb_device *dev);
uint8_t libusb_get_device_address(libusb_device *dev);
int libusb_get_max_packet_size(libusb_device *dev, unsigned char endpoint);
int libusb_get_max_iso_packet_size(libusb_device *dev, unsigned char endpoint);

int libusb_open(libusb_device *dev, libusb_device_handle **handle);
void libusb_close(libusb_device_handle *dev_handle);
libusb_device *libusb_get_device(libusb_device_handle *dev_handle);

int libusb_set_configuration(libusb_device_handle *dev, int configuration);
int libusb_claim_interface(libusb_device_handle *dev, int iface);
int libusb_release_interface(libusb_device_handle *dev, int iface);

libusb_device_handle *libusb_open_device_with_vid_pid(libusb_context *ctx,
	uint16_t vendor_id, uint16_t product_id);

int libusb_set_interface_alt_setting(libusb_device_handle *dev,
	int interface_number, int alternate_setting);
int libusb_clear_halt(libusb_device_handle *dev, unsigned char endpoint);
int libusb_reset_device(libusb_device_handle *dev);

int libusb_kernel_driver_active(libusb_device_handle *dev, int interface);
int libusb_detach_kernel_driver(libusb_device_handle *dev, int interface);
int libusb_attach_kernel_driver(libusb_device_handle *dev, int interface);


static inline unsigned char *libusb_control_transfer_get_data(
	struct libusb_transfer *transfer)
{
	return transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE;
}

static inline struct libusb_control_setup *libusb_control_transfer_get_setup(
	struct libusb_transfer *transfer)
{
	return (struct libusb_control_setup *) transfer->buffer;
}

static inline void libusb_fill_control_setup(unsigned char *buffer,
	uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
	uint16_t wLength)
{
	struct libusb_control_setup *setup = (struct libusb_control_setup *) buffer;
	setup->bmRequestType = bmRequestType;
	setup->bRequest = bRequest;
	setup->wValue = libusb_cpu_to_le16(wValue);
	setup->wIndex = libusb_cpu_to_le16(wIndex);
	setup->wLength = libusb_cpu_to_le16(wLength);
}

struct libusb_transfer *libusb_alloc_transfer(int iso_packets);
int libusb_submit_transfer(struct libusb_transfer *transfer);
int libusb_cancel_transfer(struct libusb_transfer *transfer);
void libusb_free_transfer(struct libusb_transfer *transfer);

static inline void libusb_fill_control_transfer(
	struct libusb_transfer *transfer, libusb_device_handle *dev_handle,
	unsigned char *buffer, libusb_transfer_cb_fn callback, void *user_data,
	unsigned int timeout)
{
	struct libusb_control_setup *setup = (struct libusb_control_setup *) buffer;
	transfer->dev_handle = dev_handle;
	transfer->endpoint = 0;
	transfer->type = LIBUSB_TRANSFER_TYPE_CONTROL;
	transfer->timeout = timeout;
	transfer->buffer = buffer;
	if (setup)
		transfer->length = LIBUSB_CONTROL_SETUP_SIZE
			+ libusb_le16_to_cpu(setup->wLength);
	transfer->user_data = user_data;
	transfer->callback = callback;
}

static inline void libusb_fill_bulk_transfer(struct libusb_transfer *transfer,
	libusb_device_handle *dev_handle, unsigned char endpoint,
	unsigned char *buffer, int length, libusb_transfer_cb_fn callback,
	void *user_data, unsigned int timeout)
{
	transfer->dev_handle = dev_handle;
	transfer->endpoint = endpoint;
	transfer->type = LIBUSB_TRANSFER_TYPE_BULK;
	transfer->timeout = timeout;
	transfer->buffer = buffer;
	transfer->length = length;
	transfer->user_data = user_data;
	transfer->callback = callback;
}

static inline void libusb_fill_interrupt_transfer(
	struct libusb_transfer *transfer, libusb_device_handle *dev_handle,
	unsigned char endpoint, unsigned char *buffer, int length,
	libusb_transfer_cb_fn callback, void *user_data, unsigned int timeout)
{
	transfer->dev_handle = dev_handle;
	transfer->endpoint = endpoint;
	transfer->type = LIBUSB_TRANSFER_TYPE_INTERRUPT;
	transfer->timeout = timeout;
	transfer->buffer = buffer;
	transfer->length = length;
	transfer->user_data = user_data;
	transfer->callback = callback;
}

static inline void libusb_fill_iso_transfer(struct libusb_transfer *transfer,
	libusb_device_handle *dev_handle, unsigned char endpoint,
	unsigned char *buffer, int length, int num_iso_packets,
	libusb_transfer_cb_fn callback, void *user_data, unsigned int timeout)
{
	transfer->dev_handle = dev_handle;
	transfer->endpoint = endpoint;
	transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
	transfer->timeout = timeout;
	transfer->buffer = buffer;
	transfer->length = length;
	transfer->num_iso_packets = num_iso_packets;
	transfer->user_data = user_data;
	transfer->callback = callback;
}

static inline void libusb_set_iso_packet_lengths(
	struct libusb_transfer *transfer, unsigned int length)
{
	int i;
	for (i = 0; i < transfer->num_iso_packets; i++)
		transfer->iso_packet_desc[i].length = length;
}

static inline unsigned char *libusb_get_iso_packet_buffer(
	struct libusb_transfer *transfer, unsigned int packet)
{
	int i;
	size_t offset = 0;
	int _packet;

	if (packet > INT_MAX)
		return NULL;
	_packet = packet;

	if (_packet >= transfer->num_iso_packets)
		return NULL;

	for (i = 0; i < _packet; i++)
		offset += transfer->iso_packet_desc[i].length;

	return transfer->buffer + offset;
}

static inline unsigned char *libusb_get_iso_packet_buffer_simple(
	struct libusb_transfer *transfer, unsigned int packet)
{
	int _packet;

	if (packet > INT_MAX)
		return NULL;
	_packet = packet;

	if (_packet >= transfer->num_iso_packets)
		return NULL;

	return transfer->buffer + (transfer->iso_packet_desc[0].length * _packet);
}


int libusb_control_transfer(libusb_device_handle *dev_handle,
	uint8_t request_type, uint8_t request, uint16_t value, uint16_t index,
	unsigned char *data, uint16_t length, unsigned int timeout);

int libusb_bulk_transfer(libusb_device_handle *dev_handle,
	unsigned char endpoint, unsigned char *data, int length,
	int *actual_length, unsigned int timeout);

int libusb_interrupt_transfer(libusb_device_handle *dev_handle,
	unsigned char endpoint, unsigned char *data, int length,
	int *actual_length, unsigned int timeout);

static inline int libusb_get_descriptor(libusb_device_handle *dev,
	uint8_t desc_type, uint8_t desc_index, unsigned char *data, int length)
{
	return libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN,
		LIBUSB_REQUEST_GET_DESCRIPTOR, (desc_type << 8) | desc_index, 0, data,
		length, 1000);
}

static inline int libusb_get_string_descriptor(libusb_device_handle *dev,
	uint8_t desc_index, uint16_t langid, unsigned char *data, int length)
{
	return libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN,
		LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | desc_index,
		langid, data, length, 1000);
}

int libusb_get_string_descriptor_ascii(libusb_device_handle *dev,
	uint8_t index, unsigned char *data, int length);


int libusb_try_lock_events(libusb_context *ctx);
void libusb_lock_events(libusb_context *ctx);
void libusb_unlock_events(libusb_context *ctx);
int libusb_event_handling_ok(libusb_context *ctx);
int libusb_event_handler_active(libusb_context *ctx);
void libusb_lock_event_waiters(libusb_context *ctx);
void libusb_unlock_event_waiters(libusb_context *ctx);
int libusb_wait_for_event(libusb_context *ctx, struct timeval *tv);

int libusb_handle_events_timeout(libusb_context *ctx, struct timeval *tv);
int libusb_handle_events(libusb_context *ctx);
int libusb_handle_events_locked(libusb_context *ctx, struct timeval *tv);
int libusb_pollfds_handle_timeouts(libusb_context *ctx);
int libusb_get_next_timeout(libusb_context *ctx, struct timeval *tv);

struct libusb_pollfd {
	
	int fd;

	short events;
};

typedef void (*libusb_pollfd_added_cb)(int fd, short events, void *user_data);

typedef void (*libusb_pollfd_removed_cb)(int fd, void *user_data);

const struct libusb_pollfd **libusb_get_pollfds(libusb_context *ctx);
void libusb_set_pollfd_notifiers(libusb_context *ctx,
	libusb_pollfd_added_cb added_cb, libusb_pollfd_removed_cb removed_cb,
	void *user_data);

#ifdef __cplusplus
}
#endif

#endif

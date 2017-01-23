/*
 * Core functions for libusb
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

#include <config.h>

#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "libusb.h"
#include "libusbi.h"

#if defined(OS_LINUX)
const struct usbi_os_backend * const usbi_backend = &linux_usbfs_backend;
#elif defined(OS_DARWIN)
const struct usbi_os_backend * const usbi_backend = &darwin_backend;
#else
#error "Unsupported OS"
#endif

struct libusb_context *usbi_default_context = NULL;
static pthread_mutex_t default_context_lock = PTHREAD_MUTEX_INITIALIZER;



/**
 * \page contexts Contexts
 *
 * It is possible that libusb may be used simultaneously from two independent
 * libraries linked into the same executable. For example, if your application
 * has a plugin-like system which allows the user to dynamically load a range
 * of modules into your program, it is feasible that two independently
 * developed modules may both use libusb.
 *
 * libusb is written to allow for these multiple user scenarios. The two
 * "instances" of libusb will not interfere: libusb_set_debug() calls
 * from one user will not affect the same settings for other users, other
 * users can continue using libusb after one of them calls libusb_exit(), etc.
 *
 * This is made possible through libusb's <em>context</em> concept. When you
 * call libusb_init(), you are (optionally) given a context. You can then pass
 * this context pointer back into future libusb functions.
 *
 * In order to keep things simple for more simplistic applications, it is
 * legal to pass NULL to all functions requiring a context pointer (as long as
 * you're sure no other code will attempt to use libusb from the same process).
 * When you pass NULL, the default context will be used. The default context
 * is created the first time a process calls libusb_init() when no other
 * context is alive. Contexts are destroyed during libusb_exit().
 *
 * You may be wondering why only a subset of libusb functions require a
 * context pointer in their function definition. Internally, libusb stores
 * context pointers in other objects (e.g. libusb_device instances) and hence
 * can infer the context from those objects.
 */




#define DISCOVERED_DEVICES_SIZE_STEP 8

static struct discovered_devs *discovered_devs_alloc(void)
{
	struct discovered_devs *ret =
		malloc(sizeof(*ret) + (sizeof(void *) * DISCOVERED_DEVICES_SIZE_STEP));

	if (ret) {
		ret->len = 0;
		ret->capacity = DISCOVERED_DEVICES_SIZE_STEP;
	}
	return ret;
}

struct discovered_devs *discovered_devs_append(
	struct discovered_devs *discdevs, struct libusb_device *dev)
{
	size_t len = discdevs->len;
	size_t capacity;

	
	if (len < discdevs->capacity) {
		discdevs->devices[len] = libusb_ref_device(dev);
		discdevs->len++;
		return discdevs;
	}

	
	usbi_dbg("need to increase capacity");
	capacity = discdevs->capacity + DISCOVERED_DEVICES_SIZE_STEP;
	discdevs = realloc(discdevs,
		sizeof(*discdevs) + (sizeof(void *) * capacity));
	if (discdevs) {
		discdevs->capacity = capacity;
		discdevs->devices[len] = libusb_ref_device(dev);
		discdevs->len++;
	}

	return discdevs;
}

static void discovered_devs_free(struct discovered_devs *discdevs)
{
	size_t i;

	for (i = 0; i < discdevs->len; i++)
		libusb_unref_device(discdevs->devices[i]);

	free(discdevs);
}

struct libusb_device *usbi_alloc_device(struct libusb_context *ctx,
	unsigned long session_id)
{
	size_t priv_size = usbi_backend->device_priv_size;
	struct libusb_device *dev = malloc(sizeof(*dev) + priv_size);
	int r;

	if (!dev)
		return NULL;

	r = pthread_mutex_init(&dev->lock, NULL);
	if (r)
		return NULL;

	dev->ctx = ctx;
	dev->refcnt = 1;
	dev->session_data = session_id;
	memset(&dev->os_priv, 0, priv_size);

	pthread_mutex_lock(&ctx->usb_devs_lock);
	list_add(&dev->list, &ctx->usb_devs);
	pthread_mutex_unlock(&ctx->usb_devs_lock);
	return dev;
}

int usbi_sanitize_device(struct libusb_device *dev)
{
	int r;
	unsigned char raw_desc[DEVICE_DESC_LENGTH];
	uint8_t num_configurations;
	int host_endian;

	r = usbi_backend->get_device_descriptor(dev, raw_desc, &host_endian);
	if (r < 0)
		return r;

	num_configurations = raw_desc[DEVICE_DESC_LENGTH - 1];
	if (num_configurations > USB_MAXCONFIG) {
		usbi_err(DEVICE_CTX(dev), "too many configurations");
		return LIBUSB_ERROR_IO;
	} else if (num_configurations < 1) {
		usbi_dbg("no configurations?");
		return LIBUSB_ERROR_IO;
	}

	dev->num_configurations = num_configurations;
	return 0;
}

struct libusb_device *usbi_get_device_by_session_id(struct libusb_context *ctx,
	unsigned long session_id)
{
	struct libusb_device *dev;
	struct libusb_device *ret = NULL;

	pthread_mutex_lock(&ctx->usb_devs_lock);
	list_for_each_entry(dev, &ctx->usb_devs, list)
		if (dev->session_data == session_id) {
			ret = dev;
			break;
		}
	pthread_mutex_unlock(&ctx->usb_devs_lock);

	return ret;
}

API_EXPORTED ssize_t libusb_get_device_list(libusb_context *ctx,
	libusb_device ***list)
{
	struct discovered_devs *discdevs = discovered_devs_alloc();
	struct libusb_device **ret;
	int r = 0;
	size_t i;
	ssize_t len;
	USBI_GET_CONTEXT(ctx);
	usbi_dbg("");

	if (!discdevs)
		return LIBUSB_ERROR_NO_MEM;

	r = usbi_backend->get_device_list(ctx, &discdevs);
	if (r < 0) {
		len = r;
		goto out;
	}

	
	len = discdevs->len;
	ret = malloc(sizeof(void *) * (len + 1));
	if (!ret) {
		len = LIBUSB_ERROR_NO_MEM;
		goto out;
	}

	ret[len] = NULL;
	for (i = 0; i < len; i++) {
		struct libusb_device *dev = discdevs->devices[i];
		ret[i] = libusb_ref_device(dev);
	}
	*list = ret;

out:
	discovered_devs_free(discdevs);
	return len;
}

API_EXPORTED void libusb_free_device_list(libusb_device **list,
	int unref_devices)
{
	if (!list)
		return;

	if (unref_devices) {
		int i = 0;
		struct libusb_device *dev;

		while ((dev = list[i++]) != NULL)
			libusb_unref_device(dev);
	}
	free(list);
}

API_EXPORTED uint8_t libusb_get_bus_number(libusb_device *dev)
{
	return dev->bus_number;
}

API_EXPORTED uint8_t libusb_get_device_address(libusb_device *dev)
{
	return dev->device_address;
}

static const struct libusb_endpoint_descriptor *find_endpoint(
	struct libusb_config_descriptor *config, unsigned char endpoint)
{
	int iface_idx;
	for (iface_idx = 0; iface_idx < config->bNumInterfaces; iface_idx++) {
		const struct libusb_interface *iface = &config->interface[iface_idx];
		int altsetting_idx;

		for (altsetting_idx = 0; altsetting_idx < iface->num_altsetting;
				altsetting_idx++) {
			const struct libusb_interface_descriptor *altsetting
				= &iface->altsetting[altsetting_idx];
			int ep_idx;

			for (ep_idx = 0; ep_idx < altsetting->bNumEndpoints; ep_idx++) {
				const struct libusb_endpoint_descriptor *ep =
					&altsetting->endpoint[ep_idx];
				if (ep->bEndpointAddress == endpoint)
					return ep;
			}
		}
	}
	return NULL;
}

API_EXPORTED int libusb_get_max_packet_size(libusb_device *dev,
	unsigned char endpoint)
{
	struct libusb_config_descriptor *config;
	const struct libusb_endpoint_descriptor *ep;
	int r;

	r = libusb_get_active_config_descriptor(dev, &config);
	if (r < 0) {
		usbi_err(DEVICE_CTX(dev),
			"could not retrieve active config descriptor");
		return LIBUSB_ERROR_OTHER;
	}

	ep = find_endpoint(config, endpoint);
	if (!ep)
		return LIBUSB_ERROR_NOT_FOUND;

	r = ep->wMaxPacketSize;
	libusb_free_config_descriptor(config);
	return r;
}

API_EXPORTED int libusb_get_max_iso_packet_size(libusb_device *dev,
	unsigned char endpoint)
{
	struct libusb_config_descriptor *config;
	const struct libusb_endpoint_descriptor *ep;
	enum libusb_transfer_type ep_type;
	uint16_t val;
	int r;

	r = libusb_get_active_config_descriptor(dev, &config);
	if (r < 0) {
		usbi_err(DEVICE_CTX(dev),
			"could not retrieve active config descriptor");
		return LIBUSB_ERROR_OTHER;
	}

	ep = find_endpoint(config, endpoint);
	if (!ep)
		return LIBUSB_ERROR_NOT_FOUND;

	val = ep->wMaxPacketSize;
	ep_type = ep->bmAttributes & 0x3;
	libusb_free_config_descriptor(config);

	r = val & 0x07ff;
	if (ep_type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS
			|| ep_type == LIBUSB_TRANSFER_TYPE_INTERRUPT)
		r *= (1 + ((val >> 11) & 3));
	return r;
}

API_EXPORTED libusb_device *libusb_ref_device(libusb_device *dev)
{
	pthread_mutex_lock(&dev->lock);
	dev->refcnt++;
	pthread_mutex_unlock(&dev->lock);
	return dev;
}

API_EXPORTED void libusb_unref_device(libusb_device *dev)
{
	int refcnt;

	if (!dev)
		return;

	pthread_mutex_lock(&dev->lock);
	refcnt = --dev->refcnt;
	pthread_mutex_unlock(&dev->lock);

	if (refcnt == 0) {
		usbi_dbg("destroy device %d.%d", dev->bus_number, dev->device_address);

		if (usbi_backend->destroy_device)
			usbi_backend->destroy_device(dev);

		pthread_mutex_lock(&dev->ctx->usb_devs_lock);
		list_del(&dev->list);
		pthread_mutex_unlock(&dev->ctx->usb_devs_lock);

		free(dev);
	}
}

API_EXPORTED int libusb_open(libusb_device *dev, libusb_device_handle **handle)
{
	struct libusb_context *ctx = DEVICE_CTX(dev);
	struct libusb_device_handle *_handle;
	size_t priv_size = usbi_backend->device_handle_priv_size;
	unsigned char dummy = 1;
	int r;
	usbi_dbg("open %d.%d", dev->bus_number, dev->device_address);

	_handle = malloc(sizeof(*_handle) + priv_size);
	if (!_handle)
		return LIBUSB_ERROR_NO_MEM;

	r = pthread_mutex_init(&_handle->lock, NULL);
	if (r)
		return LIBUSB_ERROR_OTHER;

	_handle->dev = libusb_ref_device(dev);
	_handle->claimed_interfaces = 0;
	memset(&_handle->os_priv, 0, priv_size);

	r = usbi_backend->open(_handle);
	if (r < 0) {
		libusb_unref_device(dev);
		free(_handle);
		return r;
	}

	pthread_mutex_lock(&ctx->open_devs_lock);
	list_add(&_handle->list, &ctx->open_devs);
	pthread_mutex_unlock(&ctx->open_devs_lock);
	*handle = _handle;



	
	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify++;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);

	
	r = write(ctx->ctrl_pipe[1], &dummy, sizeof(dummy));
	if (r <= 0) {
		usbi_warn(ctx, "internal signalling write failed");
		pthread_mutex_lock(&ctx->pollfd_modify_lock);
		ctx->pollfd_modify--;
		pthread_mutex_unlock(&ctx->pollfd_modify_lock);
		return 0;
	}

	
	libusb_lock_events(ctx);

	
	r = read(ctx->ctrl_pipe[0], &dummy, sizeof(dummy));
	if (r <= 0)
		usbi_warn(ctx, "internal signalling read failed");

	
	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify--;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);

	
	libusb_unlock_events(ctx);

	return 0;
}

API_EXPORTED libusb_device_handle *libusb_open_device_with_vid_pid(
	libusb_context *ctx, uint16_t vendor_id, uint16_t product_id)
{
	struct libusb_device **devs;
	struct libusb_device *found = NULL;
	struct libusb_device *dev;
	struct libusb_device_handle *handle = NULL;
	size_t i = 0;
	int r;

	if (libusb_get_device_list(ctx, &devs) < 0)
		return NULL;

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0)
			goto out;
		if (desc.idVendor == vendor_id && desc.idProduct == product_id) {
			found = dev;
			break;
		}
	}

	if (found) {
		r = libusb_open(found, &handle);
		if (r < 0)
			handle = NULL;
	}

out:
	libusb_free_device_list(devs, 1);
	return handle;
}

static void do_close(struct libusb_context *ctx,
	struct libusb_device_handle *dev_handle)
{
	pthread_mutex_lock(&ctx->open_devs_lock);
	list_del(&dev_handle->list);
	pthread_mutex_unlock(&ctx->open_devs_lock);

	usbi_backend->close(dev_handle);
	libusb_unref_device(dev_handle->dev);
	free(dev_handle);
}

API_EXPORTED void libusb_close(libusb_device_handle *dev_handle)
{
	struct libusb_context *ctx;
	unsigned char dummy = 1;
	ssize_t r;

	if (!dev_handle)
		return;
	usbi_dbg("");

	ctx = HANDLE_CTX(dev_handle);


	
	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify++;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);

	
	r = write(ctx->ctrl_pipe[1], &dummy, sizeof(dummy));
	if (r <= 0) {
		usbi_warn(ctx, "internal signalling write failed, closing anyway");
		do_close(ctx, dev_handle);
		pthread_mutex_lock(&ctx->pollfd_modify_lock);
		ctx->pollfd_modify--;
		pthread_mutex_unlock(&ctx->pollfd_modify_lock);
		return;
	}

	
	libusb_lock_events(ctx);

	
	r = read(ctx->ctrl_pipe[0], &dummy, sizeof(dummy));
	if (r <= 0)
		usbi_warn(ctx, "internal signalling read failed, closing anyway");

	
	do_close(ctx, dev_handle);

	
	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify--;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);

	
	libusb_unlock_events(ctx);
}

API_EXPORTED libusb_device *libusb_get_device(libusb_device_handle *dev_handle)
{
	return dev_handle->dev;
}

API_EXPORTED int libusb_get_configuration(libusb_device_handle *dev,
	int *config)
{
	int r = LIBUSB_ERROR_NOT_SUPPORTED;

	usbi_dbg("");
	if (usbi_backend->get_configuration)
		r = usbi_backend->get_configuration(dev, config);

	if (r == LIBUSB_ERROR_NOT_SUPPORTED) {
		uint8_t tmp = 0;
		usbi_dbg("falling back to control message");
		r = libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN,
			LIBUSB_REQUEST_GET_CONFIGURATION, 0, 0, &tmp, 1, 1000);
		if (r == 0) {
			usbi_err(HANDLE_CTX(dev), "zero bytes returned in ctrl transfer?");
			r = LIBUSB_ERROR_IO;
		} else if (r == 1) {
			r = 0;
			*config = tmp;
		} else {
			usbi_dbg("control failed, error %d", r);
		}
	}

	if (r == 0)
		usbi_dbg("active config %d", *config);

	return r;
}

API_EXPORTED int libusb_set_configuration(libusb_device_handle *dev,
	int configuration)
{
	usbi_dbg("configuration %d", configuration);
	return usbi_backend->set_configuration(dev, configuration);
}

API_EXPORTED int libusb_claim_interface(libusb_device_handle *dev,
	int interface_number)
{
	int r = 0;

	usbi_dbg("interface %d", interface_number);
	if (interface_number >= sizeof(dev->claimed_interfaces) * 8)
		return LIBUSB_ERROR_INVALID_PARAM;

	pthread_mutex_lock(&dev->lock);
	if (dev->claimed_interfaces & (1 << interface_number))
		goto out;

	r = usbi_backend->claim_interface(dev, interface_number);
	if (r == 0)
		dev->claimed_interfaces |= 1 << interface_number;

out:
	pthread_mutex_unlock(&dev->lock);
	return r;
}

API_EXPORTED int libusb_release_interface(libusb_device_handle *dev,
	int interface_number)
{
	int r;

	usbi_dbg("interface %d", interface_number);
	if (interface_number >= sizeof(dev->claimed_interfaces) * 8)
		return LIBUSB_ERROR_INVALID_PARAM;

	pthread_mutex_lock(&dev->lock);
	if (!(dev->claimed_interfaces & (1 << interface_number))) {
		r = LIBUSB_ERROR_NOT_FOUND;
		goto out;
	}

	r = usbi_backend->release_interface(dev, interface_number);
	if (r == 0)
		dev->claimed_interfaces &= ~(1 << interface_number);

out:
	pthread_mutex_unlock(&dev->lock);
	return r;
}

API_EXPORTED int libusb_set_interface_alt_setting(libusb_device_handle *dev,
	int interface_number, int alternate_setting)
{
	usbi_dbg("interface %d altsetting %d",
		interface_number, alternate_setting);
	if (interface_number >= sizeof(dev->claimed_interfaces) * 8)
		return LIBUSB_ERROR_INVALID_PARAM;

	pthread_mutex_lock(&dev->lock);
	if (!(dev->claimed_interfaces & (1 << interface_number))) {
		pthread_mutex_unlock(&dev->lock);
		return LIBUSB_ERROR_NOT_FOUND;
	}
	pthread_mutex_unlock(&dev->lock);

	return usbi_backend->set_interface_altsetting(dev, interface_number,
		alternate_setting);
}

API_EXPORTED int libusb_clear_halt(libusb_device_handle *dev,
	unsigned char endpoint)
{
	usbi_dbg("endpoint %x", endpoint);
	return usbi_backend->clear_halt(dev, endpoint);
}

API_EXPORTED int libusb_reset_device(libusb_device_handle *dev)
{
	usbi_dbg("");
	return usbi_backend->reset_device(dev);
}

API_EXPORTED int libusb_kernel_driver_active(libusb_device_handle *dev,
	int interface)
{
	usbi_dbg("interface %d", interface);
	if (usbi_backend->kernel_driver_active)
		return usbi_backend->kernel_driver_active(dev, interface);
	else
		return LIBUSB_ERROR_NOT_SUPPORTED;
}

API_EXPORTED int libusb_detach_kernel_driver(libusb_device_handle *dev,
	int interface)
{
	usbi_dbg("interface %d", interface);
	if (usbi_backend->detach_kernel_driver)
		return usbi_backend->detach_kernel_driver(dev, interface);
	else
		return LIBUSB_ERROR_NOT_SUPPORTED;
}

API_EXPORTED int libusb_attach_kernel_driver(libusb_device_handle *dev,
	int interface)
{
	usbi_dbg("interface %d", interface);
	if (usbi_backend->attach_kernel_driver)
		return usbi_backend->attach_kernel_driver(dev, interface);
	else
		return LIBUSB_ERROR_NOT_SUPPORTED;
}

API_EXPORTED void libusb_set_debug(libusb_context *ctx, int level)
{
	USBI_GET_CONTEXT(ctx);
	if (!ctx->debug_fixed)
		ctx->debug = level;
}

API_EXPORTED int libusb_init(libusb_context **context)
{
	char *dbg = getenv("LIBUSB_DEBUG");
	struct libusb_context *ctx = malloc(sizeof(*ctx));
	int r;

	if (!ctx)
		return LIBUSB_ERROR_NO_MEM;
	memset(ctx, 0, sizeof(*ctx));

	if (dbg) {
		ctx->debug = atoi(dbg);
		if (ctx->debug)
			ctx->debug_fixed = 1;
	}

	usbi_dbg("");

	if (usbi_backend->init) {
		r = usbi_backend->init(ctx);
		if (r)
			goto err;
	}

	pthread_mutex_init(&ctx->usb_devs_lock, NULL);
	pthread_mutex_init(&ctx->open_devs_lock, NULL);
	list_init(&ctx->usb_devs);
	list_init(&ctx->open_devs);

	r = usbi_io_init(ctx);
	if (r < 0) {
		if (usbi_backend->exit)
			usbi_backend->exit();
		goto err;
	}

	pthread_mutex_lock(&default_context_lock);
	if (!usbi_default_context) {
		usbi_dbg("created default context");
		usbi_default_context = ctx;
	}
	pthread_mutex_unlock(&default_context_lock);

	if (context)
		*context = ctx;
	return 0;

err:
	free(ctx);
	return r;
}

API_EXPORTED void libusb_exit(struct libusb_context *ctx)
{
	USBI_GET_CONTEXT(ctx);
	usbi_dbg("");

	if (!list_empty(&ctx->open_devs))
		usbi_warn(ctx, "application left some devices open");

	usbi_io_exit(ctx);
	if (usbi_backend->exit)
		usbi_backend->exit();

	pthread_mutex_lock(&default_context_lock);
	if (ctx == usbi_default_context) {
		usbi_dbg("freeing default context");
		usbi_default_context = NULL;
	}
	pthread_mutex_unlock(&default_context_lock);

	free(ctx);
}

void usbi_log(struct libusb_context *ctx, enum usbi_log_level level,
	const char *function, const char *format, ...)
{
	va_list args;
	FILE *stream = stdout;
	const char *prefix;

#ifndef ENABLE_DEBUG_LOGGING
	USBI_GET_CONTEXT(ctx);
	if (!ctx->debug)
		return;
	if (level == LOG_LEVEL_WARNING && ctx->debug < 2)
		return;
	if (level == LOG_LEVEL_INFO && ctx->debug < 3)
		return;
#endif

	switch (level) {
	case LOG_LEVEL_INFO:
		prefix = "info";
		break;
	case LOG_LEVEL_WARNING:
		stream = stderr;
		prefix = "warning";
		break;
	case LOG_LEVEL_ERROR:
		stream = stderr;
		prefix = "error";
		break;
	case LOG_LEVEL_DEBUG:
		stream = stderr;
		prefix = "debug";
		break;
	default:
		stream = stderr;
		prefix = "unknown";
		break;
	}

	fprintf(stream, "libusb:%s [%s] ", prefix, function);

	va_start (args, format);
	vfprintf(stream, format, args);
	va_end (args);

	fprintf(stream, "\n");
}


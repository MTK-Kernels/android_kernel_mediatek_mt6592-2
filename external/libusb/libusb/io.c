/*
 * I/O functions for libusb
 * Copyright (C) 2007-2009 Daniel Drake <dsd@gentoo.org>
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
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef USBI_TIMERFD_AVAILABLE
#include <sys/timerfd.h>
#endif

#include "libusbi.h"







int usbi_io_init(struct libusb_context *ctx)
{
	int r;

	pthread_mutex_init(&ctx->flying_transfers_lock, NULL);
	pthread_mutex_init(&ctx->pollfds_lock, NULL);
	pthread_mutex_init(&ctx->pollfd_modify_lock, NULL);
	pthread_mutex_init(&ctx->events_lock, NULL);
	pthread_mutex_init(&ctx->event_waiters_lock, NULL);
	pthread_cond_init(&ctx->event_waiters_cond, NULL);
	list_init(&ctx->flying_transfers);
	list_init(&ctx->pollfds);

	
	r = pipe(ctx->ctrl_pipe);
	if (r < 0)
		return LIBUSB_ERROR_OTHER;

	r = usbi_add_pollfd(ctx, ctx->ctrl_pipe[0], POLLIN);
	if (r < 0)
		return r;

#ifdef USBI_TIMERFD_AVAILABLE
	ctx->timerfd = timerfd_create(usbi_backend->get_timerfd_clockid(),
		TFD_NONBLOCK);
	if (ctx->timerfd >= 0) {
		usbi_dbg("using timerfd for timeouts");
		r = usbi_add_pollfd(ctx, ctx->timerfd, POLLIN);
		if (r < 0) {
			close(ctx->timerfd);
			return r;
		}
	} else {
		usbi_dbg("timerfd not available (code %d error %d)", ctx->timerfd, errno);
		ctx->timerfd = -1;
	}
#endif

	return 0;
}

void usbi_io_exit(struct libusb_context *ctx)
{
	usbi_remove_pollfd(ctx, ctx->ctrl_pipe[0]);
	close(ctx->ctrl_pipe[0]);
	close(ctx->ctrl_pipe[1]);
#ifdef USBI_TIMERFD_AVAILABLE
	if (usbi_using_timerfd(ctx)) {
		usbi_remove_pollfd(ctx, ctx->timerfd);
		close(ctx->timerfd);
	}
#endif
}

static int calculate_timeout(struct usbi_transfer *transfer)
{
	int r;
	struct timespec current_time;
	unsigned int timeout =
		__USBI_TRANSFER_TO_LIBUSB_TRANSFER(transfer)->timeout;

	if (!timeout)
		return 0;

	r = usbi_backend->clock_gettime(USBI_CLOCK_MONOTONIC, &current_time);
	if (r < 0) {
		usbi_err(ITRANSFER_CTX(transfer),
			"failed to read monotonic clock, errno=%d", errno);
		return r;
	}

	current_time.tv_sec += timeout / 1000;
	current_time.tv_nsec += (timeout % 1000) * 1000000;

	if (current_time.tv_nsec > 1000000000) {
		current_time.tv_nsec -= 1000000000;
		current_time.tv_sec++;
	}

	TIMESPEC_TO_TIMEVAL(&transfer->timeout, &current_time);
	return 0;
}

static int add_to_flying_list(struct usbi_transfer *transfer)
{
	struct usbi_transfer *cur;
	struct timeval *timeout = &transfer->timeout;
	struct libusb_context *ctx = ITRANSFER_CTX(transfer);
	int r = 0;
	int first = 1;

	pthread_mutex_lock(&ctx->flying_transfers_lock);

	
	if (list_empty(&ctx->flying_transfers)) {
		list_add(&transfer->list, &ctx->flying_transfers);
		if (timerisset(timeout))
			r = 1;
		goto out;
	}

	
	if (!timerisset(timeout)) {
		list_add_tail(&transfer->list, &ctx->flying_transfers);
		goto out;
	}

	
	list_for_each_entry(cur, &ctx->flying_transfers, list) {
		
		struct timeval *cur_tv = &cur->timeout;

		if (!timerisset(cur_tv) || (cur_tv->tv_sec > timeout->tv_sec) ||
				(cur_tv->tv_sec == timeout->tv_sec &&
					cur_tv->tv_usec > timeout->tv_usec)) {
			list_add_tail(&transfer->list, &cur->list);
			r = first;
			goto out;
		}
		first = 0;
	}

	
	list_add_tail(&transfer->list, &ctx->flying_transfers);
out:
	pthread_mutex_unlock(&ctx->flying_transfers_lock);
	return r;
}

API_EXPORTED struct libusb_transfer *libusb_alloc_transfer(int iso_packets)
{
	size_t os_alloc_size = usbi_backend->transfer_priv_size
		+ (usbi_backend->add_iso_packet_size * iso_packets);
	int alloc_size = sizeof(struct usbi_transfer)
		+ sizeof(struct libusb_transfer)
		+ (sizeof(struct libusb_iso_packet_descriptor) * iso_packets)
		+ os_alloc_size;
	struct usbi_transfer *itransfer = malloc(alloc_size);
	if (!itransfer)
		return NULL;

	memset(itransfer, 0, alloc_size);
	itransfer->num_iso_packets = iso_packets;
	pthread_mutex_init(&itransfer->lock, NULL);
	return __USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
}

API_EXPORTED void libusb_free_transfer(struct libusb_transfer *transfer)
{
	struct usbi_transfer *itransfer;
	if (!transfer)
		return;

	if (transfer->flags & LIBUSB_TRANSFER_FREE_BUFFER && transfer->buffer)
		free(transfer->buffer);

	itransfer = __LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer);
	pthread_mutex_destroy(&itransfer->lock);
	free(itransfer);
}

API_EXPORTED int libusb_submit_transfer(struct libusb_transfer *transfer)
{
	struct libusb_context *ctx = TRANSFER_CTX(transfer);
	struct usbi_transfer *itransfer =
		__LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer);
	int r;
	int first;

	pthread_mutex_lock(&itransfer->lock);
	itransfer->transferred = 0;
	itransfer->flags = 0;
	r = calculate_timeout(itransfer);
	if (r < 0) {
		r = LIBUSB_ERROR_OTHER;
		goto out;
	}

	first = add_to_flying_list(itransfer);
	r = usbi_backend->submit_transfer(itransfer);
	if (r) {
		pthread_mutex_lock(&ctx->flying_transfers_lock);
		list_del(&itransfer->list);
		pthread_mutex_unlock(&ctx->flying_transfers_lock);
	}
#ifdef USBI_TIMERFD_AVAILABLE
	else if (first && usbi_using_timerfd(ctx)) {
		const struct itimerspec it = { {0, 0},
			{ itransfer->timeout.tv_sec, itransfer->timeout.tv_usec * 1000 } };
		usbi_dbg("arm timerfd for timeout in %dms (first in line)", transfer->timeout);
		r = timerfd_settime(ctx->timerfd, TFD_TIMER_ABSTIME, &it, NULL);
		if (r < 0)
			r = LIBUSB_ERROR_OTHER;
	}
#endif

out:
	pthread_mutex_unlock(&itransfer->lock);
	return r;
}

API_EXPORTED int libusb_cancel_transfer(struct libusb_transfer *transfer)
{
	struct usbi_transfer *itransfer =
		__LIBUSB_TRANSFER_TO_USBI_TRANSFER(transfer);
	int r;

	usbi_dbg("");
	pthread_mutex_lock(&itransfer->lock);
	r = usbi_backend->cancel_transfer(itransfer);
	if (r < 0)
		usbi_err(TRANSFER_CTX(transfer),
			"cancel transfer failed error %d", r);
	pthread_mutex_unlock(&itransfer->lock);
	return r;
}

#ifdef USBI_TIMERFD_AVAILABLE
static int disarm_timerfd(struct libusb_context *ctx)
{
	const struct itimerspec disarm_timer = { { 0, 0 }, { 0, 0 } };
	int r;

	usbi_dbg("");
	r = timerfd_settime(ctx->timerfd, 0, &disarm_timer, NULL);
	if (r < 0)
		return LIBUSB_ERROR_OTHER;
	else
		return 0;
}

static int arm_timerfd_for_next_timeout(struct libusb_context *ctx)
{
	struct usbi_transfer *transfer;

	list_for_each_entry(transfer, &ctx->flying_transfers, list) {
		struct timeval *cur_tv = &transfer->timeout;

		if (!timerisset(cur_tv))
			return 0;

		
		if (!(transfer->flags & USBI_TRANSFER_TIMED_OUT)) {
			int r;
			const struct itimerspec it = { {0, 0},
				{ cur_tv->tv_sec, cur_tv->tv_usec * 1000 } };
			usbi_dbg("next timeout originally %dms", __USBI_TRANSFER_TO_LIBUSB_TRANSFER(transfer)->timeout);
			r = timerfd_settime(ctx->timerfd, TFD_TIMER_ABSTIME, &it, NULL);
			if (r < 0)
				return LIBUSB_ERROR_OTHER;
			return 1;
		}
	}

	return 0;
}
#else
static int disarm_timerfd(struct libusb_context *ctx)
{
	return 0;
}
static int arm_timerfd_for_next_timeout(struct libusb_context *ctx)
{
	return 0;
}
#endif

int usbi_handle_transfer_completion(struct usbi_transfer *itransfer,
	enum libusb_transfer_status status)
{
	struct libusb_transfer *transfer =
		__USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = TRANSFER_CTX(transfer);
	uint8_t flags;
	int r;


	pthread_mutex_lock(&ctx->flying_transfers_lock);
	list_del(&itransfer->list);
	r = arm_timerfd_for_next_timeout(ctx);
	pthread_mutex_unlock(&ctx->flying_transfers_lock);

	if (r < 0) {
		return r;
	} else if (r == 0) {
		r = disarm_timerfd(ctx);
		if (r < 0)
			return r;
	}

	if (status == LIBUSB_TRANSFER_COMPLETED
			&& transfer->flags & LIBUSB_TRANSFER_SHORT_NOT_OK) {
		int rqlen = transfer->length;
		if (transfer->type == LIBUSB_TRANSFER_TYPE_CONTROL)
			rqlen -= LIBUSB_CONTROL_SETUP_SIZE;
		if (rqlen != itransfer->transferred) {
			usbi_dbg("interpreting short transfer as error");
			status = LIBUSB_TRANSFER_ERROR;
		}
	}

	flags = transfer->flags;
	transfer->status = status;
	transfer->actual_length = itransfer->transferred;
	if (transfer->callback)
		transfer->callback(transfer);
	if (flags & LIBUSB_TRANSFER_FREE_TRANSFER)
		libusb_free_transfer(transfer);
	pthread_mutex_lock(&ctx->event_waiters_lock);
	pthread_cond_broadcast(&ctx->event_waiters_cond);
	pthread_mutex_unlock(&ctx->event_waiters_lock);
	return 0;
}

int usbi_handle_transfer_cancellation(struct usbi_transfer *transfer)
{
	
	if (transfer->flags & USBI_TRANSFER_TIMED_OUT) {
		usbi_dbg("detected timeout cancellation");
		return usbi_handle_transfer_completion(transfer, LIBUSB_TRANSFER_TIMED_OUT);
	}

	
	return usbi_handle_transfer_completion(transfer, LIBUSB_TRANSFER_CANCELLED);
}

API_EXPORTED int libusb_try_lock_events(libusb_context *ctx)
{
	int r;
	USBI_GET_CONTEXT(ctx);

	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	r = ctx->pollfd_modify;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);
	if (r) {
		usbi_dbg("someone else is modifying poll fds");
		return 1;
	}

	r = pthread_mutex_trylock(&ctx->events_lock);
	if (r)
		return 1;

	ctx->event_handler_active = 1;	
	return 0;
}

API_EXPORTED void libusb_lock_events(libusb_context *ctx)
{
	USBI_GET_CONTEXT(ctx);
	pthread_mutex_lock(&ctx->events_lock);
	ctx->event_handler_active = 1;
}

API_EXPORTED void libusb_unlock_events(libusb_context *ctx)
{
	USBI_GET_CONTEXT(ctx);
	ctx->event_handler_active = 0;
	pthread_mutex_unlock(&ctx->events_lock);

	pthread_mutex_lock(&ctx->event_waiters_lock);
	pthread_cond_broadcast(&ctx->event_waiters_cond);
	pthread_mutex_unlock(&ctx->event_waiters_lock);
}

API_EXPORTED int libusb_event_handling_ok(libusb_context *ctx)
{
	int r;
	USBI_GET_CONTEXT(ctx);

	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	r = ctx->pollfd_modify;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);
	if (r) {
		usbi_dbg("someone else is modifying poll fds");
		return 0;
	}

	return 1;
}


API_EXPORTED int libusb_event_handler_active(libusb_context *ctx)
{
	int r;
	USBI_GET_CONTEXT(ctx);

	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	r = ctx->pollfd_modify;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);
	if (r) {
		usbi_dbg("someone else is modifying poll fds");
		return 1;
	}

	return ctx->event_handler_active;
}

API_EXPORTED void libusb_lock_event_waiters(libusb_context *ctx)
{
	USBI_GET_CONTEXT(ctx);
	pthread_mutex_lock(&ctx->event_waiters_lock);
}

API_EXPORTED void libusb_unlock_event_waiters(libusb_context *ctx)
{
	USBI_GET_CONTEXT(ctx);
	pthread_mutex_unlock(&ctx->event_waiters_lock);
}

API_EXPORTED int libusb_wait_for_event(libusb_context *ctx, struct timeval *tv)
{
	struct timespec timeout;
	int r;

	USBI_GET_CONTEXT(ctx);
	if (tv == NULL) {
		pthread_cond_wait(&ctx->event_waiters_cond, &ctx->event_waiters_lock);
		return 0;
	}

	r = usbi_backend->clock_gettime(USBI_CLOCK_REALTIME, &timeout);
	if (r < 0) {
		usbi_err(ctx, "failed to read realtime clock, error %d", errno);
		return LIBUSB_ERROR_OTHER;
	}

	timeout.tv_sec += tv->tv_sec;
	timeout.tv_nsec += tv->tv_usec * 1000;
	if (timeout.tv_nsec > 1000000000) {
		timeout.tv_nsec -= 1000000000;
		timeout.tv_sec++;
	}

	r = pthread_cond_timedwait(&ctx->event_waiters_cond,
		&ctx->event_waiters_lock, &timeout);
	return (r == ETIMEDOUT);
}

static void handle_timeout(struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer =
		__USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	int r;

	itransfer->flags |= USBI_TRANSFER_TIMED_OUT;
	r = libusb_cancel_transfer(transfer);
	if (r < 0)
		usbi_warn(TRANSFER_CTX(transfer),
			"async cancel failed %d errno=%d", r, errno);
}

#ifdef USBI_OS_HANDLES_TIMEOUT
static int handle_timeouts_locked(struct libusb_context *ctx)
{
	return 0;
}
static int handle_timeouts(struct libusb_context *ctx)
{
	return 0;
}
#else
static int handle_timeouts_locked(struct libusb_context *ctx)
{
	int r;
	struct timespec systime_ts;
	struct timeval systime;
	struct usbi_transfer *transfer;

	if (list_empty(&ctx->flying_transfers))
		return 0;

	
	r = usbi_backend->clock_gettime(USBI_CLOCK_MONOTONIC, &systime_ts);
	if (r < 0)
		return r;

	TIMESPEC_TO_TIMEVAL(&systime, &systime_ts);

	list_for_each_entry(transfer, &ctx->flying_transfers, list) {
		struct timeval *cur_tv = &transfer->timeout;

		
		if (!timerisset(cur_tv))
			return 0;

		
		if (transfer->flags & USBI_TRANSFER_TIMED_OUT)
			continue;

		
		if ((cur_tv->tv_sec > systime.tv_sec) ||
				(cur_tv->tv_sec == systime.tv_sec &&
					cur_tv->tv_usec > systime.tv_usec))
			return 0;
	
		
		handle_timeout(transfer);
	}
	return 0;
}

static int handle_timeouts(struct libusb_context *ctx)
{
	int r;
	USBI_GET_CONTEXT(ctx);
	pthread_mutex_lock(&ctx->flying_transfers_lock);
	r = handle_timeouts_locked(ctx);
	pthread_mutex_unlock(&ctx->flying_transfers_lock);
	return r;
}
#endif

#ifdef USBI_TIMERFD_AVAILABLE
static int handle_timerfd_trigger(struct libusb_context *ctx)
{
	int r;

	r = disarm_timerfd(ctx);
	if (r < 0)
		return r;

	pthread_mutex_lock(&ctx->flying_transfers_lock);

	
	r = handle_timeouts_locked(ctx);
	if (r < 0)
		goto out;

	
	r = arm_timerfd_for_next_timeout(ctx);

out:
	pthread_mutex_unlock(&ctx->flying_transfers_lock);
	return r;
}
#endif

static int handle_events(struct libusb_context *ctx, struct timeval *tv)
{
	int r;
	struct usbi_pollfd *ipollfd;
	nfds_t nfds = 0;
	struct pollfd *fds;
	int i = -1;
	int timeout_ms;

	pthread_mutex_lock(&ctx->pollfds_lock);
	list_for_each_entry(ipollfd, &ctx->pollfds, list)
		nfds++;

	
	fds = malloc(sizeof(*fds) * nfds);
	if (!fds)
		return LIBUSB_ERROR_NO_MEM;

	list_for_each_entry(ipollfd, &ctx->pollfds, list) {
		struct libusb_pollfd *pollfd = &ipollfd->pollfd;
		int fd = pollfd->fd;
		i++;
		fds[i].fd = fd;
		fds[i].events = pollfd->events;
		fds[i].revents = 0;
	}
	pthread_mutex_unlock(&ctx->pollfds_lock);

	timeout_ms = (tv->tv_sec * 1000) + (tv->tv_usec / 1000);

	
	if (tv->tv_usec % 1000)
		timeout_ms++;

	usbi_dbg("poll() %d fds with timeout in %dms", nfds, timeout_ms);
	r = poll(fds, nfds, timeout_ms);
	usbi_dbg("poll() returned %d", r);
	if (r == 0) {
		free(fds);
		return handle_timeouts(ctx);
	} else if (r == -1 && errno == EINTR) {
		free(fds);
		return LIBUSB_ERROR_INTERRUPTED;
	} else if (r < 0) {
		free(fds);
		usbi_err(ctx, "poll failed %d err=%d\n", r, errno);
		return LIBUSB_ERROR_IO;
	}

	
	if (fds[0].revents) {
		usbi_dbg("caught a fish on the control pipe");

		if (r == 1) {
			r = 0;
			goto handled;
		} else {
			
			fds[0].revents = 0;
			r--;
		}
	}

#ifdef USBI_TIMERFD_AVAILABLE
	
	if (usbi_using_timerfd(ctx) && fds[1].revents) {
		
		int ret;
		usbi_dbg("timerfd triggered");

		ret = handle_timerfd_trigger(ctx);
		if (ret < 0) {
			
			r = ret;
			goto handled;
		} else if (r == 1) {
			
			r = 0;
			goto handled;
		} else {
			fds[1].revents = 0;
			r--;
		}
	}
#endif

	r = usbi_backend->handle_events(ctx, fds, nfds, r);
	if (r)
		usbi_err(ctx, "backend handle_events failed with error %d", r);

handled:
	free(fds);
	return r;
}

static int get_next_timeout(libusb_context *ctx, struct timeval *tv,
	struct timeval *out)
{
	struct timeval timeout;
	int r = libusb_get_next_timeout(ctx, &timeout);
	if (r) {
		
		if (!timerisset(&timeout))
			return 1;

		
		if (timercmp(&timeout, tv, <))
			*out = timeout;
		else
			*out = *tv;
	} else {
		*out = *tv;
	}
	return 0;
}

API_EXPORTED int libusb_handle_events_timeout(libusb_context *ctx,
	struct timeval *tv)
{
	int r;
	struct timeval poll_timeout;

	USBI_GET_CONTEXT(ctx);
	r = get_next_timeout(ctx, tv, &poll_timeout);
	if (r) {
		
		return handle_timeouts(ctx);
	}

retry:
	if (libusb_try_lock_events(ctx) == 0) {
		
		r = handle_events(ctx, &poll_timeout);
		libusb_unlock_events(ctx);
		return r;
	}

	libusb_lock_event_waiters(ctx);

	if (!libusb_event_handler_active(ctx)) {
		libusb_unlock_event_waiters(ctx);
		usbi_dbg("event handler was active but went away, retrying");
		goto retry;
	}

	usbi_dbg("another thread is doing event handling");
	r = libusb_wait_for_event(ctx, &poll_timeout);
	libusb_unlock_event_waiters(ctx);

	if (r < 0)
		return r;
	else if (r == 1)
		return handle_timeouts(ctx);
	else
		return 0;
}

API_EXPORTED int libusb_handle_events(libusb_context *ctx)
{
	struct timeval tv;
	tv.tv_sec = 60;
	tv.tv_usec = 0;
	return libusb_handle_events_timeout(ctx, &tv);
}

API_EXPORTED int libusb_handle_events_locked(libusb_context *ctx,
	struct timeval *tv)
{
	int r;
	struct timeval poll_timeout;

	USBI_GET_CONTEXT(ctx);
	r = get_next_timeout(ctx, tv, &poll_timeout);
	if (r) {
		
		return handle_timeouts(ctx);
	}

	return handle_events(ctx, &poll_timeout);
}

API_EXPORTED int libusb_pollfds_handle_timeouts(libusb_context *ctx)
{
#if defined(USBI_OS_HANDLES_TIMEOUT)
	return 1;
#elif defined(USBI_TIMERFD_AVAILABLE)
	USBI_GET_CONTEXT(ctx);
	return usbi_using_timerfd(ctx);
#else
	return 0;
#endif
}

API_EXPORTED int libusb_get_next_timeout(libusb_context *ctx,
	struct timeval *tv)
{
#ifndef USBI_OS_HANDLES_TIMEOUT
	struct usbi_transfer *transfer;
	struct timespec cur_ts;
	struct timeval cur_tv;
	struct timeval *next_timeout;
	int r;
	int found = 0;

	USBI_GET_CONTEXT(ctx);
	if (usbi_using_timerfd(ctx))
		return 0;

	pthread_mutex_lock(&ctx->flying_transfers_lock);
	if (list_empty(&ctx->flying_transfers)) {
		pthread_mutex_unlock(&ctx->flying_transfers_lock);
		usbi_dbg("no URBs, no timeout!");
		return 0;
	}

	
	list_for_each_entry(transfer, &ctx->flying_transfers, list) {
		if (!(transfer->flags & USBI_TRANSFER_TIMED_OUT)) {
			found = 1;
			break;
		}
	}
	pthread_mutex_unlock(&ctx->flying_transfers_lock);

	if (!found) {
		usbi_dbg("all URBs have already been processed for timeouts");
		return 0;
	}

	next_timeout = &transfer->timeout;

	
	if (!timerisset(next_timeout)) {
		usbi_dbg("no URBs with timeouts, no timeout!");
		return 0;
	}

	r = usbi_backend->clock_gettime(USBI_CLOCK_MONOTONIC, &cur_ts);
	if (r < 0) {
		usbi_err(ctx, "failed to read monotonic clock, errno=%d", errno);
		return LIBUSB_ERROR_OTHER;
	}
	TIMESPEC_TO_TIMEVAL(&cur_tv, &cur_ts);

	if (timercmp(&cur_tv, next_timeout, >=)) {
		usbi_dbg("first timeout already expired");
		timerclear(tv);
	} else {
		timersub(next_timeout, &cur_tv, tv);
		usbi_dbg("next timeout in %d.%06ds", tv->tv_sec, tv->tv_usec);
	}

	return 1;
#else
	return 0;
#endif
}

API_EXPORTED void libusb_set_pollfd_notifiers(libusb_context *ctx,
	libusb_pollfd_added_cb added_cb, libusb_pollfd_removed_cb removed_cb,
	void *user_data)
{
	USBI_GET_CONTEXT(ctx);
	ctx->fd_added_cb = added_cb;
	ctx->fd_removed_cb = removed_cb;
	ctx->fd_cb_user_data = user_data;
}

int usbi_add_pollfd(struct libusb_context *ctx, int fd, short events)
{
	struct usbi_pollfd *ipollfd = malloc(sizeof(*ipollfd));
	if (!ipollfd)
		return LIBUSB_ERROR_NO_MEM;

	usbi_dbg("add fd %d events %d", fd, events);
	ipollfd->pollfd.fd = fd;
	ipollfd->pollfd.events = events;
	pthread_mutex_lock(&ctx->pollfds_lock);
	list_add_tail(&ipollfd->list, &ctx->pollfds);
	pthread_mutex_unlock(&ctx->pollfds_lock);

	if (ctx->fd_added_cb)
		ctx->fd_added_cb(fd, events, ctx->fd_cb_user_data);
	return 0;
}

void usbi_remove_pollfd(struct libusb_context *ctx, int fd)
{
	struct usbi_pollfd *ipollfd;
	int found = 0;

	usbi_dbg("remove fd %d", fd);
	pthread_mutex_lock(&ctx->pollfds_lock);
	list_for_each_entry(ipollfd, &ctx->pollfds, list)
		if (ipollfd->pollfd.fd == fd) {
			found = 1;
			break;
		}

	if (!found) {
		usbi_dbg("couldn't find fd %d to remove", fd);
		pthread_mutex_unlock(&ctx->pollfds_lock);
		return;
	}

	list_del(&ipollfd->list);
	pthread_mutex_unlock(&ctx->pollfds_lock);
	free(ipollfd);
	if (ctx->fd_removed_cb)
		ctx->fd_removed_cb(fd, ctx->fd_cb_user_data);
}

API_EXPORTED const struct libusb_pollfd **libusb_get_pollfds(
	libusb_context *ctx)
{
	struct libusb_pollfd **ret = NULL;
	struct usbi_pollfd *ipollfd;
	size_t i = 0;
	size_t cnt = 0;
	USBI_GET_CONTEXT(ctx);

	pthread_mutex_lock(&ctx->pollfds_lock);
	list_for_each_entry(ipollfd, &ctx->pollfds, list)
		cnt++;

	ret = calloc(cnt + 1, sizeof(struct libusb_pollfd *));
	if (!ret)
		goto out;

	list_for_each_entry(ipollfd, &ctx->pollfds, list)
		ret[i++] = (struct libusb_pollfd *) ipollfd;
	ret[cnt] = NULL;

out:
	pthread_mutex_unlock(&ctx->pollfds_lock);
	return (const struct libusb_pollfd **) ret;
}

void usbi_handle_disconnect(struct libusb_device_handle *handle)
{
	struct usbi_transfer *cur;
	struct usbi_transfer *to_cancel;

	usbi_dbg("device %d.%d",
		handle->dev->bus_number, handle->dev->device_address);


	while (1) {
		pthread_mutex_lock(&HANDLE_CTX(handle)->flying_transfers_lock);
		to_cancel = NULL;
		list_for_each_entry(cur, &HANDLE_CTX(handle)->flying_transfers, list)
			if (__USBI_TRANSFER_TO_LIBUSB_TRANSFER(cur)->dev_handle == handle) {
				to_cancel = cur;
				break;
			}
		pthread_mutex_unlock(&HANDLE_CTX(handle)->flying_transfers_lock);

		if (!to_cancel)
			break;

		usbi_backend->clear_transfer_priv(to_cancel);
		usbi_handle_transfer_completion(to_cancel, LIBUSB_TRANSFER_NO_DEVICE);
	}

}


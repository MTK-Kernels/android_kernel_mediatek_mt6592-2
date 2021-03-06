/* ptp.c
 *
 * Copyright (C) 2001-2004 Mariusz Woloszyn <emsi@ipartners.pl>
 * Copyright (C) 2003-2009 Marcus Meissner <marcus@jet.franken.de>
 * Copyright (C) 2006-2008 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2007 Tero Saarni <tero.saarni@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#define _BSD_SOURCE
#include "config.h"
#include "ptp.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CHECK_PTP_RC(result)	{uint16_t r=(result); if (r!=PTP_RC_OK) return r;}

#define PTP_CNT_INIT(cnt) {memset(&cnt,0,sizeof(cnt));}

static uint16_t ptp_exit_recv_memory_handler (PTPDataHandler*,unsigned char**,unsigned long*);
static uint16_t ptp_init_recv_memory_handler(PTPDataHandler*);
static uint16_t ptp_init_send_memory_handler(PTPDataHandler*,unsigned char*,unsigned long len);
static uint16_t ptp_exit_send_memory_handler (PTPDataHandler *handler);

static void
ptp_debug (PTPParams *params, const char *format, ...)
{  
        va_list args;

        va_start (args, format);
        if (params->debug_func!=NULL)
                params->debug_func (params->data, format, args);
        else
	{
                vfprintf (stderr, format, args);
		fprintf (stderr,"\n");
		fflush (stderr);
	}
        va_end (args);
}  

static void
ptp_error (PTPParams *params, const char *format, ...)
{  
        va_list args;

        va_start (args, format);
        if (params->error_func!=NULL)
                params->error_func (params->data, format, args);
        else
	{
                vfprintf (stderr, format, args);
		fprintf (stderr,"\n");
		fflush (stderr);
	}
        va_end (args);
}


#include "ptp-pack.c"


#define PTP_DP_NODATA		0x0000	
#define PTP_DP_SENDDATA		0x0001	
#define PTP_DP_GETDATA		0x0002	
#define PTP_DP_DATA_MASK	0x00ff	

static uint16_t
ptp_transaction_new (PTPParams* params, PTPContainer* ptp, 
		uint16_t flags, unsigned int sendlen,
		PTPDataHandler *handler
) {
	int tries;

	if ((params==NULL) || (ptp==NULL)) 
		return PTP_ERROR_BADPARAM;

	ptp->Transaction_ID=params->transaction_id++;
	ptp->SessionID=params->session_id;
	
	CHECK_PTP_RC(params->sendreq_func (params, ptp));
	
	switch (flags&PTP_DP_DATA_MASK) {
	case PTP_DP_SENDDATA:
		{
			uint16_t ret;
			ret = params->senddata_func(params, ptp,
						    sendlen, handler);
			if (ret == PTP_ERROR_CANCEL) {
				ret = params->cancelreq_func(params, 
							     params->transaction_id-1);
				if (ret == PTP_RC_OK)
					ret = PTP_ERROR_CANCEL;
			}
			if (ret != PTP_RC_OK)
				return ret;
		}
		break;
	case PTP_DP_GETDATA:
		{
			uint16_t ret;
			ret = params->getdata_func(params, ptp, handler);
			if (ret == PTP_ERROR_CANCEL) {
				ret = params->cancelreq_func(params, 
							     params->transaction_id-1);
				if (ret == PTP_RC_OK)
					ret = PTP_ERROR_CANCEL;
			}
			if (ret != PTP_RC_OK)
				return ret;
		}
		break;
	case PTP_DP_NODATA:
		break;
	default:
		return PTP_ERROR_BADPARAM;
	}
	tries = 3;
	while (1) {
		
		CHECK_PTP_RC(params->getresp_func(params, ptp));
		if (ptp->Transaction_ID != params->transaction_id-1) {
			
			if ((ptp->Code == PTP_OC_OpenSession) && tries--)
				continue;
			ptp_error (params,
				"PTP: Sequence number mismatch %d vs expected %d.",
				ptp->Transaction_ID, params->transaction_id-1
			);
			return PTP_ERROR_BADPARAM;
		}
		break;
	}
	return ptp->Code;
}

typedef struct {
	unsigned char	*data;
	unsigned long	size, curoff;
} PTPMemHandlerPrivate;

static uint16_t
memory_getfunc(PTPParams* params, void* private,
	       unsigned long wantlen, unsigned char *data,
	       unsigned long *gotlen
) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)private;
	unsigned long tocopy = wantlen;

	if (priv->curoff + tocopy > priv->size)
		tocopy = priv->size - priv->curoff;
	memcpy (data, priv->data + priv->curoff, tocopy);
	priv->curoff += tocopy;
	*gotlen = tocopy;
	return PTP_RC_OK;
}

static uint16_t
memory_putfunc(PTPParams* params, void* private,
	       unsigned long sendlen, unsigned char *data,
	       unsigned long *putlen
) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)private;

	if (priv->curoff + sendlen > priv->size) {
		priv->data = realloc (priv->data, priv->curoff+sendlen);
		priv->size = priv->curoff + sendlen;
	}
	memcpy (priv->data + priv->curoff, data, sendlen);
	priv->curoff += sendlen;
	*putlen = sendlen;
	return PTP_RC_OK;
}

static uint16_t
ptp_init_recv_memory_handler(PTPDataHandler *handler) {
	PTPMemHandlerPrivate* priv;
	priv = malloc (sizeof(PTPMemHandlerPrivate));
	handler->priv = priv;
	handler->getfunc = memory_getfunc;
	handler->putfunc = memory_putfunc;
	priv->data = NULL;
	priv->size = 0;
	priv->curoff = 0;
	return PTP_RC_OK;
}

static uint16_t
ptp_init_send_memory_handler(PTPDataHandler *handler,
	unsigned char *data, unsigned long len
) {
	PTPMemHandlerPrivate* priv;
	priv = malloc (sizeof(PTPMemHandlerPrivate));
	if (!priv)
		return PTP_RC_GeneralError;
	handler->priv = priv;
	handler->getfunc = memory_getfunc;
	handler->putfunc = memory_putfunc;
	priv->data = data;
	priv->size = len;
	priv->curoff = 0;
	return PTP_RC_OK;
}

static uint16_t
ptp_exit_send_memory_handler (PTPDataHandler *handler) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)handler->priv;
	
	free (priv);
	return PTP_RC_OK;
}

static uint16_t
ptp_exit_recv_memory_handler (PTPDataHandler *handler,
	unsigned char **data, unsigned long *size
) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)handler->priv;
	*data = priv->data;
	*size = priv->size;
	free (priv);
	return PTP_RC_OK;
}

typedef struct {
	int fd;
} PTPFDHandlerPrivate;

static uint16_t
fd_getfunc(PTPParams* params, void* private,
	       unsigned long wantlen, unsigned char *data,
	       unsigned long *gotlen
) {
	PTPFDHandlerPrivate* priv = (PTPFDHandlerPrivate*)private;
	int		got;

	got = read (priv->fd, data, wantlen);
	if (got != -1)
		*gotlen = got;
	else
		return PTP_RC_GeneralError;
	return PTP_RC_OK;
}

static uint16_t
fd_putfunc(PTPParams* params, void* private,
	       unsigned long sendlen, unsigned char *data,
	       unsigned long *putlen
) {
	int		written;
	PTPFDHandlerPrivate* priv = (PTPFDHandlerPrivate*)private;

	written = write (priv->fd, data, sendlen);
	if (written != -1)
		*putlen = written;
	else
		return PTP_RC_GeneralError;
	return PTP_RC_OK;
}

static uint16_t
ptp_init_fd_handler(PTPDataHandler *handler, int fd) {
	PTPFDHandlerPrivate* priv;
	priv = malloc (sizeof(PTPFDHandlerPrivate));
	handler->priv = priv;
	handler->getfunc = fd_getfunc;
	handler->putfunc = fd_putfunc;
	priv->fd = fd;
	return PTP_RC_OK;
}

static uint16_t
ptp_exit_fd_handler (PTPDataHandler *handler) {
	PTPFDHandlerPrivate* priv = (PTPFDHandlerPrivate*)handler->priv;
	free (priv);
	return PTP_RC_OK;
}

static uint16_t
ptp_transaction (PTPParams* params, PTPContainer* ptp, 
		uint16_t flags, unsigned int sendlen,
		unsigned char **data, unsigned int *recvlen
) {
	PTPDataHandler	handler;
	uint16_t	ret;

	switch (flags & PTP_DP_DATA_MASK) {
	case PTP_DP_SENDDATA:
		ptp_init_send_memory_handler (&handler, *data, sendlen);
		break;
	case PTP_DP_GETDATA:
		ptp_init_recv_memory_handler (&handler);
		break;
	default:break;
	}
	ret = ptp_transaction_new (params, ptp, flags, sendlen, &handler);
	switch (flags & PTP_DP_DATA_MASK) {
	case PTP_DP_SENDDATA:
		ptp_exit_send_memory_handler (&handler);
		break;
	case PTP_DP_GETDATA: {
		unsigned long len;
		ptp_exit_recv_memory_handler (&handler, data, &len);
		if (recvlen)
			*recvlen = len;
		break;
	}
	default:break;
	}
	return ret;
}




uint16_t
ptp_getdeviceinfo (PTPParams* params, PTPDeviceInfo* deviceinfo)
{
	uint16_t 	ret;
	unsigned long	len;
	PTPContainer	ptp;
	unsigned char*	di=NULL;
	PTPDataHandler	handler;

	ptp_init_recv_memory_handler (&handler);
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetDeviceInfo;
	ptp.Nparam=0;
	len=0;
	ret=ptp_transaction_new(params, &ptp, PTP_DP_GETDATA, 0, &handler);
	ptp_exit_recv_memory_handler (&handler, &di, &len);
	if (!di) ret = PTP_RC_GeneralError;
	if (ret == PTP_RC_OK) ptp_unpack_DI(params, di, deviceinfo, len);
	free(di);
	return ret;
}

uint16_t
ptp_canon_eos_getdeviceinfo (PTPParams* params, PTPCanonEOSDeviceInfo*di)
{
	uint16_t 	ret;
	PTPContainer	ptp;
	PTPDataHandler	handler;
	unsigned long	len;
	unsigned char	*data;

	ptp_init_recv_memory_handler (&handler);
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_EOS_GetDeviceInfoEx;
	ptp.Nparam=0;
	len=0;
	data=NULL;
	ret=ptp_transaction_new(params, &ptp, PTP_DP_GETDATA, 0, &handler);
	ptp_exit_recv_memory_handler (&handler, &data, &len);
	if (ret == PTP_RC_OK) ptp_unpack_EOS_DI(params, data, di, len);
	free (data);
	return ret;
}

uint16_t
ptp_generic_no_data (PTPParams* params, uint16_t code, unsigned int n_param, ...)
{
	PTPContainer ptp;
	va_list args;
	int i;

	if( n_param > 5 )
		return PTP_RC_InvalidParameter;

	PTP_CNT_INIT(ptp);
	ptp.Code=code;
	ptp.Nparam=n_param;

	va_start(args, n_param);
	for( i=0; i<n_param; ++i )
		(&ptp.Param1)[i] = va_arg(args, uint32_t);
	va_end(args);

	return ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
}

uint16_t
ptp_opensession (PTPParams* params, uint32_t session)
{
	uint16_t ret;
	PTPContainer ptp;

	ptp_debug(params,"PTP: Opening session");

	params->session_id=0x00000000;
	
	params->transaction_id=0x0000000;
	
	params->response_packet = NULL;
	params->response_packet_size = 0;
	
	params->split_header_data = 0;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_OpenSession;
	ptp.Param1=session;
	ptp.Nparam=1;
	ret=ptp_transaction_new(params, &ptp, PTP_DP_NODATA, 0, NULL);
	
	params->session_id=session;
	return ret;
}

void
ptp_free_params (PTPParams *params) {
	int i;

	if (params->cameraname) free (params->cameraname);
	if (params->wifi_profiles) free (params->wifi_profiles);
	for (i=0;i<params->nrofobjects;i++)
		ptp_free_object (&params->objects[i]);
	free (params->objects);
	ptp_free_DI (&params->deviceinfo);
}

uint16_t
ptp_getstorageids (PTPParams* params, PTPStorageIDs* storageids)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned int len;
	unsigned char* sids=NULL;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetStorageIDs;
	ptp.Nparam=0;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &sids, &len);
	if (ret == PTP_RC_OK) ptp_unpack_SIDs(params, sids, storageids, len);
	free(sids);
	return ret;
}

uint16_t
ptp_getstorageinfo (PTPParams* params, uint32_t storageid,
			PTPStorageInfo* storageinfo)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* si=NULL;
	unsigned int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetStorageInfo;
	ptp.Param1=storageid;
	ptp.Nparam=1;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &si, &len);
	if (ret == PTP_RC_OK) ptp_unpack_SI(params, si, storageinfo, len);
	free(si);
	return ret;
}

uint16_t
ptp_getobjecthandles (PTPParams* params, uint32_t storage,
			uint32_t objectformatcode, uint32_t associationOH,
			PTPObjectHandles* objecthandles)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* oh=NULL;
	unsigned int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObjectHandles;
	ptp.Param1=storage;
	ptp.Param2=objectformatcode;
	ptp.Param3=associationOH;
	ptp.Nparam=3;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &oh, &len);
	if (ret == PTP_RC_OK) {
		ptp_unpack_OH(params, oh, objecthandles, len);
	} else {
		if (	(storage == 0xffffffff) &&
			(objectformatcode == 0) &&
			(associationOH == 0)
		) {
			objecthandles->Handler = NULL;
			objecthandles->n = 0;
			ret = PTP_RC_OK;
		}
	}
	free(oh);
	return ret;
}

uint16_t
ptp_getnumobjects (PTPParams* params, uint32_t storage,
			uint32_t objectformatcode, uint32_t associationOH,
			uint32_t* numobs)
{
	uint16_t ret;
	PTPContainer ptp;
	int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetNumObjects;
	ptp.Param1=storage;
	ptp.Param2=objectformatcode;
	ptp.Param3=associationOH;
	ptp.Nparam=3;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
	if (ret == PTP_RC_OK) {
		if (ptp.Nparam >= 1)
			*numobs = ptp.Param1;
		else
			ret = PTP_RC_GeneralError;
	}
	return ret;
}

uint16_t
ptp_getobjectinfo (PTPParams* params, uint32_t handle,
			PTPObjectInfo* objectinfo)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* oi=NULL;
	unsigned int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObjectInfo;
	ptp.Param1=handle;
	ptp.Nparam=1;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &oi, &len);
	if (ret == PTP_RC_OK) ptp_unpack_OI(params, oi, objectinfo, len);
	free(oi);
	return ret;
}

uint16_t
ptp_getobject (PTPParams* params, uint32_t handle, unsigned char** object)
{
	PTPContainer ptp;
	unsigned int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObject;
	ptp.Param1=handle;
	ptp.Nparam=1;
	len=0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, &len);
}

uint16_t
ptp_getobject_to_handler (PTPParams* params, uint32_t handle, PTPDataHandler *handler)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObject;
	ptp.Param1=handle;
	ptp.Nparam=1;
	return ptp_transaction_new(params, &ptp, PTP_DP_GETDATA, 0, handler);
}

uint16_t
ptp_getobject_tofd (PTPParams* params, uint32_t handle, int fd)
{
	PTPContainer	ptp;
	PTPDataHandler	handler;
	uint16_t	ret;

	ptp_init_fd_handler (&handler, fd);
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObject;
	ptp.Param1=handle;
	ptp.Nparam=1;
	ret = ptp_transaction_new(params, &ptp, PTP_DP_GETDATA, 0, &handler);
	ptp_exit_fd_handler (&handler);
	return ret;
}

uint16_t
ptp_getpartialobject (PTPParams* params, uint32_t handle, uint32_t offset,
			uint32_t maxbytes, unsigned char** object)
{
	PTPContainer ptp;
	unsigned int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetPartialObject;
	ptp.Param1=handle;
	ptp.Param2=offset;
	ptp.Param3=maxbytes;
	ptp.Nparam=3;
	len=0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, &len);
}

uint16_t
ptp_getthumb (PTPParams* params, uint32_t handle, unsigned char** object)
{
	PTPContainer ptp;
	unsigned int len;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetThumb;
	ptp.Param1=handle;
	ptp.Nparam=1;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, object, &len);
}

uint16_t
ptp_deleteobject (PTPParams* params, uint32_t handle, uint32_t ofc)
{
	PTPContainer ptp;
	uint16_t ret;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_DeleteObject;
	ptp.Param1=handle;
	ptp.Param2=ofc;
	ptp.Nparam=2;
	ret = ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
	if (ret != PTP_RC_OK) {
		return ret;
	}
	
	ptp_remove_object_from_cache(params, handle);
	return PTP_RC_OK;
}

uint16_t
ptp_sendobjectinfo (PTPParams* params, uint32_t* store, 
			uint32_t* parenthandle, uint32_t* handle,
			PTPObjectInfo* objectinfo)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* oidata=NULL;
	uint32_t size;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SendObjectInfo;
	ptp.Param1=*store;
	ptp.Param2=*parenthandle;
	ptp.Nparam=2;
	
	size=ptp_pack_OI(params, objectinfo, &oidata);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &oidata, NULL); 
	free(oidata);
	*store=ptp.Param1;
	*parenthandle=ptp.Param2;
	*handle=ptp.Param3; 
	return ret;
}

uint16_t
ptp_sendobject (PTPParams* params, unsigned char* object, uint32_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SendObject;
	ptp.Nparam=0;

	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &object, NULL);
}

uint16_t
ptp_sendobject_from_handler (PTPParams* params, PTPDataHandler *handler, uint32_t size)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SendObject;
	ptp.Nparam=0;
	return ptp_transaction_new(params, &ptp, PTP_DP_SENDDATA, size, handler);
}


uint16_t
ptp_sendobject_fromfd (PTPParams* params, int fd, uint32_t size)
{
	PTPContainer	ptp;
	PTPDataHandler	handler;
	uint16_t	ret;

	ptp_init_fd_handler (&handler, fd);
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SendObject;
	ptp.Nparam=0;
	ret = ptp_transaction_new(params, &ptp, PTP_DP_SENDDATA, size, &handler);
	ptp_exit_fd_handler (&handler);
	return ret;
}


uint16_t
ptp_getdevicepropdesc (PTPParams* params, uint16_t propcode, 
			PTPDevicePropDesc* devicepropertydesc)
{
	PTPContainer ptp;
	uint16_t ret;
	unsigned int len;
	unsigned char* dpd=NULL;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetDevicePropDesc;
	ptp.Param1=propcode;
	ptp.Nparam=1;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dpd, &len);
	if (ret == PTP_RC_OK) ptp_unpack_DPD(params, dpd, devicepropertydesc, len);
	free(dpd);
	return ret;
}


uint16_t
ptp_getdevicepropvalue (PTPParams* params, uint16_t propcode,
			PTPPropertyValue* value, uint16_t datatype)
{
	PTPContainer ptp;
	uint16_t ret;
	unsigned int len;
	int offset;
	unsigned char* dpv=NULL;


	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetDevicePropValue;
	ptp.Param1=propcode;
	ptp.Nparam=1;
	len=offset=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dpv, &len);
	if (ret == PTP_RC_OK) ptp_unpack_DPV(params, dpv, &offset, len, value, datatype);
	free(dpv);
	return ret;
}

uint16_t
ptp_setdevicepropvalue (PTPParams* params, uint16_t propcode,
			PTPPropertyValue *value, uint16_t datatype)
{
	PTPContainer ptp;
	uint16_t ret;
	uint32_t size;
	unsigned char* dpv=NULL;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SetDevicePropValue;
	ptp.Param1=propcode;
	ptp.Nparam=1;
	size=ptp_pack_DPV(params, value, &dpv, datatype);
	ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &dpv, NULL);
	free(dpv);
	return ret;
}

uint16_t
ptp_ek_sendfileobjectinfo (PTPParams* params, uint32_t* store, 
			uint32_t* parenthandle, uint32_t* handle,
			PTPObjectInfo* objectinfo)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* oidata=NULL;
	uint32_t size;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_EK_SendFileObjectInfo;
	ptp.Param1=*store;
	ptp.Param2=*parenthandle;
	ptp.Nparam=2;
	
	size=ptp_pack_OI(params, objectinfo, &oidata);
	ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &oidata, NULL); 
	free(oidata);
	*store=ptp.Param1;
	*parenthandle=ptp.Param2;
	*handle=ptp.Param3; 
	return ret;
}

uint16_t
ptp_ek_getserial (PTPParams* params, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_EK_GetSerial;
	ptp.Nparam = 0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

uint16_t
ptp_ek_setserial (PTPParams* params, unsigned char *data, unsigned int size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_EK_SetSerial;
	ptp.Nparam = 0;
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL); 
}

uint16_t
ptp_ek_9007 (PTPParams* params, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code   = 0x9007;
	ptp.Nparam = 0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

uint16_t
ptp_ek_9009 (PTPParams* params, uint32_t *p1, uint32_t *p2)
{
	PTPContainer	ptp;
	uint16_t	ret;

	PTP_CNT_INIT(ptp);
	ptp.Code   = 0x9009;
	ptp.Nparam = 0;
	ret = ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL); 
	*p1 = ptp.Param1;
	*p2 = ptp.Param2;
	return ret;
}

uint16_t
ptp_ek_900c (PTPParams* params, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code   = 0x900c;
	ptp.Nparam = 0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
	
}

uint16_t
ptp_ek_settext (PTPParams* params, PTPEKTextParams *text)
{
	PTPContainer ptp;
	uint16_t ret;
	unsigned int size;
	unsigned char *data;

	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_EK_SetText;
	ptp.Nparam = 0;
	if (0 == (size = ptp_pack_EK_text(params, text, &data)))
		return PTP_ERROR_BADPARAM;
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL); 
	free(data);
	return ret;
}

uint16_t
ptp_ek_sendfileobject (PTPParams* params, unsigned char* object, uint32_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_EK_SendFileObject;
	ptp.Nparam=0;

	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &object, NULL);
}

uint16_t
ptp_ek_sendfileobject_from_handler (PTPParams* params, PTPDataHandler*handler, uint32_t size)
{
	PTPContainer ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_EK_SendFileObject;
	ptp.Nparam=0;
	return ptp_transaction_new(params, &ptp, PTP_DP_SENDDATA, size, handler);
}



uint16_t
ptp_canon_getpartialobjectinfo (PTPParams* params, uint32_t handle, uint32_t p2, 
			uint32_t* size, uint32_t* rp2) 
{
	uint16_t ret;
	PTPContainer ptp;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetPartialObjectInfo;
	ptp.Param1=handle;
	ptp.Param2=p2;
	ptp.Nparam=2;
	ret=ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
	*size=ptp.Param1;
	*rp2=ptp.Param2;
	return ret;
}

uint16_t
ptp_canon_get_mac_address (PTPParams* params, unsigned char **mac)
{
	PTPContainer ptp;
	unsigned int size = 0;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetMACAddress;
	ptp.Nparam=0;
	*mac = NULL;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, mac, &size);
}

uint16_t
ptp_canon_get_directory (PTPParams* params,
	PTPObjectHandles	*handles,
	PTPObjectInfo		**oinfos,	
	uint32_t		**flags		
) {
	PTPContainer	ptp;
	unsigned char	*dir = NULL;
	unsigned int	size = 0;
	uint16_t	ret;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetDirectory;
	ptp.Nparam=0;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dir, &size);
	if (ret != PTP_RC_OK)
		return ret;
	ret = ptp_unpack_canon_directory(params, dir, ptp.Param1, handles, oinfos, flags);
	free (dir);
	return ret;
}

uint16_t
ptp_canon_gettreeinfo (PTPParams* params, uint32_t *out)
{
	PTPContainer ptp;
	uint16_t ret;

	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_CANON_GetTreeInfo;
	ptp.Nparam = 1;
	ptp.Param1 = 0xf;
	ret = ptp_transaction(params, &ptp, PTP_DP_NODATA, 0, NULL, NULL);
	if ((ret == PTP_RC_OK) && (ptp.Nparam>0))
		*out = ptp.Param1;
	return ret;
}

uint16_t
ptp_canon_getpairinginfo (PTPParams* params, uint32_t nr, unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;
	uint16_t ret;
	
	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_CANON_GetPairingInfo;
	ptp.Nparam = 1;
	ptp.Param1 = nr;
	*data = NULL;
	*size = 0;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size);
	if (ret != PTP_RC_OK)
		return ret;
	return PTP_RC_OK;
}

uint16_t
ptp_canon_gettreesize (PTPParams* params,
	PTPCanon_directtransfer_entry **entries, unsigned int *cnt)
{
	PTPContainer ptp;
	uint16_t ret;
	unsigned char *out = NULL, *cur;
	int i;
	unsigned int size;
	
	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_CANON_GetTreeSize;
	ptp.Nparam = 0;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &out, &size);
	if (ret != PTP_RC_OK)
		return ret;
	*cnt = dtoh32a(out);
	*entries = malloc(sizeof(PTPCanon_directtransfer_entry)*(*cnt));
	cur = out+4;
	for (i=0;i<*cnt;i++) {
		unsigned char len;
		(*entries)[i].oid = dtoh32a(cur);
		(*entries)[i].str = ptp_unpack_string(params, cur, 4, &len);
		cur += 4+(cur[4]*2+1);
	}
	free (out);
	return PTP_RC_OK;
}

uint16_t
ptp_canon_checkevent (PTPParams* params, PTPContainer* event, int* isevent)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char *evdata = NULL;
	unsigned int len;
	
	*isevent=0;
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_CheckEvent;
	ptp.Nparam=0;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &evdata, &len);
	if (evdata!=NULL) {
		if (ret == PTP_RC_OK) {
        		ptp_unpack_EC(params, evdata, event, len);
    			*isevent=1;
        	}
		free(evdata);
	}
	return ret;
}

uint16_t
ptp_check_event (PTPParams *params) {
	PTPContainer		event;
	uint16_t		ret;

	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) &&
		ptp_operation_issupported(params, PTP_OC_NIKON_CheckEvent)
	) {
		int evtcnt;
		PTPContainer	*xevent = NULL;

		ret = ptp_nikon_check_event(params, &xevent, &evtcnt);
		if (ret != PTP_RC_OK)
			return ret;

		if (evtcnt) {
			if (params->nrofevents)
				params->events = realloc(params->events, sizeof(PTPContainer)*(evtcnt+params->nrofevents));
			else
				params->events = malloc(sizeof(PTPContainer)*evtcnt);
			memcpy (&params->events[params->nrofevents],xevent,evtcnt*sizeof(PTPContainer));
			params->nrofevents += evtcnt;
			free (xevent);
		}
		return PTP_RC_OK;
	}
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(params, PTP_OC_CANON_CheckEvent)
	) {
		int isevent;

		ret = ptp_canon_checkevent (params,&event,&isevent);
		if (ret!=PTP_RC_OK)
			return ret;
		if (isevent)
			goto store_event;
		
	}
	ret = params->event_check(params,&event);

store_event:
	if (ret == PTP_RC_OK) {
		ptp_debug (params, "event: nparams=0x%X, code=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X", event.Nparam,event.Code,event.Transaction_ID, event.Param1, event.Param2, event.Param3);
		if (params->nrofevents)
			params->events = realloc(params->events, sizeof(PTPContainer)*(params->nrofevents+1));
		else
			params->events = malloc(sizeof(PTPContainer)*1);
		memcpy (&params->events[params->nrofevents],&event,1*sizeof(PTPContainer));
		params->nrofevents += 1;
	}
	if (ret == PTP_ERROR_TIMEOUT) 
		ret = PTP_RC_OK;
	return ret;
}

int
ptp_get_one_event(PTPParams *params, PTPContainer *event) {
	if (!params->nrofevents)
		return 0;
	memcpy (event, params->events, sizeof(PTPContainer));
	memmove (params->events, params->events+1, sizeof(PTPContainer)*(params->nrofevents-1));
	
	params->nrofevents--;
	return 1;
}

uint16_t
ptp_canon_eos_getevent (PTPParams* params, PTPCanon_changes_entry **entries, int *nrofentries)
{
	PTPContainer ptp;
	uint16_t	ret;
	unsigned int 	size = 0;
	unsigned char	*data = NULL;

	*nrofentries = 0;
	*entries = NULL;
	PTP_CNT_INIT(ptp);
	ptp.Code = PTP_OC_CANON_EOS_GetEvent;
	ptp.Nparam = 0;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	if (ret != PTP_RC_OK) return ret;
        *nrofentries = ptp_unpack_CANON_changes(params,data,size,entries);
	return PTP_RC_OK;
}

uint16_t
ptp_canon_eos_getdevicepropdesc (PTPParams* params, uint16_t propcode,
	PTPDevicePropDesc *dpd)
{
	int i;

	for (i=0;i<params->nrofcanon_props;i++)
		if (params->canon_props[i].proptype == propcode)
			break;
	if (params->nrofcanon_props == i)
		return PTP_RC_Undefined;
	memcpy (dpd, &params->canon_props[i].dpd, sizeof (*dpd));
	if (dpd->FormFlag == PTP_DPFF_Enumeration) {
		
		dpd->FORM.Enum.SupportedValue = malloc (sizeof (PTPPropertyValue)*dpd->FORM.Enum.NumberOfValues);
		memcpy (dpd->FORM.Enum.SupportedValue,
			params->canon_props[i].dpd.FORM.Enum.SupportedValue,
			sizeof (PTPPropertyValue)*dpd->FORM.Enum.NumberOfValues
		);
	}
	if (dpd->DataType == PTP_DTC_STR) {
		dpd->FactoryDefaultValue.str = strdup( params->canon_props[i].dpd.FactoryDefaultValue.str );
		dpd->CurrentValue.str = strdup( params->canon_props[i].dpd.CurrentValue.str );
	}

	return PTP_RC_OK;
}


uint16_t
ptp_canon_eos_getstorageids (PTPParams* params, PTPStorageIDs* storageids)
{
	PTPContainer	ptp;
	unsigned int	len = 0;
	uint16_t	ret;
	unsigned char*	sids=NULL;
	
	PTP_CNT_INIT(ptp);
	ptp.Code 	= PTP_OC_CANON_EOS_GetStorageIDs;
	ptp.Nparam	= 0;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &sids, &len);
	if (ret == PTP_RC_OK) ptp_unpack_SIDs(params, sids, storageids, len);
	free(sids);
	return ret;
}

uint16_t
ptp_canon_eos_getstorageinfo (PTPParams* params, uint32_t p1)
{
	PTPContainer ptp;
	unsigned char	*data = NULL;
	unsigned int	size = 0;
	uint16_t	ret;
	
	PTP_CNT_INIT(ptp);
	ptp.Code 	= PTP_OC_CANON_EOS_GetStorageInfo;
	ptp.Nparam	= 1;
	ptp.Param1	= p1;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	
	return ret;
}

uint16_t
ptp_canon_eos_getpartialobject (PTPParams* params, uint32_t oid, uint32_t offset, uint32_t xsize, unsigned char**data)
{
	PTPContainer	ptp;
	unsigned int	size = 0;

	*data = NULL;
	PTP_CNT_INIT(ptp);
	ptp.Code 	= PTP_OC_CANON_EOS_GetPartialObject;
	ptp.Nparam	= 3;
	ptp.Param1	= oid;
	ptp.Param2	= offset;
	ptp.Param3	= xsize;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, &size);
}

uint16_t
ptp_canon_eos_setdevicepropvalueex (PTPParams* params, unsigned char* data, unsigned int size)
{
	PTPContainer	ptp;

	PTP_CNT_INIT(ptp);
	ptp.Code 	= PTP_OC_CANON_EOS_SetDevicePropValueEx;
	ptp.Nparam	= 0;
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
}

uint16_t
ptp_canon_eos_setdevicepropvalue (PTPParams* params,
	uint16_t propcode, PTPPropertyValue *value, uint16_t datatype
) {
	PTPContainer	ptp;
	uint16_t	ret;
	int 		i;
	unsigned char	*data;
	unsigned int	size;

	PTP_CNT_INIT(ptp);
	ptp.Code 	= PTP_OC_CANON_EOS_SetDevicePropValueEx;
	ptp.Nparam	= 0;
	for (i=0;i<params->nrofcanon_props;i++)
		if (params->canon_props[i].proptype == propcode)
			break;
	if (params->nrofcanon_props == i)
		return PTP_RC_Undefined;

	switch (propcode) {
	case PTP_DPC_CANON_EOS_ImageFormat:
	case PTP_DPC_CANON_EOS_ImageFormatCF:
	case PTP_DPC_CANON_EOS_ImageFormatSD:
	case PTP_DPC_CANON_EOS_ImageFormatExtHD:
		
		size = 8 + ptp_pack_EOS_ImageFormat( params, NULL, value->u16 );
		data = malloc( size );
		params->canon_props[i].dpd.CurrentValue.u16 = value->u16;
		ptp_pack_EOS_ImageFormat( params, data + 8, value->u16 );
		break;
	default:
		if (datatype != PTP_DTC_STR) {
			data = calloc(sizeof(uint32_t),3);
			size = sizeof(uint32_t)*3;
		} else {
			size = strlen(value->str) + 1 + 8;
			data = calloc(sizeof(char),size);
		}
		switch (datatype) {
		case PTP_DTC_UINT8:
			
			htod8a(&data[8], value->u8);
			params->canon_props[i].dpd.CurrentValue.u8 = value->u8;
			break;
		case PTP_DTC_UINT16:
			
			htod16a(&data[8], value->u16);
			params->canon_props[i].dpd.CurrentValue.u16 = value->u16;
			break;
		case PTP_DTC_UINT32:
			
			htod32a(&data[8], value->u32);
			params->canon_props[i].dpd.CurrentValue.u32 = value->u32;
			break;
		case PTP_DTC_STR:
			strcpy((char*)data + 8, value->str);
			free (params->canon_props[i].dpd.CurrentValue.str);
			params->canon_props[i].dpd.CurrentValue.str = strdup(value->str);
			break;
		}
	}

	htod32a(&data[0], size);
	htod32a(&data[4], propcode);

	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free (data);
	return ret;
}

uint16_t
ptp_canon_getpartialobject (PTPParams* params, uint32_t handle, 
				uint32_t offset, uint32_t size,
				uint32_t pos, unsigned char** block, 
				uint32_t* readnum)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char *data=NULL;
	unsigned int len;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetPartialObjectEx;
	ptp.Param1=handle;
	ptp.Param2=offset;
	ptp.Param3=size;
	ptp.Param4=pos;
	ptp.Nparam=4;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &len);
	if (ret==PTP_RC_OK) {
		*block=data;
		*readnum=ptp.Param1;
	}
	return ret;
}

uint16_t
ptp_canon_getviewfinderimage (PTPParams* params, unsigned char** image, uint32_t* size)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned int len;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetViewfinderImage;
	ptp.Nparam=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, image, &len);
	if (ret==PTP_RC_OK) *size=ptp.Param1;
	return ret;
}

uint16_t
ptp_canon_getchanges (PTPParams* params, uint16_t** props, uint32_t* propnum)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* data=NULL;
	unsigned int len;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetChanges;
	ptp.Nparam=0;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &len);
	if (ret == PTP_RC_OK)
        	*propnum=ptp_unpack_uint16_t_array(params,data,0,props);
	free(data);
	return ret;
}

uint16_t
ptp_canon_getobjectinfo (PTPParams* params, uint32_t store, uint32_t p2, 
			    uint32_t parent, uint32_t handle, 
			    PTPCANONFolderEntry** entries, uint32_t* entnum)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char *data = NULL;
	unsigned int len;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_CANON_GetObjectInfoEx;
	ptp.Param1=store;
	ptp.Param2=p2;
	ptp.Param3=parent;
	ptp.Param4=handle;
	ptp.Nparam=4;
	len=0;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &len);
	if (ret == PTP_RC_OK) {
		int i;
		*entnum=ptp.Param1;
		*entries=calloc(*entnum, sizeof(PTPCANONFolderEntry));
		if (*entries!=NULL) {
			for(i=0; i<(*entnum); i++)
				ptp_unpack_Canon_FE(params,
					data+i*PTP_CANON_FolderEntryLen,
					&((*entries)[i]) );
		} else {
			ret=PTP_ERROR_IO; 
		}
	}
	free(data);
	return ret;
}

uint16_t
ptp_canon_get_objecthandle_by_name (PTPParams* params, char* name, uint32_t* objectid)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char *data = NULL;
	uint8_t len;

	PTP_CNT_INIT (ptp);
	ptp.Code=PTP_OC_CANON_GetObjectHandleByName;
	ptp.Nparam=0;
	len=0;
	data = malloc (2*(strlen(name)+1)+2);
	memset (data, 0, 2*(strlen(name)+1)+2);
	ptp_pack_string (params, name, data, 0, &len);
	ret=ptp_transaction (params, &ptp, PTP_DP_SENDDATA, (len+1)*2+1, &data, NULL);
	free (data);
	*objectid = ptp.Param1;
	return ret;
}

uint16_t
ptp_canon_get_customize_data (PTPParams* params, uint32_t themenr,
		unsigned char **data, unsigned int *size)
{
	PTPContainer ptp;

	*data = NULL;
	*size = 0;
	PTP_CNT_INIT(ptp);
	ptp.Code	= PTP_OC_CANON_GetCustomizeData;
	ptp.Param1	= themenr;
	ptp.Nparam	= 1;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}


uint16_t
ptp_nikon_curve_download (PTPParams* params, unsigned char **data, unsigned int *size) {
	PTPContainer ptp;
	*data = NULL;
	*size = 0;
	PTP_CNT_INIT(ptp);
	ptp.Code	= PTP_OC_NIKON_CurveDownload;
	ptp.Nparam	= 0;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

uint16_t
ptp_nikon_get_vendorpropcodes (PTPParams* params, uint16_t **props, unsigned int *size) {
	PTPContainer	ptp;
	uint16_t	ret;
	unsigned char	*xdata;
	unsigned int 	xsize;

	*props = NULL;
	*size = 0;
	PTP_CNT_INIT(ptp);
	ptp.Code	= PTP_OC_NIKON_GetVendorPropCodes;
	ptp.Nparam	= 0;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &xdata, &xsize); 
	if (ret == PTP_RC_OK)
        	*size = ptp_unpack_uint16_t_array(params,xdata,0,props);
	return ret;
}

uint16_t
ptp_nikon_getfileinfoinblock ( PTPParams* params,
	uint32_t p1, uint32_t p2, uint32_t p3,
	unsigned char **data, unsigned int *size
) {
	PTPContainer ptp;
	*data = NULL;
	*size = 0;
	PTP_CNT_INIT(ptp);
	ptp.Code	= PTP_OC_NIKON_GetFileInfoInBlock;
	ptp.Nparam	= 3;
	ptp.Param1	= p1;
	ptp.Param2	= p2;
	ptp.Param3	= p3;
	return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size); 
}

uint16_t
ptp_nikon_get_liveview_image (PTPParams* params, unsigned char **data, unsigned int *size)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_NIKON_GetLiveViewImg;
        ptp.Nparam=0;
        return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size);
}

uint16_t
ptp_nikon_get_preview_image (PTPParams* params, unsigned char **xdata, unsigned int *xsize,
	uint32_t *handle)
{
        PTPContainer	ptp;
	uint16_t	ret;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_NIKON_GetPreviewImg;
        ptp.Nparam=0;
        ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, xdata, xsize);
	if (ret == PTP_RC_OK) {
		if (ptp.Nparam > 0)
			*handle = ptp.Param1;
	}
	return ret;
}

uint16_t
ptp_canon_eos_get_viewfinder_image (PTPParams* params, unsigned char **data, unsigned int *size)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_CANON_EOS_GetViewFinderData;
        ptp.Nparam=1;
        ptp.Param1=0x00100000; 
        return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size);
}

uint16_t
ptp_nikon_check_event (PTPParams* params, PTPContainer** event, int* evtcnt)
{
        PTPContainer ptp;
	uint16_t ret;
	unsigned char *data = NULL;
	unsigned int size = 0;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_NIKON_CheckEvent;
	ptp.Nparam=0;
	*evtcnt = 0;
	ret = ptp_transaction (params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	if (ret == PTP_RC_OK) {
		ptp_unpack_Nikon_EC (params, data, size, event, evtcnt);
		free (data);
	}
	return ret;
}

uint16_t
ptp_nikon_getptpipinfo (PTPParams* params, unsigned char **data, unsigned int *size)
{
        PTPContainer ptp;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_NIKON_GetDevicePTPIPInfo;
        ptp.Nparam=0;
        return ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, data, size);
}

uint16_t
ptp_nikon_getwifiprofilelist (PTPParams* params)
{
        PTPContainer ptp;
	unsigned char* data;
	unsigned int size;
	unsigned int pos;
	unsigned int profn;
	unsigned int n;
	char* buffer;
	uint8_t len;
	
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_NIKON_GetProfileAllData;
        ptp.Nparam=0;
	size = 0;
	data = NULL;
	CHECK_PTP_RC(ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size));

	if (size < 2) return PTP_RC_Undefined; 

	params->wifi_profiles_version = data[0];
	params->wifi_profiles_number = data[1];
	if (params->wifi_profiles)
		free(params->wifi_profiles);
	
	params->wifi_profiles = malloc(params->wifi_profiles_number*sizeof(PTPNIKONWifiProfile));
	memset(params->wifi_profiles, 0, params->wifi_profiles_number*sizeof(PTPNIKONWifiProfile));

	pos = 2;
	profn = 0;
	while (profn < params->wifi_profiles_number && pos < size) {
		if (pos+6 >= size) return PTP_RC_Undefined;
		params->wifi_profiles[profn].id = data[pos++];
		params->wifi_profiles[profn].valid = data[pos++];

		n = dtoh32a(&data[pos]);
		pos += 4;
		if (pos+n+4 >= size) return PTP_RC_Undefined;
		strncpy(params->wifi_profiles[profn].profile_name, (char*)&data[pos], n);
		params->wifi_profiles[profn].profile_name[16] = '\0';
		pos += n;

		params->wifi_profiles[profn].display_order = data[pos++];
		params->wifi_profiles[profn].device_type = data[pos++];
		params->wifi_profiles[profn].icon_type = data[pos++];

		buffer = ptp_unpack_string(params, data, pos, &len);
		strncpy(params->wifi_profiles[profn].creation_date, buffer, sizeof(params->wifi_profiles[profn].creation_date));
		free (buffer);
		pos += (len*2+1);
		if (pos+1 >= size) return PTP_RC_Undefined;
		
		buffer = ptp_unpack_string(params, data, pos, &len);
		strncpy(params->wifi_profiles[profn].lastusage_date, buffer, sizeof(params->wifi_profiles[profn].lastusage_date));
		free (buffer);
		pos += (len*2+1);
		if (pos+5 >= size) return PTP_RC_Undefined;
		
		n = dtoh32a(&data[pos]);
		pos += 4;
		if (pos+n >= size) return PTP_RC_Undefined;
		strncpy(params->wifi_profiles[profn].essid, (char*)&data[pos], n);
		params->wifi_profiles[profn].essid[32] = '\0';
		pos += n;
		pos += 1;
		profn++;
	}

#if 0
	PTPNIKONWifiProfile test;
	memset(&test, 0, sizeof(PTPNIKONWifiProfile));
	strcpy(test.profile_name, "MyTest");
	test.icon_type = 1;
	strcpy(test.essid, "nikon");
	test.ip_address = 10 + 11 << 16 + 11 << 24;
	test.subnet_mask = 24;
	test.access_mode = 1;
	test.wifi_channel = 1;
	test.key_nr = 1;

	ptp_nikon_writewifiprofile(params, &test);
#endif

	return PTP_RC_OK;
}

uint16_t
ptp_nikon_writewifiprofile (PTPParams* params, PTPNIKONWifiProfile* profile)
{
	unsigned char guid[16];
	
	PTPContainer ptp;
	unsigned char buffer[1024];
	unsigned char* data = buffer;
	int size = 0;
	int i;
	uint8_t len;
	int profilenr = -1;
	
	ptp_nikon_getptpipguid(guid);

	if (!params->wifi_profiles)
		CHECK_PTP_RC(ptp_nikon_getwifiprofilelist(params));
	
	for (i = 0; i < params->wifi_profiles_number; i++) {
		if (!params->wifi_profiles[i].valid) {
			profilenr = params->wifi_profiles[i].id;
			break;
		}
	}
	
	if (profilenr == -1) {
		
		return PTP_RC_StoreFull;
	}
	
	memset(buffer, 0, 1024);
	
	buffer[0x00] = 0x64; 
	
	
	htod32a(&buffer[0x01], 17);
	
	strncpy((char*)&buffer[0x05], profile->profile_name, 16);
	
	buffer[0x16] = 0x00; 
	buffer[0x17] = profile->device_type;
	buffer[0x18] = profile->icon_type;
	
	
	ptp_pack_string(params, "19990909T090909", data, 0x19, &len);
	
	
	*((unsigned int*)&buffer[0x3A]) = profile->ip_address; 
	buffer[0x3E] = profile->subnet_mask;
	*((unsigned int*)&buffer[0x3F]) = profile->gateway_address; 
	buffer[0x43] = profile->address_mode;
	
	
	buffer[0x44] = profile->access_mode;
	buffer[0x45] = profile->wifi_channel;
	
	htod32a(&buffer[0x46], 33); 
	 
	strncpy((char*)&buffer[0x4A], profile->essid, 32);
	
	buffer[0x6B] = profile->authentification;
	buffer[0x6C] = profile->encryption;
	htod32a(&buffer[0x6D], 64);
	for (i = 0; i < 64; i++) {
		buffer[0x71+i] = profile->key[i];
	}
	buffer[0xB1] = profile->key_nr;
	memcpy(&buffer[0xB2], guid, 16);
	
	switch(profile->encryption) {
	case 1: 
		htod16a(&buffer[0xC2], 5); 
		break;
	case 2: 
		htod16a(&buffer[0xC2], 13); 
		break;
	default:
		htod16a(&buffer[0xC2], 0);
	}
	size = 0xC4;
	       
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_NIKON_SendProfileData;
	ptp.Nparam=1;
	ptp.Param1=profilenr;
	return ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
}

uint16_t
ptp_mtp_getobjectpropssupported (PTPParams* params, uint16_t ofc,
		 uint32_t *propnum, uint16_t **props
) {
        PTPContainer ptp;
	uint16_t ret;
	unsigned char *data = NULL;
	unsigned int size = 0;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_MTP_GetObjectPropsSupported;
        ptp.Nparam = 1;
        ptp.Param1 = ofc;
        ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	if (ret == PTP_RC_OK)
        	*propnum=ptp_unpack_uint16_t_array(params,data,0,props);
	free(data);
	return ret;
}

uint16_t
ptp_mtp_getobjectpropdesc (
	PTPParams* params, uint16_t opc, uint16_t ofc, PTPObjectPropDesc *opd
) {
        PTPContainer ptp;
	uint16_t ret;
	unsigned char *data = NULL;
	unsigned int size = 0;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_MTP_GetObjectPropDesc;
        ptp.Nparam = 2;
        ptp.Param1 = opc;
        ptp.Param2 = ofc;
        ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	if (ret == PTP_RC_OK)
		ptp_unpack_OPD (params, data, opd, size);
	free(data);
	return ret;
}

uint16_t
ptp_mtp_getobjectpropvalue (
	PTPParams* params, uint32_t oid, uint16_t opc,
	PTPPropertyValue *value, uint16_t datatype
) {
        PTPContainer ptp;
	uint16_t ret;
	unsigned char *data = NULL;
	unsigned int size = 0;
	int offset = 0;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_MTP_GetObjectPropValue;
        ptp.Nparam = 2;
        ptp.Param1 = oid;
        ptp.Param2 = opc;
        ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &data, &size);
	if (ret == PTP_RC_OK)
		ptp_unpack_DPV(params, data, &offset, size, value, datatype);
	free(data);
	return ret;
}

uint16_t
ptp_mtp_setobjectpropvalue (
	PTPParams* params, uint32_t oid, uint16_t opc,
	PTPPropertyValue *value, uint16_t datatype
) {
        PTPContainer ptp;
	uint16_t ret;
	unsigned char *data = NULL;
	unsigned int size ;
        
        PTP_CNT_INIT(ptp);
        ptp.Code=PTP_OC_MTP_SetObjectPropValue;
        ptp.Nparam = 2;
        ptp.Param1 = oid;
        ptp.Param2 = opc;
	size = ptp_pack_DPV(params, value, &data, datatype);
        ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &data, NULL);
	free(data);
	return ret;
}

uint16_t
ptp_mtp_getobjectreferences (PTPParams* params, uint32_t handle, uint32_t** ohArray, uint32_t* arraylen)
{
	PTPContainer ptp;
	uint16_t ret;
	unsigned char* dpv=NULL;
	unsigned int dpvlen = 0;

	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_MTP_GetObjectReferences;
	ptp.Param1=handle;
	ptp.Nparam=1;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dpv, &dpvlen);
	if (ret == PTP_RC_OK) {
		if ((dpv == NULL) || (dpvlen == 0)) {
			*arraylen = 0;
			*ohArray = NULL;
		} else {
			*arraylen = ptp_unpack_uint32_t_array(params, dpv, 0, ohArray);
		}
	}
	free(dpv);
	return ret;
}

uint16_t
ptp_mtp_setobjectreferences (PTPParams* params, uint32_t handle, uint32_t* ohArray, uint32_t arraylen)
{
	PTPContainer ptp;
	uint16_t ret;
	uint32_t size;
	unsigned char* dpv=NULL;

	PTP_CNT_INIT(ptp);
	ptp.Code   = PTP_OC_MTP_SetObjectReferences;
	ptp.Param1 = handle;
	ptp.Nparam = 1;
	size = ptp_pack_uint32_t_array(params, ohArray, arraylen, &dpv);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, (unsigned char **)&dpv, NULL);
	free(dpv);
	return ret;
}

uint16_t
ptp_mtp_getobjectproplist (PTPParams* params, uint32_t handle, MTPProperties **props, int *nrofprops)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* opldata = NULL;
	unsigned int oplsize;

	PTP_CNT_INIT(ptp);
	ptp.Code = PTP_OC_MTP_GetObjPropList;
	ptp.Param1 = handle;
	ptp.Param2 = 0x00000000U;  
	ptp.Param3 = 0xFFFFFFFFU;  
	ptp.Param4 = 0x00000000U;
	ptp.Param5 = 0x00000000U;
	ptp.Nparam = 5;
	ret = ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &opldata, &oplsize);  
	if (ret == PTP_RC_OK) *nrofprops = ptp_unpack_OPL(params, opldata, props, oplsize);
	if (opldata != NULL)
		free(opldata);
	return ret;
}

uint16_t
ptp_mtp_sendobjectproplist (PTPParams* params, uint32_t* store, uint32_t* parenthandle, uint32_t* handle,
			    uint16_t objecttype, uint64_t objectsize, MTPProperties *props, int nrofprops)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* opldata=NULL;
	uint32_t oplsize;

	PTP_CNT_INIT(ptp);
	ptp.Code = PTP_OC_MTP_SendObjectPropList;
	ptp.Param1 = *store;
	ptp.Param2 = *parenthandle;
	ptp.Param3 = (uint32_t) objecttype;
	ptp.Param4 = (uint32_t) (objectsize >> 32);
	ptp.Param5 = (uint32_t) (objectsize & 0xffffffffU);
	ptp.Nparam = 5;

	
	oplsize = ptp_pack_OPL(params,props,nrofprops,&opldata);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, oplsize, &opldata, NULL); 
	free(opldata);
	*store = ptp.Param1;
	*parenthandle = ptp.Param2;
	*handle = ptp.Param3; 

	return ret;
}

uint16_t
ptp_mtp_setobjectproplist (PTPParams* params, MTPProperties *props, int nrofprops)
{
	uint16_t ret;
	PTPContainer ptp;
	unsigned char* opldata=NULL;
	uint32_t oplsize;
  
	PTP_CNT_INIT(ptp);
	ptp.Code = PTP_OC_MTP_SetObjPropList;
	ptp.Nparam = 0;
  
	oplsize = ptp_pack_OPL(params,props,nrofprops,&opldata);
	ret = ptp_transaction(params, &ptp, PTP_DP_SENDDATA, oplsize, &opldata, NULL); 
	free(opldata);

	return ret;
}


int
ptp_operation_issupported(PTPParams* params, uint16_t operation)
{
	int i=0;

	for (;i<params->deviceinfo.OperationsSupported_len;i++) {
		if (params->deviceinfo.OperationsSupported[i]==operation)
			return 1;
	}
	return 0;
}


int
ptp_event_issupported(PTPParams* params, uint16_t event)
{
	int i=0;

	for (;i<params->deviceinfo.EventsSupported_len;i++) {
		if (params->deviceinfo.EventsSupported[i]==event)
			return 1;
	}
	return 0;
}


int
ptp_property_issupported(PTPParams* params, uint16_t property)
{
	int i;

	for (i=0;i<params->deviceinfo.DevicePropertiesSupported_len;i++)
		if (params->deviceinfo.DevicePropertiesSupported[i]==property)
			return 1;
	return 0;
}

void
ptp_free_devicepropvalue(uint16_t dt, PTPPropertyValue* dpd) {
	switch (dt) {
	case PTP_DTC_INT8:	case PTP_DTC_UINT8:
	case PTP_DTC_UINT16:	case PTP_DTC_INT16:
	case PTP_DTC_UINT32:	case PTP_DTC_INT32:
	case PTP_DTC_UINT64:	case PTP_DTC_INT64:
	case PTP_DTC_UINT128:	case PTP_DTC_INT128:
		
		break;
	case PTP_DTC_AINT8:	case PTP_DTC_AUINT8:
	case PTP_DTC_AUINT16:	case PTP_DTC_AINT16:
	case PTP_DTC_AUINT32:	case PTP_DTC_AINT32:
	case PTP_DTC_AUINT64:	case PTP_DTC_AINT64:
	case PTP_DTC_AUINT128:	case PTP_DTC_AINT128:
		if (dpd->a.v)
			free(dpd->a.v);
		break;
	case PTP_DTC_STR:
		if (dpd->str)
			free(dpd->str);
		break;
	}
}

void
ptp_free_devicepropdesc(PTPDevicePropDesc* dpd)
{
	uint16_t i;

	ptp_free_devicepropvalue (dpd->DataType, &dpd->FactoryDefaultValue);
	ptp_free_devicepropvalue (dpd->DataType, &dpd->CurrentValue);
	switch (dpd->FormFlag) {
	case PTP_DPFF_Range:
		ptp_free_devicepropvalue (dpd->DataType, &dpd->FORM.Range.MinimumValue);
		ptp_free_devicepropvalue (dpd->DataType, &dpd->FORM.Range.MaximumValue);
		ptp_free_devicepropvalue (dpd->DataType, &dpd->FORM.Range.StepSize);
		break;
	case PTP_DPFF_Enumeration:
		if (dpd->FORM.Enum.SupportedValue) {
			for (i=0;i<dpd->FORM.Enum.NumberOfValues;i++)
				ptp_free_devicepropvalue (dpd->DataType, dpd->FORM.Enum.SupportedValue+i);
			free (dpd->FORM.Enum.SupportedValue);
		}
	}
}

void
ptp_free_objectpropdesc(PTPObjectPropDesc* opd)
{
	uint16_t i;

	ptp_free_devicepropvalue (opd->DataType, &opd->FactoryDefaultValue);
	switch (opd->FormFlag) {
	case PTP_OPFF_None:
		break;
	case PTP_OPFF_Range:
		ptp_free_devicepropvalue (opd->DataType, &opd->FORM.Range.MinimumValue);
		ptp_free_devicepropvalue (opd->DataType, &opd->FORM.Range.MaximumValue);
		ptp_free_devicepropvalue (opd->DataType, &opd->FORM.Range.StepSize);
		break;
	case PTP_OPFF_Enumeration:
		if (opd->FORM.Enum.SupportedValue) {
			for (i=0;i<opd->FORM.Enum.NumberOfValues;i++)
				ptp_free_devicepropvalue (opd->DataType, opd->FORM.Enum.SupportedValue+i);
			free (opd->FORM.Enum.SupportedValue);
		}
		break;
	case PTP_OPFF_DateTime:
	case PTP_OPFF_FixedLengthArray:
	case PTP_OPFF_RegularExpression:
	case PTP_OPFF_ByteArray:
	case PTP_OPFF_LongString:
		
		break;
	default:
		fprintf (stderr, "Unknown OPFF type %d\n", opd->FormFlag);
		break;
	}
}

void
ptp_free_objectinfo (PTPObjectInfo *oi)
{
	if (!oi) return;
        free (oi->Filename); oi->Filename = NULL;
        free (oi->Keywords); oi->Keywords = NULL;
}

void
ptp_free_object (PTPObject *ob)
{
	int i;
	if (!ob) return;

	ptp_free_objectinfo (&ob->oi);
	for (i=0;i<ob->nrofmtpprops;i++)
		ptp_destroy_object_prop(&ob->mtpprops[i]);
	ob->flags = 0;
}

void 
ptp_perror(PTPParams* params, uint16_t error) {

	int i;
	
	static struct {
		short n;
		const char *txt;
	} ptp_errors[] = {
	{PTP_RC_Undefined, 		N_("PTP: Undefined Error")},
	{PTP_RC_OK, 			N_("PTP: OK!")},
	{PTP_RC_GeneralError, 		N_("PTP: General Error")},
	{PTP_RC_SessionNotOpen, 	N_("PTP: Session Not Open")},
	{PTP_RC_InvalidTransactionID, 	N_("PTP: Invalid Transaction ID")},
	{PTP_RC_OperationNotSupported, 	N_("PTP: Operation Not Supported")},
	{PTP_RC_ParameterNotSupported, 	N_("PTP: Parameter Not Supported")},
	{PTP_RC_IncompleteTransfer, 	N_("PTP: Incomplete Transfer")},
	{PTP_RC_InvalidStorageId, 	N_("PTP: Invalid Storage ID")},
	{PTP_RC_InvalidObjectHandle, 	N_("PTP: Invalid Object Handle")},
	{PTP_RC_DevicePropNotSupported, N_("PTP: Device Prop Not Supported")},
	{PTP_RC_InvalidObjectFormatCode, N_("PTP: Invalid Object Format Code")},
	{PTP_RC_StoreFull, 		N_("PTP: Store Full")},
	{PTP_RC_ObjectWriteProtected, 	N_("PTP: Object Write Protected")},
	{PTP_RC_StoreReadOnly, 		N_("PTP: Store Read Only")},
	{PTP_RC_AccessDenied,		N_("PTP: Access Denied")},
	{PTP_RC_NoThumbnailPresent, 	N_("PTP: No Thumbnail Present")},
	{PTP_RC_SelfTestFailed, 	N_("PTP: Self Test Failed")},
	{PTP_RC_PartialDeletion, 	N_("PTP: Partial Deletion")},
	{PTP_RC_StoreNotAvailable, 	N_("PTP: Store Not Available")},
	{PTP_RC_SpecificationByFormatUnsupported,
				N_("PTP: Specification By Format Unsupported")},
	{PTP_RC_NoValidObjectInfo, 	N_("PTP: No Valid Object Info")},
	{PTP_RC_InvalidCodeFormat, 	N_("PTP: Invalid Code Format")},
	{PTP_RC_UnknownVendorCode, 	N_("PTP: Unknown Vendor Code")},
	{PTP_RC_CaptureAlreadyTerminated,
					N_("PTP: Capture Already Terminated")},
	{PTP_RC_DeviceBusy, 		N_("PTP: Device Busy")},
	{PTP_RC_InvalidParentObject, 	N_("PTP: Invalid Parent Object")},
	{PTP_RC_InvalidDevicePropFormat, N_("PTP: Invalid Device Prop Format")},
	{PTP_RC_InvalidDevicePropValue, N_("PTP: Invalid Device Prop Value")},
	{PTP_RC_InvalidParameter, 	N_("PTP: Invalid Parameter")},
	{PTP_RC_SessionAlreadyOpened, 	N_("PTP: Session Already Opened")},
	{PTP_RC_TransactionCanceled, 	N_("PTP: Transaction Canceled")},
	{PTP_RC_SpecificationOfDestinationUnsupported,
			N_("PTP: Specification Of Destination Unsupported")},
	{PTP_RC_EK_FilenameRequired,	N_("PTP: EK Filename Required")},
	{PTP_RC_EK_FilenameConflicts,	N_("PTP: EK Filename Conflicts")},
	{PTP_RC_EK_FilenameInvalid,	N_("PTP: EK Filename Invalid")},

	{PTP_ERROR_IO,		  N_("PTP: I/O error")},
	{PTP_ERROR_BADPARAM,	  N_("PTP: Error: bad parameter")},
	{PTP_ERROR_DATA_EXPECTED, N_("PTP: Protocol error, data expected")},
	{PTP_ERROR_RESP_EXPECTED, N_("PTP: Protocol error, response expected")},
	{0, NULL}
};

	for (i=0; ptp_errors[i].txt!=NULL; i++)
		if (ptp_errors[i].n == error)
			ptp_error(params, ptp_errors[i].txt);
}

const char*
ptp_get_property_description(PTPParams* params, uint16_t dpc)
{
	int i;
	
	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties[] = {
		{PTP_DPC_Undefined,		N_("Undefined PTP Property")},
		{PTP_DPC_BatteryLevel,		N_("Battery Level")},
		{PTP_DPC_FunctionalMode,	N_("Functional Mode")},
		{PTP_DPC_ImageSize,		N_("Image Size")},
		{PTP_DPC_CompressionSetting,	N_("Compression Setting")},
		{PTP_DPC_WhiteBalance,		N_("White Balance")},
		{PTP_DPC_RGBGain,		N_("RGB Gain")},
		{PTP_DPC_FNumber,		N_("F-Number")},
		{PTP_DPC_FocalLength,		N_("Focal Length")},
		{PTP_DPC_FocusDistance,		N_("Focus Distance")},
		{PTP_DPC_FocusMode,		N_("Focus Mode")},
		{PTP_DPC_ExposureMeteringMode,	N_("Exposure Metering Mode")},
		{PTP_DPC_FlashMode,		N_("Flash Mode")},
		{PTP_DPC_ExposureTime,		N_("Exposure Time")},
		{PTP_DPC_ExposureProgramMode,	N_("Exposure Program Mode")},
		{PTP_DPC_ExposureIndex,
					N_("Exposure Index (film speed ISO)")},
		{PTP_DPC_ExposureBiasCompensation,
					N_("Exposure Bias Compensation")},
		{PTP_DPC_DateTime,		N_("Date & Time")},
		{PTP_DPC_CaptureDelay,		N_("Pre-Capture Delay")},
		{PTP_DPC_StillCaptureMode,	N_("Still Capture Mode")},
		{PTP_DPC_Contrast,		N_("Contrast")},
		{PTP_DPC_Sharpness,		N_("Sharpness")},
		{PTP_DPC_DigitalZoom,		N_("Digital Zoom")},
		{PTP_DPC_EffectMode,		N_("Effect Mode")},
		{PTP_DPC_BurstNumber,		N_("Burst Number")},
		{PTP_DPC_BurstInterval,		N_("Burst Interval")},
		{PTP_DPC_TimelapseNumber,	N_("Timelapse Number")},
		{PTP_DPC_TimelapseInterval,	N_("Timelapse Interval")},
		{PTP_DPC_FocusMeteringMode,	N_("Focus Metering Mode")},
		{PTP_DPC_UploadURL,		N_("Upload URL")},
		{PTP_DPC_Artist,		N_("Artist")},
		{PTP_DPC_CopyrightInfo,		N_("Copyright Info")},
		{0,NULL}
	};
	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties_EK[] = {
		{PTP_DPC_EK_ColorTemperature,	N_("Color Temperature")},
		{PTP_DPC_EK_DateTimeStampFormat,
					N_("Date Time Stamp Format")},
		{PTP_DPC_EK_BeepMode,		N_("Beep Mode")},
		{PTP_DPC_EK_VideoOut,		N_("Video Out")},
		{PTP_DPC_EK_PowerSaving,	N_("Power Saving")},
		{PTP_DPC_EK_UI_Language,	N_("UI Language")},
		{0,NULL}
	};

	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties_Canon[] = {
		{PTP_DPC_CANON_BeepMode,	N_("Beep Mode")},
		{PTP_DPC_CANON_BatteryKind,	N_("Battery Type")},
		{PTP_DPC_CANON_BatteryStatus,	N_("Battery Mode")},
		{PTP_DPC_CANON_UILockType,	N_("UILockType")},
		{PTP_DPC_CANON_CameraMode,	N_("Camera Mode")},
		{PTP_DPC_CANON_ImageQuality,	N_("Image Quality")},
		{PTP_DPC_CANON_FullViewFileFormat,	N_("Full View File Format")},
		{PTP_DPC_CANON_ImageSize,	N_("Image Size")},
		{PTP_DPC_CANON_SelfTime,	N_("Self Time")},
		{PTP_DPC_CANON_FlashMode,	N_("Flash Mode")},
		{PTP_DPC_CANON_Beep,		N_("Beep")},
		{PTP_DPC_CANON_ShootingMode,	N_("Shooting Mode")},
		{PTP_DPC_CANON_ImageMode,	N_("Image Mode")},
		{PTP_DPC_CANON_DriveMode,	N_("Drive Mode")},
		{PTP_DPC_CANON_EZoom,		N_("Zoom")},
		{PTP_DPC_CANON_MeteringMode,	N_("Metering Mode")},
		{PTP_DPC_CANON_AFDistance,	N_("AF Distance")},
		{PTP_DPC_CANON_FocusingPoint,	N_("Focusing Point")},
		{PTP_DPC_CANON_WhiteBalance,	N_("White Balance")},
		{PTP_DPC_CANON_SlowShutterSetting,	N_("Slow Shutter Setting")},
		{PTP_DPC_CANON_AFMode,		N_("AF Mode")},
		{PTP_DPC_CANON_ImageStabilization,		N_("Image Stabilization")},
		{PTP_DPC_CANON_Contrast,	N_("Contrast")},
		{PTP_DPC_CANON_ColorGain,	N_("Color Gain")},
		{PTP_DPC_CANON_Sharpness,	N_("Sharpness")},
		{PTP_DPC_CANON_Sensitivity,	N_("Sensitivity")},
		{PTP_DPC_CANON_ParameterSet,	N_("Parameter Set")},
		{PTP_DPC_CANON_ISOSpeed,	N_("ISO Speed")},
		{PTP_DPC_CANON_Aperture,	N_("Aperture")},
		{PTP_DPC_CANON_ShutterSpeed,	N_("Shutter Speed")},
		{PTP_DPC_CANON_ExpCompensation,	N_("Exposure Compensation")},
		{PTP_DPC_CANON_FlashCompensation,	N_("Flash Compensation")},
		{PTP_DPC_CANON_AEBExposureCompensation,	N_("AEB Exposure Compensation")},
		{PTP_DPC_CANON_AvOpen,		N_("Av Open")},
		{PTP_DPC_CANON_AvMax,		N_("Av Max")},
		{PTP_DPC_CANON_FocalLength,	N_("Focal Length")},
		{PTP_DPC_CANON_FocalLengthTele,	N_("Focal Length Tele")},
		{PTP_DPC_CANON_FocalLengthWide,	N_("Focal Length Wide")},
		{PTP_DPC_CANON_FocalLengthDenominator,	N_("Focal Length Denominator")},
		{PTP_DPC_CANON_CaptureTransferMode,	N_("Capture Transfer Mode")},
		{PTP_DPC_CANON_Zoom,		N_("Zoom")},
		{PTP_DPC_CANON_NamePrefix,	N_("Name Prefix")},
		{PTP_DPC_CANON_SizeQualityMode,	N_("Size Quality Mode")},
		{PTP_DPC_CANON_SupportedThumbSize,	N_("Supported Thumb Size")},
		{PTP_DPC_CANON_SizeOfOutputDataFromCamera,	N_("Size of Output Data from Camera")},
		{PTP_DPC_CANON_SizeOfInputDataToCamera,		N_("Size of Input Data to Camera")},
		{PTP_DPC_CANON_RemoteAPIVersion,N_("Remote API Version")},
		{PTP_DPC_CANON_FirmwareVersion,	N_("Firmware Version")},
		{PTP_DPC_CANON_CameraModel,	N_("Camera Model")},
		{PTP_DPC_CANON_CameraOwner,	N_("Camera Owner")},
		{PTP_DPC_CANON_UnixTime,	N_("UNIX Time")},
		{PTP_DPC_CANON_CameraBodyID,	N_("Camera Body ID")},
		{PTP_DPC_CANON_CameraOutput,	N_("Camera Output")},
		{PTP_DPC_CANON_DispAv,		N_("Disp Av")},
		{PTP_DPC_CANON_AvOpenApex,	N_("Av Open Apex")},
		{PTP_DPC_CANON_DZoomMagnification,	N_("Digital Zoom Magnification")},
		{PTP_DPC_CANON_MlSpotPos,	N_("Ml Spot Position")},
		{PTP_DPC_CANON_DispAvMax,	N_("Disp Av Max")},
		{PTP_DPC_CANON_AvMaxApex,	N_("Av Max Apex")},
		{PTP_DPC_CANON_EZoomStartPosition,	N_("EZoom Start Position")},
		{PTP_DPC_CANON_FocalLengthOfTele,	N_("Focal Length Tele")},
		{PTP_DPC_CANON_EZoomSizeOfTele,	N_("EZoom Size of Tele")},
		{PTP_DPC_CANON_PhotoEffect,	N_("Photo Effect")},
		{PTP_DPC_CANON_AssistLight,	N_("Assist Light")},
		{PTP_DPC_CANON_FlashQuantityCount,	N_("Flash Quantity Count")},
		{PTP_DPC_CANON_RotationAngle,	N_("Rotation Angle")},
		{PTP_DPC_CANON_RotationScene,	N_("Rotation Scene")},
		{PTP_DPC_CANON_EventEmulateMode,N_("Event Emulate Mode")},
		{PTP_DPC_CANON_DPOFVersion,	N_("DPOF Version")},
		{PTP_DPC_CANON_TypeOfSupportedSlideShow,	N_("Type of Slideshow")},
		{PTP_DPC_CANON_AverageFilesizes,N_("Average Filesizes")},
		{PTP_DPC_CANON_ModelID,		N_("Model ID")},
		{0,NULL}
	};

	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties_Nikon[] = {
		{PTP_DPC_NIKON_ShootingBank, 			
		 N_("Shooting Bank")},
		{PTP_DPC_NIKON_ShootingBankNameA,		
		 N_("Shooting Bank Name A")},
		{PTP_DPC_NIKON_ShootingBankNameB,		
		 N_("Shooting Bank Name B")},
		{PTP_DPC_NIKON_ShootingBankNameC,		
		 N_("Shooting Bank Name C")},
		{PTP_DPC_NIKON_ShootingBankNameD,		
		 N_("Shooting Bank Name D")},
		{PTP_DPC_NIKON_ResetBank0,			
		 N_("Reset Bank 0")},
		{PTP_DPC_NIKON_RawCompression,			
		 N_("Raw Compression")},
		{PTP_DPC_NIKON_WhiteBalanceAutoBias,		
		 N_("Auto White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceTungstenBias,	
		 N_("Tungsten White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceFluorescentBias,	
		 N_("Fluorescent White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceDaylightBias,	
		 N_("Daylight White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceFlashBias,		
		 N_("Flash White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceCloudyBias,		
		 N_("Cloudy White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceShadeBias,		
		 N_("Shady White Balance Bias")},
		{PTP_DPC_NIKON_WhiteBalanceColorTemperature,	
		 N_("White Balance Colour Temperature")},
		{PTP_DPC_NIKON_WhiteBalancePresetNo,		
		 N_("White Balance Preset Number")},
		{PTP_DPC_NIKON_WhiteBalancePresetName0,		
		 N_("White Balance Preset Name 0")},
		{PTP_DPC_NIKON_WhiteBalancePresetName1,		
		 N_("White Balance Preset Name 1")},
		{PTP_DPC_NIKON_WhiteBalancePresetName2,		
		 N_("White Balance Preset Name 2")},
		{PTP_DPC_NIKON_WhiteBalancePresetName3,		
		 N_("White Balance Preset Name 3")},
		{PTP_DPC_NIKON_WhiteBalancePresetName4,		
		 N_("White Balance Preset Name 4")},
		{PTP_DPC_NIKON_WhiteBalancePresetVal0,		
		 N_("White Balance Preset Value 0")},
		{PTP_DPC_NIKON_WhiteBalancePresetVal1,		
		 N_("White Balance Preset Value 1")},
		{PTP_DPC_NIKON_WhiteBalancePresetVal2,		
		 N_("White Balance Preset Value 2")},
		{PTP_DPC_NIKON_WhiteBalancePresetVal3,		
		 N_("White Balance Preset Value 3")},
		{PTP_DPC_NIKON_WhiteBalancePresetVal4,		
		 N_("White Balance Preset Value 4")},
		{PTP_DPC_NIKON_ImageSharpening,			
		 N_("Sharpening")},
		{PTP_DPC_NIKON_ToneCompensation,		
		 N_("Tone Compensation")},
		{PTP_DPC_NIKON_ColorModel,			
		 N_("Color Model")},
		{PTP_DPC_NIKON_HueAdjustment,			
		 N_("Hue Adjustment")},
		{PTP_DPC_NIKON_NonCPULensDataFocalLength,	
		 N_("Lens Focal Length (Non CPU)")},
		{PTP_DPC_NIKON_NonCPULensDataMaximumAperture,	
		 N_("Lens Maximum Aperture (Non CPU)")},
		{PTP_DPC_NIKON_ShootingMode,			
		 N_("Shooting Mode")},
		{PTP_DPC_NIKON_JPEG_Compression_Policy,		
		 N_("JPEG Compression Policy")},
		{PTP_DPC_NIKON_ColorSpace,			
		 N_("Color Space")},
		{PTP_DPC_NIKON_AutoDXCrop,			
		 N_("Auto DX Crop")},
		{PTP_DPC_NIKON_CSMMenuBankSelect,		
		 "PTP_DPC_NIKON_CSMMenuBankSelect"},
		{PTP_DPC_NIKON_MenuBankNameA,			
		 N_("Menu Bank Name A")},
		{PTP_DPC_NIKON_MenuBankNameB,			
		 N_("Menu Bank Name B")},
		{PTP_DPC_NIKON_MenuBankNameC,			
		 N_("Menu Bank Name C")},
		{PTP_DPC_NIKON_MenuBankNameD,			
		 N_("Menu Bank Name D")},
		{PTP_DPC_NIKON_ResetBank,			
		 N_("Reset Menu Bank")},
		{PTP_DPC_NIKON_A1AFCModePriority,		
		 "PTP_DPC_NIKON_A1AFCModePriority"},
		{PTP_DPC_NIKON_A2AFSModePriority,		
		 "PTP_DPC_NIKON_A2AFSModePriority"},
		{PTP_DPC_NIKON_A3GroupDynamicAF,		
		 "PTP_DPC_NIKON_A3GroupDynamicAF"},
		{PTP_DPC_NIKON_A4AFActivation,			
		 "PTP_DPC_NIKON_A4AFActivation"},
		{PTP_DPC_NIKON_FocusAreaIllumManualFocus,	
		 "PTP_DPC_NIKON_FocusAreaIllumManualFocus"},
		{PTP_DPC_NIKON_FocusAreaIllumContinuous,	
		 "PTP_DPC_NIKON_FocusAreaIllumContinuous"},
		{PTP_DPC_NIKON_FocusAreaIllumWhenSelected,	
		 "PTP_DPC_NIKON_FocusAreaIllumWhenSelected"},
		{PTP_DPC_NIKON_FocusAreaWrap,			
		 N_("Focus Area Wrap")},
		{PTP_DPC_NIKON_VerticalAFON,			
		 N_("Vertical AF On")},
		{PTP_DPC_NIKON_AFLockOn,			
		 N_("AF Lock On")},
		{PTP_DPC_NIKON_FocusAreaZone,			
		 N_("Focus Area Zone")},
		{PTP_DPC_NIKON_EnableCopyright,			
		 N_("Enable Copyright")},
		{PTP_DPC_NIKON_ISOAuto,				
		 N_("Auto ISO")},
		{PTP_DPC_NIKON_EVISOStep,			
		 N_("Exposure ISO Step")},
		{PTP_DPC_NIKON_EVStep,				
		 N_("Exposure Step")},
		{PTP_DPC_NIKON_EVStepExposureComp,		
		 N_("Exposure Compensation (EV)")},
		{PTP_DPC_NIKON_ExposureCompensation,		
		 N_("Exposure Compensation")},
		{PTP_DPC_NIKON_CenterWeightArea,		
		 N_("Centre Weight Area")},
		{PTP_DPC_NIKON_ExposureBaseMatrix,		
		 N_("Exposure Base Matrix")},
		{PTP_DPC_NIKON_ExposureBaseCenter,		
		 N_("Exposure Base Center")},
		{PTP_DPC_NIKON_ExposureBaseSpot,		
		 N_("Exposure Base Spot")},
		{PTP_DPC_NIKON_LiveViewAF,			
		 N_("Live View AF")},
		{PTP_DPC_NIKON_AELockMode,			
		 N_("Exposure Lock")},
		{PTP_DPC_NIKON_AELAFLMode,			
		 N_("Focus Lock")},
		{PTP_DPC_NIKON_MeterOff,			
		 N_("Auto Meter Off Time")},
		{PTP_DPC_NIKON_SelfTimer,			
		 N_("Self Timer Delay")},
		{PTP_DPC_NIKON_MonitorOff,			
		 N_("LCD Off Time")},
		{PTP_DPC_NIKON_ImgConfTime,			
		 N_("Img Conf Time")},
		{PTP_DPC_NIKON_AngleLevel,			
		 N_("Angle Level")},
		{PTP_DPC_NIKON_D1ShootingSpeed,			
		 N_("Shooting Speed")},
		{PTP_DPC_NIKON_D2MaximumShots,			
		 N_("Maximum Shots")},
		{PTP_DPC_NIKON_D3ExpDelayMode,			
		 N_("Exposure delay mode")},
		{PTP_DPC_NIKON_LongExposureNoiseReduction,	
		 N_("Long Exposure Noise Reduction")},
		{PTP_DPC_NIKON_FileNumberSequence,		
		 N_("File Number Sequencing")},
		{PTP_DPC_NIKON_ControlPanelFinderRearControl,	
		 "PTP_DPC_NIKON_ControlPanelFinderRearControl"},
		{PTP_DPC_NIKON_ControlPanelFinderViewfinder,	
		 "PTP_DPC_NIKON_ControlPanelFinderViewfinder"},
		{PTP_DPC_NIKON_D7Illumination,			
		 N_("LCD Illumination")},
		{PTP_DPC_NIKON_NrHighISO,			
		 N_("High ISO noise reduction")},
		{PTP_DPC_NIKON_SHSET_CH_GUID_DISP,		
		 N_("On screen tips")},
		{PTP_DPC_NIKON_ArtistName,			
		 N_("Artist Name")},
		{PTP_DPC_NIKON_CopyrightInfo,			
		 N_("Copyright Information")},
		{PTP_DPC_NIKON_FlashSyncSpeed,			
		 N_("Flash Sync. Speed")},
		{PTP_DPC_NIKON_FlashShutterSpeed,		
		 N_("Flash Shutter Speed")},
		{PTP_DPC_NIKON_E3AAFlashMode,			
		 N_("Flash Mode")},
		{PTP_DPC_NIKON_E4ModelingFlash,			
		 N_("Modeling Flash")},
		{PTP_DPC_NIKON_BracketSet,			
		 N_("Bracket Set")},
		{PTP_DPC_NIKON_E6ManualModeBracketing,		
		 N_("Manual Mode Bracketing")},
		{PTP_DPC_NIKON_BracketOrder,			
		 N_("Bracket Order")},
		{PTP_DPC_NIKON_E8AutoBracketSelection,		
		 N_("Auto Bracket Selection")},
		{PTP_DPC_NIKON_BracketingSet, N_("NIKON Auto Bracketing Set")},	
		{PTP_DPC_NIKON_F1CenterButtonShootingMode,	
		 N_("Center Button Shooting Mode")},
		{PTP_DPC_NIKON_CenterButtonPlaybackMode,	
		 N_("Center Button Playback Mode")},
		{PTP_DPC_NIKON_F2Multiselector,			
		 N_("Multiselector")},
		{PTP_DPC_NIKON_F3PhotoInfoPlayback,		
		 N_("Photo Info. Playback")},
		{PTP_DPC_NIKON_F4AssignFuncButton,		
		 N_("Assign Func. Button")},
		{PTP_DPC_NIKON_F5CustomizeCommDials,		
		 N_("Customise Command Dials")},
		{PTP_DPC_NIKON_ReverseCommandDial,		
		 N_("Reverse Command Dial")},
		{PTP_DPC_NIKON_ApertureSetting,			
		 N_("Aperture Setting")},
		{PTP_DPC_NIKON_MenusAndPlayback,		
		 N_("Menus and Playback")},
		{PTP_DPC_NIKON_F6ButtonsAndDials,		
		 N_("Buttons and Dials")},
		{PTP_DPC_NIKON_NoCFCard,			
		 N_("No CF Card Release")},
		{PTP_DPC_NIKON_CenterButtonZoomRatio,		
		 N_("Center Button Zoom Ratio")},
		{PTP_DPC_NIKON_FunctionButton2,			
		 N_("Function Button 2")},
		{PTP_DPC_NIKON_AFAreaPoint,			
		 N_("AF Area Point")},
		{PTP_DPC_NIKON_NormalAFOn,			
		 N_("Normal AF On")},
		{PTP_DPC_NIKON_ImageCommentString,		
		 N_("Image Comment String")},
		{PTP_DPC_NIKON_ImageCommentEnable,		
		 N_("Image Comment Enable")},
		{PTP_DPC_NIKON_ImageRotation,			
		 N_("Image Rotation")},
		{PTP_DPC_NIKON_ManualSetLensNo,			
		 N_("Manual Set Lens Number")},
		{PTP_DPC_NIKON_MovScreenSize,			
		 N_("Movie Screen Size")},
		{PTP_DPC_NIKON_MovVoice,			
		 N_("Movie Voice")},
		{PTP_DPC_NIKON_Bracketing,			
		 N_("Bracketing Enable")},
		{PTP_DPC_NIKON_AutoExposureBracketStep,		
		 N_("Exposure Bracketing Step")},
		{PTP_DPC_NIKON_AutoExposureBracketProgram,	
		 N_("Exposure Bracketing Program")},
		{PTP_DPC_NIKON_AutoExposureBracketCount,	
		 N_("Auto Exposure Bracket Count")},
		{PTP_DPC_NIKON_WhiteBalanceBracketStep, N_("White Balance Bracket Step")}, 
		{PTP_DPC_NIKON_WhiteBalanceBracketProgram, N_("White Balance Bracket Program")}, 
		{PTP_DPC_NIKON_LensID,				
		 N_("Lens ID")},
		{PTP_DPC_NIKON_LensSort,			
		 N_("Lens Sort")},
		{PTP_DPC_NIKON_LensType,			
		 N_("Lens Type")},
		{PTP_DPC_NIKON_FocalLengthMin,			
		 N_("Min. Focal Length")},
		{PTP_DPC_NIKON_FocalLengthMax,			
		 N_("Max. Focal Length")},
		{PTP_DPC_NIKON_MaxApAtMinFocalLength,		
		 N_("Max. Aperture at Min. Focal Length")},
		{PTP_DPC_NIKON_MaxApAtMaxFocalLength,		
		 N_("Max. Aperture at Max. Focal Length")},
		{PTP_DPC_NIKON_FinderISODisp,			
		 N_("Finder ISO Display")},
		{PTP_DPC_NIKON_AutoOffPhoto,			
		 N_("Auto Off Photo")},
		{PTP_DPC_NIKON_AutoOffMenu,			
		 N_("Auto Off Menu")},
		{PTP_DPC_NIKON_AutoOffInfo,			
		 N_("Auto Off Info")},
		{PTP_DPC_NIKON_SelfTimerShootNum,		
		 N_("Self Timer Shot Number")},
		{PTP_DPC_NIKON_VignetteCtrl,			
		 N_("Vignette Control")},
		{PTP_DPC_NIKON_ExposureTime,			
		 N_("Nikon Exposure Time")},
		{PTP_DPC_NIKON_ACPower, N_("AC Power")},	
		{PTP_DPC_NIKON_WarningStatus, N_("Warning Status")},
		{PTP_DPC_NIKON_MaximumShots,			
		 N_("Maximum Shots")},
		{PTP_DPC_NIKON_AFLockStatus, N_("AF Locked")},
		{PTP_DPC_NIKON_AELockStatus, N_("AE Locked")},
		{PTP_DPC_NIKON_FVLockStatus, N_("FV Locked")},
		{PTP_DPC_NIKON_AutofocusLCDTopMode2,		
		 N_("AF LCD Top Mode 2")},
		{PTP_DPC_NIKON_AutofocusArea,			
		 N_("Active AF Sensor")},
		{PTP_DPC_NIKON_FlexibleProgram,			
		 N_("Flexible Program")},
		{PTP_DPC_NIKON_LightMeter,			
		 N_("Exposure Meter")},
		{PTP_DPC_NIKON_RecordingMedia,			
		 N_("Recording Media")},
		{PTP_DPC_NIKON_USBSpeed,			
		 N_("USB Speed")},
		{PTP_DPC_NIKON_CCDNumber,			
		 N_("CCD Serial Number")},
		{PTP_DPC_NIKON_CameraOrientation,		
		 N_("Camera Orientation")},
		{PTP_DPC_NIKON_GroupPtnType,			
		 N_("Group PTN Type")},
		{PTP_DPC_NIKON_FNumberLock,			
		 N_("FNumber Lock")},
		{PTP_DPC_NIKON_ExposureApertureLock,		
		 N_("Exposure Aperture Lock")},
		{PTP_DPC_NIKON_TVLockSetting,			
		 N_("TV Lock Setting")},
		{PTP_DPC_NIKON_AVLockSetting,			
		 N_("AV Lock Setting")},
		{PTP_DPC_NIKON_IllumSetting,			
		 N_("Illum Setting")},
		{PTP_DPC_NIKON_FocusPointBright,		
		 N_("Focus Point Bright")},
		{PTP_DPC_NIKON_ExternalFlashAttached,		
		 N_("External Flash Attached")},
		{PTP_DPC_NIKON_ExternalFlashStatus,		
		 N_("External Flash Status")},
		{PTP_DPC_NIKON_ExternalFlashSort,		
		 N_("External Flash Sort")},
		{PTP_DPC_NIKON_ExternalFlashMode,		
		 N_("External Flash Mode")},
		{PTP_DPC_NIKON_ExternalFlashCompensation,	
		 N_("External Flash Compensation")},
		{PTP_DPC_NIKON_NewExternalFlashMode,		
		 N_("External Flash Mode")},
		{PTP_DPC_NIKON_FlashExposureCompensation,	
		 N_("Flash Exposure Compensation")},
		{PTP_DPC_NIKON_OptimizeImage,			
		 N_("Optimize Image")},
		{PTP_DPC_NIKON_Saturation,			
		 N_("Saturation")},
		{PTP_DPC_NIKON_BW_FillerEffect,			
		 N_("BW Filler Effect")},
		{PTP_DPC_NIKON_BW_Sharpness,			
		 N_("BW Sharpness")},
		{PTP_DPC_NIKON_BW_Contrast,			
		 N_("BW Contrast")},
		{PTP_DPC_NIKON_BW_Setting_Type,			
		 N_("BW Setting Type")},
		{PTP_DPC_NIKON_Slot2SaveMode,			
		 N_("Slot 2 Save Mode")},
		{PTP_DPC_NIKON_RawBitMode,			
		 N_("Raw Bit Mode")},
		{PTP_DPC_NIKON_ISOAutoTime,			
		 N_("ISO Auto Time")},
		{PTP_DPC_NIKON_FlourescentType,			
		 N_("Flourescent Type")},
		{PTP_DPC_NIKON_TuneColourTemperature,		
		 N_("Tune Colour Temperature")},
		{PTP_DPC_NIKON_TunePreset0,			
		 N_("Tune Preset 0")},
		{PTP_DPC_NIKON_TunePreset1,			
		 N_("Tune Preset 1")},
		{PTP_DPC_NIKON_TunePreset2,			
		 N_("Tune Preset 2")},
		{PTP_DPC_NIKON_TunePreset3,			
		 N_("Tune Preset 3")},
		{PTP_DPC_NIKON_TunePreset4,			
		 N_("Tune Preset 4")},
		{PTP_DPC_NIKON_BeepOff,				
		 N_("AF Beep Mode")},
		{PTP_DPC_NIKON_AutofocusMode,			
		 N_("Autofocus Mode")},
		{PTP_DPC_NIKON_AFAssist,			
		 N_("AF Assist Lamp")},
		{PTP_DPC_NIKON_PADVPMode,			
		 N_("Auto ISO P/A/DVP Setting")},
		{PTP_DPC_NIKON_ImageReview,			
		 N_("Image Review")},
		{PTP_DPC_NIKON_AFAreaIllumination,		
		 N_("AF Area Illumination")},
		{PTP_DPC_NIKON_FlashMode,			
		 N_("Flash Mode")},
		{PTP_DPC_NIKON_FlashCommanderMode,	 	
		 N_("Flash Commander Mode")},
		{PTP_DPC_NIKON_FlashSign,			
		 N_("Flash Sign")},
		{PTP_DPC_NIKON_ISOAuto,				
		 N_("ISO Auto")},
		{PTP_DPC_NIKON_RemoteTimeout,			
		 N_("Remote Timeout")},
		{PTP_DPC_NIKON_GridDisplay,			
		 N_("Viewfinder Grid Display")},
		{PTP_DPC_NIKON_FlashModeManualPower,		
		 N_("Flash Mode Manual Power")},
		{PTP_DPC_NIKON_FlashModeCommanderPower,		
		 N_("Flash Mode Commander Power")},
		{PTP_DPC_NIKON_AutoFP,				
		 N_("Auto FP")},
		{PTP_DPC_NIKON_CSMMenu,				
		 N_("CSM Menu")},
		{PTP_DPC_NIKON_WarningDisplay,			
		 N_("Warning Display")},
		{PTP_DPC_NIKON_BatteryCellKind,			
		 N_("Battery Cell Kind")},
		{PTP_DPC_NIKON_ISOAutoHiLimit,			
		 N_("ISO Auto High Limit")},
		{PTP_DPC_NIKON_DynamicAFArea,			
		 N_("Dynamic AF Area")},
		{PTP_DPC_NIKON_ContinuousSpeedHigh,		
		 N_("Continuous Speed High")},
		{PTP_DPC_NIKON_InfoDispSetting,			
		 N_("Info Disp Setting")},
		{PTP_DPC_NIKON_PreviewButton,			
		 N_("Preview Button")},
		{PTP_DPC_NIKON_PreviewButton2,			
		 N_("Preview Button 2")},
		{PTP_DPC_NIKON_AEAFLockButton2,			
		 N_("AEAF Lock Button 2")},
		{PTP_DPC_NIKON_IndicatorDisp,			
		 N_("Indicator Display")},
		{PTP_DPC_NIKON_CellKindPriority,		
		 N_("Cell Kind Priority")},
		{PTP_DPC_NIKON_BracketingFramesAndSteps,	
		 N_("Bracketing Frames and Steps")},
		{PTP_DPC_NIKON_LiveViewMode,			
		 N_("Live View Mode")},
		{PTP_DPC_NIKON_LiveViewDriveMode,		
		 N_("Live View Drive Mode")},
		{PTP_DPC_NIKON_LiveViewStatus,			
		 N_("Live View Status")},
		{PTP_DPC_NIKON_LiveViewImageZoomRatio,		
		 N_("Live View Image Zoom Ratio")},
		{PTP_DPC_NIKON_LiveViewProhibitCondition,	
		 N_("Live View Prohibit Condition")},
		{PTP_DPC_NIKON_ExposureDisplayStatus,		
		 N_("Exposure Display Status")},
		{PTP_DPC_NIKON_ExposureIndicateStatus,		
		 N_("Exposure Indicate Status")},
		{PTP_DPC_NIKON_ExposureIndicateLightup,		
		 N_("Exposure Indicate Lightup")},
		{PTP_DPC_NIKON_FlashOpen,			
		 N_("Flash Open")},
		{PTP_DPC_NIKON_FlashCharged,			
		 N_("Flash Charged")},
		{PTP_DPC_NIKON_FlashMRepeatValue,		
		 N_("Flash MRepeat Value")},
		{PTP_DPC_NIKON_FlashMRepeatCount,		
		 N_("Flash MRepeat Count")},
		{PTP_DPC_NIKON_FlashMRepeatInterval,		
		 N_("Flash MRepeat Interval")},
		{PTP_DPC_NIKON_FlashCommandChannel,		
		 N_("Flash Command Channel")},
		{PTP_DPC_NIKON_FlashCommandSelfMode,		
		 N_("Flash Command Self Mode")},
		{PTP_DPC_NIKON_FlashCommandSelfCompensation,	
		 N_("Flash Command Self Compensation")},
		{PTP_DPC_NIKON_FlashCommandSelfValue,		
		 N_("Flash Command Self Value")},
		{PTP_DPC_NIKON_FlashCommandAMode,		
		 N_("Flash Command A Mode")},
		{PTP_DPC_NIKON_FlashCommandACompensation,	
		 N_("Flash Command A Compensation")},
		{PTP_DPC_NIKON_FlashCommandAValue,		
		 N_("Flash Command A Value")},
		{PTP_DPC_NIKON_FlashCommandBMode,		
		 N_("Flash Command B Mode")},
		{PTP_DPC_NIKON_FlashCommandBCompensation,	
		 N_("Flash Command B Compensation")},
		{PTP_DPC_NIKON_FlashCommandBValue,		
		 N_("Flash Command B Value")},
		{PTP_DPC_NIKON_ActivePicCtrlItem,		
		 N_("Active Pic Ctrl Item")},
		{PTP_DPC_NIKON_ChangePicCtrlItem,		
		 N_("Change Pic Ctrl Item")},
		{0,NULL}
	};
        struct {
		uint16_t dpc;
		const char *txt;
        } ptp_device_properties_MTP[] = {
		{PTP_DPC_MTP_SecureTime,        N_("Secure Time")},		
		{PTP_DPC_MTP_DeviceCertificate, N_("Device Certificate")},	
		{PTP_DPC_MTP_RevocationInfo,    N_("Revocation Info")},		
		{PTP_DPC_MTP_SynchronizationPartner,				
		 N_("Synchronization Partner")},
		{PTP_DPC_MTP_DeviceFriendlyName,				
		 N_("Friendly Device Name")},
		{PTP_DPC_MTP_VolumeLevel,       N_("Volume Level")},		
		{PTP_DPC_MTP_DeviceIcon,        N_("Device Icon")},		
		{PTP_DPC_MTP_SessionInitiatorInfo,	N_("Session Initiator Info")},
		{PTP_DPC_MTP_PerceivedDeviceType,	N_("Perceived Device Type")},
		{PTP_DPC_MTP_PlaybackRate,      N_("Playback Rate")},		
		{PTP_DPC_MTP_PlaybackObject,    N_("Playback Object")},		
		{PTP_DPC_MTP_PlaybackContainerIndex,				
		 N_("Playback Container Index")},
		{PTP_DPC_MTP_PlaybackPosition,  N_("Playback Position")},	
		{PTP_DPC_MTP_PlaysForSureID,    N_("PlaysForSure ID")},		
		{0,NULL}
        };

	for (i=0; ptp_device_properties[i].txt!=NULL; i++)
		if (ptp_device_properties[i].dpc==dpc)
			return (ptp_device_properties[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_MICROSOFT
	    || params->deviceinfo.VendorExtensionID==PTP_VENDOR_MTP)
		for (i=0; ptp_device_properties_MTP[i].txt!=NULL; i++)
			if (ptp_device_properties_MTP[i].dpc==dpc)
				return (ptp_device_properties_MTP[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_EASTMAN_KODAK)
		for (i=0; ptp_device_properties_EK[i].txt!=NULL; i++)
			if (ptp_device_properties_EK[i].dpc==dpc)
				return (ptp_device_properties_EK[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_CANON)
		for (i=0; ptp_device_properties_Canon[i].txt!=NULL; i++)
			if (ptp_device_properties_Canon[i].dpc==dpc)
				return (ptp_device_properties_Canon[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_NIKON)
		for (i=0; ptp_device_properties_Nikon[i].txt!=NULL; i++)
			if (ptp_device_properties_Nikon[i].dpc==dpc)
				return (ptp_device_properties_Nikon[i].txt);

	return NULL;
}

static int64_t
_value_to_num(PTPPropertyValue *data, uint16_t dt) {
	if (dt == PTP_DTC_STR) {
		if (!data->str)
			return 0;
		return atol(data->str);
	}
	if (dt & PTP_DTC_ARRAY_MASK) {
		return 0;
	} else {
		switch (dt) {
		case PTP_DTC_UNDEF: 
			return 0;
		case PTP_DTC_INT8:
			return data->i8;
		case PTP_DTC_UINT8:
			return data->u8;
		case PTP_DTC_INT16:
			return data->i16;
		case PTP_DTC_UINT16:
			return data->u16;
		case PTP_DTC_INT32:
			return data->i32;
		case PTP_DTC_UINT32:
			return data->u32;
		default:
			return 0;
		}
	}

	return 0;
}

#define PTP_VAL_BOOL(dpc) {dpc, 0, N_("Off")}, {dpc, 1, N_("On")}
#define PTP_VENDOR_VAL_BOOL(dpc,vendor) {dpc, vendor, 0, N_("Off")}, {dpc, vendor, 1, N_("On")}
#define PTP_VENDOR_VAL_RBOOL(dpc,vendor) {dpc, vendor, 0, N_("On")}, {dpc, vendor, 1, N_("Off")}
#define PTP_VENDOR_VAL_YN(dpc,vendor) {dpc, vendor, 0, N_("No")}, {dpc, vendor, 1, N_("Yes")}

int
ptp_render_property_value(PTPParams* params, uint16_t dpc,
			  PTPDevicePropDesc *dpd, int length, char *out)
{
	int i;
	int64_t	kval;

	struct {
		uint16_t dpc;
		uint16_t vendor;
		double coef;
		double bias;
		const char *format;
	} ptp_value_trans[] = {
		{PTP_DPC_BatteryLevel, 0, 1.0, 0.0, "%.0f%%"},		
		{PTP_DPC_FNumber, 0, 0.01, 0.0, "f/%.2g"},		
		{PTP_DPC_FocalLength, 0, 0.01, 0.0, "%.0f mm"},		
		{PTP_DPC_FocusDistance, 0, 0.01, 0.0, "%.0f mm"},	
		{PTP_DPC_ExposureTime, 0, 0.00001, 0.0, "%.2g sec"},	
		{PTP_DPC_ExposureIndex, 0, 1.0, 0.0, "ISO %.0f"},	
		{PTP_DPC_ExposureBiasCompensation, 0, 0.001, 0.0, N_("%.1f stops")},
		{PTP_DPC_DigitalZoom, 0, 0.1, 0.0, "%.1f"},		

		
		{PTP_DPC_NIKON_LightMeter, PTP_VENDOR_NIKON, 0.08333, 0.0, N_("%.1f stops")},
		{PTP_DPC_NIKON_FlashExposureCompensation, PTP_VENDOR_NIKON, 0.16666, 0.0, N_("%.1f stops")}, 
		{PTP_DPC_NIKON_CenterWeightArea, PTP_VENDOR_NIKON, 2.0, 6.0, N_("%.0f mm")},
		{PTP_DPC_NIKON_FocalLengthMin, PTP_VENDOR_NIKON, 0.01, 0.0, "%.0f mm"}, 
		{PTP_DPC_NIKON_FocalLengthMax, PTP_VENDOR_NIKON, 0.01, 0.0, "%.0f mm"}, 
		{PTP_DPC_NIKON_MaxApAtMinFocalLength, PTP_VENDOR_NIKON, 0.01, 0.0, "f/%.2g"}, 
		{PTP_DPC_NIKON_MaxApAtMaxFocalLength, PTP_VENDOR_NIKON, 0.01, 0.0, "f/%.2g"}, 
		{PTP_DPC_NIKON_ExternalFlashCompensation, PTP_VENDOR_NIKON, 1.0/6.0, 0.0,"%.0f"}, 
		{PTP_DPC_NIKON_ExposureIndicateStatus, PTP_VENDOR_NIKON, 0.08333, 0.0, N_("%.1f stops")},
		{0, 0, 0.0, 0.0, NULL}
	};

	struct {
		uint16_t dpc;
		uint16_t vendor;
		int64_t key;
		char *value;
	} ptp_value_list[] = {
		{PTP_DPC_CompressionSetting, 0, 0, N_("JPEG Basic")},	
		{PTP_DPC_CompressionSetting, 0, 1, N_("JPEG Norm")},
		{PTP_DPC_CompressionSetting, 0, 2, N_("JPEG Fine")},
		{PTP_DPC_CompressionSetting, 0, 4, N_("RAW")},
		{PTP_DPC_CompressionSetting, 0, 5, N_("RAW + JPEG Basic")},
		{PTP_DPC_WhiteBalance, 0, 1, N_("Manual")},
		{PTP_DPC_WhiteBalance, 0, 2, N_("Automatic")},		
		{PTP_DPC_WhiteBalance, 0, 3, N_("One-push Automatic")},
		{PTP_DPC_WhiteBalance, 0, 4, N_("Daylight")},
		{PTP_DPC_WhiteBalance, 0, 5, N_("Fluorescent")},
		{PTP_DPC_WhiteBalance, 0, 6, N_("Incandescent")},
		{PTP_DPC_WhiteBalance, 0, 7, N_("Flash")},
		{PTP_DPC_WhiteBalance, PTP_VENDOR_NIKON, 32784, N_("Cloudy")},
		{PTP_DPC_WhiteBalance, PTP_VENDOR_NIKON, 32785, N_("Shade")},
		{PTP_DPC_WhiteBalance, PTP_VENDOR_NIKON, 32786, N_("Color Temperature")},
		{PTP_DPC_WhiteBalance, PTP_VENDOR_NIKON, 32787, N_("Preset")},
		{PTP_DPC_FocusMode, 0, 1, N_("Manual Focus")},		
		{PTP_DPC_FocusMode, 0, 2, N_("Automatic")},
		{PTP_DPC_FocusMode, 0, 3, N_("Automatic Macro (close-up)")},
		{PTP_DPC_FocusMode, PTP_VENDOR_NIKON, 32784, "AF-S"},
		{PTP_DPC_FocusMode, PTP_VENDOR_NIKON, 32785, "AF-C"},
		{PTP_DPC_FocusMode, PTP_VENDOR_NIKON, 32786, "AF-A"},
		{PTP_DPC_ExposureMeteringMode, 0, 1, N_("Average")},	
		{PTP_DPC_ExposureMeteringMode, 0, 2, N_("Center Weighted Average")},
		{PTP_DPC_ExposureMeteringMode, 0, 3, N_("Multi-spot")},
		{PTP_DPC_ExposureMeteringMode, 0, 4, N_("Center-spot")},
		{PTP_DPC_FlashMode, 0, 0, N_("Undefined")},		
		{PTP_DPC_FlashMode, 0, 1, N_("Automatic flash")},
		{PTP_DPC_FlashMode, 0, 2, N_("Flash off")},
		{PTP_DPC_FlashMode, 0, 3, N_("Fill flash")},
		{PTP_DPC_FlashMode, 0, 4, N_("Automatic Red-eye Reduction")},
		{PTP_DPC_FlashMode, 0, 5, N_("Red-eye fill flash")},
		{PTP_DPC_FlashMode, 0, 6, N_("External sync")},
		{PTP_DPC_FlashMode, PTP_VENDOR_NIKON, 32784, N_("Default")},
		{PTP_DPC_FlashMode, PTP_VENDOR_NIKON, 32785, N_("Slow Sync")},
		{PTP_DPC_FlashMode, PTP_VENDOR_NIKON, 32786, N_("Rear Curtain Sync + Slow Sync")},
		{PTP_DPC_FlashMode, PTP_VENDOR_NIKON, 32787, N_("Red-eye Reduction + Slow Sync")},
		{PTP_DPC_ExposureProgramMode, 0, 1, "M"},		
		{PTP_DPC_ExposureProgramMode, 0, 3, "A"},
		{PTP_DPC_ExposureProgramMode, 0, 4, "S"},
		{PTP_DPC_ExposureProgramMode, 0, 2, "P"},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32784, N_("Auto")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32785, N_("Portrait")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32786, N_("Landscape")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32787, N_("Macro")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32788, N_("Sports")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32790, N_("Night Landscape")},
		{PTP_DPC_ExposureProgramMode, PTP_VENDOR_NIKON, 32789, N_("Night Portrait")},
		{PTP_DPC_StillCaptureMode, 0, 1, N_("Single Shot")},	
		{PTP_DPC_StillCaptureMode, 0, 2, N_("Power Wind")},
		{PTP_DPC_StillCaptureMode, 0, 3, N_("Timelapse")},
		{PTP_DPC_StillCaptureMode, PTP_VENDOR_NIKON, 32784, N_("Continuous Low Speed")},
		{PTP_DPC_StillCaptureMode, PTP_VENDOR_NIKON, 32785, N_("Timer")},
		{PTP_DPC_StillCaptureMode, PTP_VENDOR_NIKON, 32787, N_("Remote")},
		{PTP_DPC_StillCaptureMode, PTP_VENDOR_NIKON, 32787, N_("Mirror Up")},
		{PTP_DPC_StillCaptureMode, PTP_VENDOR_NIKON, 32788, N_("Timer + Remote")},
		{PTP_DPC_FocusMeteringMode, 0, 1, N_("Centre-spot")},	
		{PTP_DPC_FocusMeteringMode, 0, 2, N_("Multi-spot")},
		{PTP_DPC_FocusMeteringMode, PTP_VENDOR_NIKON, 32784, N_("Single Area")},
		{PTP_DPC_FocusMeteringMode, PTP_VENDOR_NIKON, 32785, N_("Closest Subject")},
		{PTP_DPC_FocusMeteringMode, PTP_VENDOR_NIKON, 32786, N_("Group Dynamic")},


		
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 0, N_("Auto")},	
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 1, N_("Normal")},
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 2, N_("Low")},
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 3, N_("Medium Low")},
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 4, N_("Medium high")},
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 5, N_("High")},
		{PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, 6, N_("None")},

		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 0, N_("Auto")},	
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 1, N_("Normal")},
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 2, N_("Low contrast")},
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 3, N_("Medium Low")},
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 4, N_("Medium High")},
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 5, N_("High control")},
		{PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, 6, N_("Custom")},

		{PTP_DPC_NIKON_ColorModel, PTP_VENDOR_NIKON, 0, "sRGB"},		
		{PTP_DPC_NIKON_ColorModel, PTP_VENDOR_NIKON, 1, "AdobeRGB"},
		{PTP_DPC_NIKON_ColorModel, PTP_VENDOR_NIKON, 2, "sRGB"},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_AutoDXCrop,PTP_VENDOR_NIKON),	   	

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_FocusAreaWrap,PTP_VENDOR_NIKON),   	

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_EnableCopyright,PTP_VENDOR_NIKON),   	
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_ISOAuto,PTP_VENDOR_NIKON),	   	

		
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 0, "1/125"},			
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 1, "1/60"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 2, "1/30"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 3, "1/15"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 4, "1/8"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 5, "1/4"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 6, "1/2"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 7, "1"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 8, "2"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 9, "4"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 10, "8"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 11, "15"},
		{PTP_DPC_NIKON_ISOAuto,	PTP_VENDOR_NIKON, 12, "30"},

		{PTP_DPC_NIKON_EVStep, PTP_VENDOR_NIKON, 0, "1/3"},			
		{PTP_DPC_NIKON_EVStep, PTP_VENDOR_NIKON, 1, "1/2"},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_ExposureCompensation,PTP_VENDOR_NIKON),
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_AELockMode,PTP_VENDOR_NIKON),    	

		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 0, N_("AE/AF Lock")},	
		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 1, N_("AF Lock only")},
		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 2, N_("AE Lock only")},
		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 3, N_("AF Lock Hold")},
		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 4, N_("AF On")},
		{PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, 5, N_("Flash Lock")},

		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 0, N_("4 seconds")},		
		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 1, N_("6 seconds")},
		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 2, N_("8 seconds")},
		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 3, N_("16 seconds")},
		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 4, N_("30 minutes")},
		{PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, 5, N_("30 seconds")},

		{PTP_DPC_NIKON_SelfTimer, PTP_VENDOR_NIKON, 0, N_("2 seconds")},	
		{PTP_DPC_NIKON_SelfTimer, PTP_VENDOR_NIKON, 1, N_("5 seconds")},
		{PTP_DPC_NIKON_SelfTimer, PTP_VENDOR_NIKON, 2, N_("10 seconds")},
		{PTP_DPC_NIKON_SelfTimer, PTP_VENDOR_NIKON, 3, N_("20 seconds")},

		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 0, N_("10 seconds")},	
		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 1, N_("20 seconds")},
		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 2, N_("1 minute")},
		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 3, N_("5 minutes")},
		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 4, N_("10 minutes")},
		{PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, 5, N_("5 seconds")}, 

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_D3ExpDelayMode,PTP_VENDOR_NIKON),	
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_LongExposureNoiseReduction,PTP_VENDOR_NIKON),	
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_FileNumberSequence,PTP_VENDOR_NIKON),	
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_D7Illumination,PTP_VENDOR_NIKON),	

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_SHSET_CH_GUID_DISP,PTP_VENDOR_NIKON),	

		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 0, "1/60s"},		
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 1, "1/30s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 2, "1/15s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 3, "1/8s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 4, "1/4s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 5, "1/2s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 6, "1s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 7, "2s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 8, "4s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 9, "8s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 10, "15s"},
		{PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, 11, "30s"},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_E4ModelingFlash,PTP_VENDOR_NIKON),	

		{PTP_DPC_NIKON_BracketSet, PTP_VENDOR_NIKON, 0, N_("AE & Flash")},	
		{PTP_DPC_NIKON_BracketSet, PTP_VENDOR_NIKON, 1, N_("AE only")},
		{PTP_DPC_NIKON_BracketSet, PTP_VENDOR_NIKON, 2, N_("Flash only")},
		{PTP_DPC_NIKON_BracketSet, PTP_VENDOR_NIKON, 3, N_("WB bracketing")},

		{PTP_DPC_NIKON_BracketOrder, PTP_VENDOR_NIKON, 0, N_("MTR > Under")},	
		{PTP_DPC_NIKON_BracketOrder, PTP_VENDOR_NIKON, 1, N_("Under > MTR")},

		{PTP_DPC_NIKON_F1CenterButtonShootingMode, PTP_VENDOR_NIKON, 0, N_("Reset focus point to center")}, 
		{PTP_DPC_NIKON_F1CenterButtonShootingMode, PTP_VENDOR_NIKON, 1, N_("Highlight active focus point")},
		{PTP_DPC_NIKON_F1CenterButtonShootingMode, PTP_VENDOR_NIKON, 2, N_("Unused")},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_F3PhotoInfoPlayback,PTP_VENDOR_NIKON),
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_F5CustomizeCommDials,PTP_VENDOR_NIKON),
		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_ReverseCommandDial,PTP_VENDOR_NIKON),	
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_F6ButtonsAndDials,PTP_VENDOR_NIKON),	
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_NoCFCard,PTP_VENDOR_NIKON),		
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_AFAreaPoint,PTP_VENDOR_NIKON),	

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_ImageCommentEnable,PTP_VENDOR_NIKON),	
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_ImageRotation,PTP_VENDOR_NIKON),	

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_MovVoice,PTP_VENDOR_NIKON),		

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_Bracketing,PTP_VENDOR_NIKON),		

		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 0, N_("Unknown")},		
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 38, "Sigma 70-300mm 1:4-5.6 D APO Macro"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 83, "AF Nikkor 80-200mm 1:2.8 D ED"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 118, "AF Nikkor 50mm 1:1.8 D"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 127, "AF-S Nikkor 18-70mm 1:3.5-4.5G ED DX"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 139, "AF-S Nikkor 18-200mm 1:3.5-5.6 GED DX VR"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 147, "AF-S Nikkor 24-70mm 1:2.8G ED DX"},
		{PTP_DPC_NIKON_LensID, PTP_VENDOR_NIKON, 154, "AF-S Nikkor 18-55mm 1:3.5-F5.6G DX VR"},
		{PTP_DPC_NIKON_FinderISODisp, PTP_VENDOR_NIKON, 0, "Show ISO sensitivity"},
		{PTP_DPC_NIKON_FinderISODisp, PTP_VENDOR_NIKON, 1, "Show ISO/Easy ISO"},
		{PTP_DPC_NIKON_FinderISODisp, PTP_VENDOR_NIKON, 2, "Show frame count"},

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ACPower,PTP_VENDOR_NIKON),		
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_AFLockStatus,PTP_VENDOR_NIKON),		
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_AELockStatus,PTP_VENDOR_NIKON),		
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_FVLockStatus,PTP_VENDOR_NIKON),		

		{PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, 0, N_("Centre")},	
		{PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, 1, N_("Top")},
		{PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, 2, N_("Bottom")},
		{PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, 3, N_("Left")},
		{PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, 4, N_("Right")},

		{PTP_DPC_NIKON_RecordingMedia, PTP_VENDOR_NIKON, 0, N_("Card")},	
		{PTP_DPC_NIKON_RecordingMedia, PTP_VENDOR_NIKON, 1, N_("SDRam")},

		{PTP_DPC_NIKON_USBSpeed, PTP_VENDOR_NIKON, 0, N_("USB 1.1")},		
		{PTP_DPC_NIKON_USBSpeed, PTP_VENDOR_NIKON, 1, N_("USB 2.0")},

		{PTP_DPC_NIKON_CameraOrientation, PTP_VENDOR_NIKON, 0, "0'"},		
		{PTP_DPC_NIKON_CameraOrientation, PTP_VENDOR_NIKON, 1, "270'"},
		{PTP_DPC_NIKON_CameraOrientation, PTP_VENDOR_NIKON, 2, "90'"},
		{PTP_DPC_NIKON_CameraOrientation, PTP_VENDOR_NIKON, 3, "180'"},

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_FNumberLock,PTP_VENDOR_NIKON),		
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ExposureApertureLock,PTP_VENDOR_NIKON),	
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_TVLockSetting,PTP_VENDOR_NIKON),	
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_AVLockSetting,PTP_VENDOR_NIKON),	

		{PTP_DPC_NIKON_IllumSetting,PTP_VENDOR_NIKON,0,N_("LCD Backlight")},	
		{PTP_DPC_NIKON_IllumSetting,PTP_VENDOR_NIKON,1,N_("LCD Backlight and Info Display")},

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ExternalFlashAttached,PTP_VENDOR_NIKON),
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ExternalFlashStatus,PTP_VENDOR_NIKON),	

		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 0, N_("Normal")},	
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 1, N_("Vivid")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 2, N_("Sharper")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 3, N_("Softer")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 4, N_("Direct Print")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 5, N_("Portrait")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 6, N_("Landscape")},
		{PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, 7, N_("Custom")},

		{PTP_DPC_NIKON_Saturation, PTP_VENDOR_NIKON, 0, N_("Normal")},		
		{PTP_DPC_NIKON_Saturation, PTP_VENDOR_NIKON, 1, N_("Moderate")},
		{PTP_DPC_NIKON_Saturation, PTP_VENDOR_NIKON, 2, N_("Enhanced")},

		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_BeepOff,PTP_VENDOR_NIKON),		

		{PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, 0, N_("AF-S")},	 	
		{PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, 1, N_("AF-C")},
		{PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, 2, N_("AF-A")},
		{PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, 3, N_("MF (fixed)")},
		{PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, 4, N_("MF (selection)")},

		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_AFAssist,PTP_VENDOR_NIKON),   	

		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 0,  "1/125"},		
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 1,  "1/60"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 2,  "1/30"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 3,  "1/15"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 4,  "1/8"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 5,  "1/4"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 6,  "1/2"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 7,  "1"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 8,  "2"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 9,  "4"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 10, "8"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 11, "15"},
		{PTP_DPC_NIKON_PADVPMode, PTP_VENDOR_NIKON, 12, "30"},

		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_ImageReview,PTP_VENDOR_NIKON),	

		{PTP_DPC_NIKON_AFAreaIllumination, PTP_VENDOR_NIKON, 0, N_("Auto")},	
		{PTP_DPC_NIKON_AFAreaIllumination, PTP_VENDOR_NIKON, 1, N_("Off")},
		{PTP_DPC_NIKON_AFAreaIllumination, PTP_VENDOR_NIKON, 2, N_("On")},

		{PTP_DPC_NIKON_FlashMode, PTP_VENDOR_NIKON, 0, "iTTL"},			
		{PTP_DPC_NIKON_FlashMode, PTP_VENDOR_NIKON, 1, N_("Manual")},
		{PTP_DPC_NIKON_FlashMode, PTP_VENDOR_NIKON, 2, N_("Commander")},

		{PTP_DPC_NIKON_FlashCommanderMode, PTP_VENDOR_NIKON, 0, N_("TTL")},	
		{PTP_DPC_NIKON_FlashCommanderMode, PTP_VENDOR_NIKON, 1, N_("Auto Aperture")},
		{PTP_DPC_NIKON_FlashCommanderMode, PTP_VENDOR_NIKON, 2, N_("Full Manual")},

		PTP_VENDOR_VAL_RBOOL(PTP_DPC_NIKON_FlashSign,PTP_VENDOR_NIKON),		

		{PTP_DPC_NIKON_RemoteTimeout, PTP_VENDOR_NIKON, 0, N_("1 min")},	
		{PTP_DPC_NIKON_RemoteTimeout, PTP_VENDOR_NIKON, 1, N_("5 mins")},
		{PTP_DPC_NIKON_RemoteTimeout, PTP_VENDOR_NIKON, 2, N_("10 mins")},
		{PTP_DPC_NIKON_RemoteTimeout, PTP_VENDOR_NIKON, 3, N_("15 mins")},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_NIKON_GridDisplay,PTP_VENDOR_NIKON),	

		{PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, 0, N_("Full")},	
		{PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, 1, "1/2"},
		{PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, 2, "1/4"},
		{PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, 3, "1/8"},
		{PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, 4, "1/16"},

		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 0, N_("Full")},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 1, "1/2"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 2, "1/4"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 3, "1/8"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 4, "1/16"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 5, "1/32"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 6, "1/64"},
		{PTP_DPC_NIKON_FlashModeCommanderPower, PTP_VENDOR_NIKON, 7, "1/128"},

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_CSMMenu,PTP_VENDOR_NIKON),		
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_WarningDisplay,PTP_VENDOR_NIKON),	

		{PTP_DPC_NIKON_BatteryCellKind, PTP_VENDOR_NIKON, 0, "LR6 (AA alkaline)"},
		{PTP_DPC_NIKON_BatteryCellKind, PTP_VENDOR_NIKON, 1, "HR6 (AA Ni-Mh)"},
		{PTP_DPC_NIKON_BatteryCellKind, PTP_VENDOR_NIKON, 2, "FR6 (AA Lithium)"},
		{PTP_DPC_NIKON_BatteryCellKind, PTP_VENDOR_NIKON, 3, "ZR6 (AA Ni-Mn)"},

		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 0, "400"},		
		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 1, "800"},
		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 2, "1600"},
		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 3, "3200"},
		{PTP_DPC_NIKON_ISOAutoHiLimit, PTP_VENDOR_NIKON, 4, "Hi 1"},

		{PTP_DPC_NIKON_InfoDispSetting, PTP_VENDOR_NIKON, 0, N_("Auto")},	
		{PTP_DPC_NIKON_InfoDispSetting, PTP_VENDOR_NIKON, 1, N_("Dark on light")},
		{PTP_DPC_NIKON_InfoDispSetting, PTP_VENDOR_NIKON, 2, N_("Light on dark")},

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_IndicatorDisp,PTP_VENDOR_NIKON),	

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_LiveViewStatus,PTP_VENDOR_NIKON),	

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ExposureDisplayStatus,PTP_VENDOR_NIKON),
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_InfoDispErrStatus,PTP_VENDOR_NIKON),	
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_ExposureIndicateLightup,PTP_VENDOR_NIKON),

		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_FlashOpen,PTP_VENDOR_NIKON),		
		PTP_VENDOR_VAL_YN(PTP_DPC_NIKON_FlashCharged,PTP_VENDOR_NIKON),		

		
		PTP_VENDOR_VAL_BOOL(PTP_DPC_CANON_AssistLight,PTP_VENDOR_CANON),
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_CANON_RotationScene,PTP_VENDOR_CANON),
		PTP_VENDOR_VAL_RBOOL(PTP_DPC_CANON_BeepMode,PTP_VENDOR_CANON),
		PTP_VENDOR_VAL_BOOL(PTP_DPC_CANON_Beep,PTP_VENDOR_CANON),

		{PTP_DPC_CANON_RotationAngle, PTP_VENDOR_CANON, 0, "0'"},
		{PTP_DPC_CANON_RotationAngle, PTP_VENDOR_CANON, 3, "270'"},
		{PTP_DPC_CANON_RotationAngle, PTP_VENDOR_CANON, 1, "90'"},

		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 0, N_("Unknown")},
		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 1, N_("AC")},
		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 2, N_("Lithium Ion")},
		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 3, N_("Nickel hydride")},
		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 4, N_("Nickel cadmium")},
		{PTP_DPC_CANON_BatteryKind, PTP_VENDOR_CANON, 5, N_("Alkalium manganese")},

		{PTP_DPC_CANON_BatteryStatus, PTP_VENDOR_CANON, 0, N_("Undefined")},
		{PTP_DPC_CANON_BatteryStatus, PTP_VENDOR_CANON, 1, N_("Normal")},
		{PTP_DPC_CANON_BatteryStatus, PTP_VENDOR_CANON, 2, N_("Warning Level 1")},
		{PTP_DPC_CANON_BatteryStatus, PTP_VENDOR_CANON, 3, N_("Emergency")},
		{PTP_DPC_CANON_BatteryStatus, PTP_VENDOR_CANON, 4, N_("Warning Level 0")},

		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 0, N_("Undefined")},
		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 1, N_("Economy")},
		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 2, N_("Normal")},
		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 3, N_("Fine")},
		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 4, N_("Lossless")},
		{PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, 5, N_("SuperFine")},

		{PTP_DPC_CANON_FullViewFileFormat, PTP_VENDOR_CANON, 0, N_("Undefined")},
		{PTP_DPC_CANON_FullViewFileFormat, PTP_VENDOR_CANON, 1, N_("JPEG")},
		{PTP_DPC_CANON_FullViewFileFormat, PTP_VENDOR_CANON, 2, N_("CRW")},

		{PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, 0, N_("Large")},
		{PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, 1, N_("Medium 1")},
		{PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, 2, N_("Small")},
		{PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, 3, N_("Medium 2")},
		{PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, 7, N_("Medium 3")},

		{PTP_DPC_CANON_SelfTime, PTP_VENDOR_CANON, 0,   N_("Not used")},
		{PTP_DPC_CANON_SelfTime, PTP_VENDOR_CANON, 100, N_("10 seconds")},
		{PTP_DPC_CANON_SelfTime, PTP_VENDOR_CANON, 20,  N_("2 seconds")},

		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 0,  N_("Off")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 1,  N_("Auto")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 2,  N_("On")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 3,  N_("Red Eye Suppression")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 4,  N_("Low Speed Synchronization")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 5,  N_("Auto + Red Eye Suppression")},
		{PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, 6,  N_("On + Red Eye Suppression")},

		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 0,  N_("Auto")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 1,  N_("P")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 2,  N_("Tv")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 3,  N_("Av")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 4,  N_("M")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 5,  N_("A_DEP")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 6,  N_("M_DEP")},
		{PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, 7,  N_("Bulb")},
		

		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 0,  N_("Auto")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 1,  N_("Manual")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 2,  N_("Distant View")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 3,  N_("High-Speed Shutter")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 4,  N_("Low-Speed Shutter")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 5,  N_("Night View")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 6,  N_("Grayscale")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 7,  N_("Sepia")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 8,  N_("Portrait")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 9,  N_("Sports")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 10,  N_("Macro")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 11,  N_("Monochrome")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 12,  N_("Pan Focus")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 13,  N_("Neutral")},
		{PTP_DPC_CANON_ImageMode, PTP_VENDOR_CANON, 14,  N_("Soft")},

		{PTP_DPC_CANON_DriveMode, PTP_VENDOR_CANON, 0,  N_("Single-Frame Shooting")},
		{PTP_DPC_CANON_DriveMode, PTP_VENDOR_CANON, 1,  N_("Continuous Shooting")},
		{PTP_DPC_CANON_DriveMode, PTP_VENDOR_CANON, 2,  N_("Timer (Single) Shooting")},
		{PTP_DPC_CANON_DriveMode, PTP_VENDOR_CANON, 4,  N_("Continuous Low-speed Shooting")},
		{PTP_DPC_CANON_DriveMode, PTP_VENDOR_CANON, 5,  N_("Continuous High-speed Shooting")},

		{PTP_DPC_CANON_EZoom, PTP_VENDOR_CANON, 0,  N_("Off")},
		{PTP_DPC_CANON_EZoom, PTP_VENDOR_CANON, 1,  N_("2x")},
		{PTP_DPC_CANON_EZoom, PTP_VENDOR_CANON, 2,  N_("4x")},
		{PTP_DPC_CANON_EZoom, PTP_VENDOR_CANON, 3,  N_("Smooth")},

		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 0,  N_("Center-weighted Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 1,  N_("Spot Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 2,  N_("Average Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 3,  N_("Evaluative Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 4,  N_("Partial Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 5,  N_("Center-weighted Average Metering")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 6,  N_("Spot Metering Interlocked with AF Frame")},
		{PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, 7,  N_("Multi-Spot Metering")},

		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 0,  N_("Manual")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 1,  N_("Auto")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 2,  N_("Unknown")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 3,  N_("Zone Focus (Close-up)")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 4,  N_("Zone Focus (Very Close)")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 5,  N_("Zone Focus (Close)")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 6,  N_("Zone Focus (Medium)")},
		{PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, 7,  N_("Zone Focus (Far)")},

		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0,  N_("Invalid")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x1000,  N_("Focusing Point on Center Only, Manual")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x1001,  N_("Focusing Point on Center Only, Auto")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x3000,  N_("Multiple Focusing Points (No Specification), Manual")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x3001,  N_("Multiple Focusing Points, Auto")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x3002,  N_("Multiple Focusing Points (Right)")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x3003,  N_("Multiple Focusing Points (Center)")},
		{PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, 0x3004,  N_("Multiple Focusing Points (Left)")},

		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 0,  N_("Auto")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 1,  N_("Daylight")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 2,  N_("Cloudy")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 3,  N_("Tungsten")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 4,  N_("Fluorescent")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 6,  N_("Preset")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 7,  N_("Fluorescent H")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 9,  N_("Color Temperature")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 10,  N_("Custom Whitebalance PC-1")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 11,  N_("Custom Whitebalance PC-2")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 12,  N_("Custom Whitebalance PC-3")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 13,  N_("Missing Number")},
		{PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, 14,  N_("Fluorescent H")}, 

		{PTP_DPC_CANON_SlowShutterSetting, PTP_VENDOR_CANON, 0,  N_("Off")},
		{PTP_DPC_CANON_SlowShutterSetting, PTP_VENDOR_CANON, 1,  N_("Night View")},
		{PTP_DPC_CANON_SlowShutterSetting, PTP_VENDOR_CANON, 2,  N_("On")},
		{PTP_DPC_CANON_SlowShutterSetting, PTP_VENDOR_CANON, 3,  N_("Low-speed shutter function not available")},

		{PTP_DPC_CANON_AFMode, PTP_VENDOR_CANON, 0,  N_("Single Shot")},
		{PTP_DPC_CANON_AFMode, PTP_VENDOR_CANON, 1,  N_("AI Servo")},
		{PTP_DPC_CANON_AFMode, PTP_VENDOR_CANON, 2,  N_("AI Focus")},
		{PTP_DPC_CANON_AFMode, PTP_VENDOR_CANON, 3,  N_("Manual")},
		{PTP_DPC_CANON_AFMode, PTP_VENDOR_CANON, 4,  N_("Continuous")},

		PTP_VENDOR_VAL_BOOL(PTP_DPC_CANON_ImageStabilization,PTP_VENDOR_CANON),

		{PTP_DPC_CANON_Contrast, PTP_VENDOR_CANON, -2,  N_("Low 2")},
		{PTP_DPC_CANON_Contrast, PTP_VENDOR_CANON, -1,  N_("Low")},
		{PTP_DPC_CANON_Contrast, PTP_VENDOR_CANON, 0,  N_("Standard")},
		{PTP_DPC_CANON_Contrast, PTP_VENDOR_CANON, 1,  N_("High")},
		{PTP_DPC_CANON_Contrast, PTP_VENDOR_CANON, 2,  N_("High 2")},

		{PTP_DPC_CANON_ColorGain, PTP_VENDOR_CANON, -2,  N_("Low 2")},
		{PTP_DPC_CANON_ColorGain, PTP_VENDOR_CANON, -1,  N_("Low")},
		{PTP_DPC_CANON_ColorGain, PTP_VENDOR_CANON, 0,  N_("Standard")},
		{PTP_DPC_CANON_ColorGain, PTP_VENDOR_CANON, 1,  N_("High")},
		{PTP_DPC_CANON_ColorGain, PTP_VENDOR_CANON, 2,  N_("High 2")},

		{PTP_DPC_CANON_Sharpness, PTP_VENDOR_CANON, -2,  N_("Low 2")},
		{PTP_DPC_CANON_Sharpness, PTP_VENDOR_CANON, -1,  N_("Low")},
		{PTP_DPC_CANON_Sharpness, PTP_VENDOR_CANON, 0,  N_("Standard")},
		{PTP_DPC_CANON_Sharpness, PTP_VENDOR_CANON, 1,  N_("High")},
		{PTP_DPC_CANON_Sharpness, PTP_VENDOR_CANON, 2,  N_("High 2")},

		{PTP_DPC_CANON_Sensitivity, PTP_VENDOR_CANON, 0,  N_("Standard")},
		{PTP_DPC_CANON_Sensitivity, PTP_VENDOR_CANON, 1,  N_("Upper 1")},
		{PTP_DPC_CANON_Sensitivity, PTP_VENDOR_CANON, 2,  N_("Upper 2")},

		{PTP_DPC_CANON_ParameterSet, PTP_VENDOR_CANON, 0x08,  N_("Standard Development Parameters")},
		{PTP_DPC_CANON_ParameterSet, PTP_VENDOR_CANON, 0x10,  N_("Development Parameters 1")},
		{PTP_DPC_CANON_ParameterSet, PTP_VENDOR_CANON, 0x20,  N_("Development Parameters 2")},
		{PTP_DPC_CANON_ParameterSet, PTP_VENDOR_CANON, 0x40,  N_("Development Parameters 3")},

		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x00,  N_("Auto")},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x28,  "6"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x30,  "12"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x38,  "25"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x40,  "50"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x43,  "64"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x48,  "100"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x50,  "200"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x58,  "400"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x60,  "800"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x68,  "1600"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x70,  "3200"},
		{PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, 0x78,  "6400"},

		
		
		
		
		
		
		

		{PTP_DPC_CANON_CameraOutput, PTP_VENDOR_CANON, 0,  N_("Undefined")},
		{PTP_DPC_CANON_CameraOutput, PTP_VENDOR_CANON, 1,  N_("LCD")},
		{PTP_DPC_CANON_CameraOutput, PTP_VENDOR_CANON, 2,  N_("Video OUT")},
		{PTP_DPC_CANON_CameraOutput, PTP_VENDOR_CANON, 3,  N_("Off")},

		{PTP_DPC_CANON_MlSpotPos, PTP_VENDOR_CANON, 0, N_("MlSpotPosCenter")},
		{PTP_DPC_CANON_MlSpotPos, PTP_VENDOR_CANON, 1, N_("MlSpotPosAfLink")},

		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 0, N_("Off")},
		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 1, N_("Vivid")},
		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 2, N_("Neutral")},
		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 3, N_("Soft")},
		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 4, N_("Sepia")},
		{PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, 5, N_("Monochrome")},

		{0, 0, 0, NULL}
	};
	for (i=0; ptp_value_trans[i].dpc!=0; i++) {
		if ((ptp_value_trans[i].dpc == dpc) && 
			(((ptp_value_trans[i].dpc & 0xf000) == 0x5000) ||
		         (ptp_value_trans[i].vendor == params->deviceinfo.VendorExtensionID))
		) {
			double value = _value_to_num(&(dpd->CurrentValue), dpd->DataType);

			return snprintf(out, length, 
				_(ptp_value_trans[i].format),
				value * ptp_value_trans[i].coef +
				ptp_value_trans[i].bias);
		}
	}

	kval = _value_to_num(&(dpd->CurrentValue), dpd->DataType);
	for (i=0; ptp_value_list[i].dpc!=0; i++) {
		if ((ptp_value_list[i].dpc == dpc) && 
			(((ptp_value_list[i].dpc & 0xf000) == 0x5000) ||
		          (ptp_value_list[i].vendor == params->deviceinfo.VendorExtensionID)) &&
		    (ptp_value_list[i].key==kval)
		) {
			return snprintf(out, length, "%s", _(ptp_value_list[i].value));
		}
	}
	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_MICROSOFT
	    || params->deviceinfo.VendorExtensionID==PTP_VENDOR_MTP) {
		switch (dpc) {
		case PTP_DPC_MTP_SynchronizationPartner:
		case PTP_DPC_MTP_DeviceFriendlyName:
			return snprintf(out, length, "%s", dpd->CurrentValue.str);
		case PTP_DPC_MTP_SecureTime:
		case PTP_DPC_MTP_DeviceCertificate: {
			
			for (i=0;(i<dpd->CurrentValue.a.count) && (i<length);i++)
				out[i] = dpd->CurrentValue.a.v[i].u16;
			if (	dpd->CurrentValue.a.count &&
				(dpd->CurrentValue.a.count < length)) {
				out[dpd->CurrentValue.a.count-1] = 0;
				return dpd->CurrentValue.a.count-1;
			} else {
				out[length-1] = 0;
				return length;
			}
			break;
		}
		default:
			break;
		}
	}

	return 0;
}

struct {
	uint16_t ofc;
	const char *format;
} ptp_ofc_trans[] = {
	{PTP_OFC_Undefined,"Undefined Type"},
	{PTP_OFC_Defined,"Defined Type"},
	{PTP_OFC_Association,"Association/Directory"},
	{PTP_OFC_Script,"Script"},
	{PTP_OFC_Executable,"Executable"},
	{PTP_OFC_Text,"Text"},
	{PTP_OFC_HTML,"HTML"},
	{PTP_OFC_DPOF,"DPOF"},
	{PTP_OFC_AIFF,"AIFF"},
	{PTP_OFC_WAV,"MS Wave"},
	{PTP_OFC_MP3,"MP3"},
	{PTP_OFC_AVI,"MS AVI"},
	{PTP_OFC_MPEG,"MPEG"},
	{PTP_OFC_ASF,"ASF"},
	{PTP_OFC_QT,"Apple Quicktime"},
	{PTP_OFC_EXIF_JPEG,"JPEG"},
	{PTP_OFC_TIFF_EP,"TIFF EP"},
	{PTP_OFC_FlashPix,"FlashPix"},
	{PTP_OFC_BMP,"BMP"},
	{PTP_OFC_CIFF,"CIFF"},
	{PTP_OFC_GIF,"GIF"},
	{PTP_OFC_JFIF,"JFIF"},
	{PTP_OFC_PCD,"PCD"},
	{PTP_OFC_PICT,"PICT"},
	{PTP_OFC_PNG,"PNG"},
	{PTP_OFC_TIFF,"TIFF"},
	{PTP_OFC_TIFF_IT,"TIFF_IT"},
	{PTP_OFC_JP2,"JP2"},
	{PTP_OFC_JPX,"JPX"},
	{PTP_OFC_DNG,"DNG"},
};

struct {
	uint16_t ofc;
	const char *format;
} ptp_ofc_mtp_trans[] = {
	{PTP_OFC_MTP_MediaCard,N_("Media Card")},
	{PTP_OFC_MTP_MediaCardGroup,N_("Media Card Group")},
	{PTP_OFC_MTP_Encounter,N_("Encounter")},
	{PTP_OFC_MTP_EncounterBox,N_("Encounter Box")},
	{PTP_OFC_MTP_M4A,N_("M4A")},
	{PTP_OFC_MTP_Firmware,N_("Firmware")},
	{PTP_OFC_MTP_WindowsImageFormat,N_("Windows Image Format")},
	{PTP_OFC_MTP_UndefinedAudio,N_("Undefined Audio")},
	{PTP_OFC_MTP_WMA,"WMA"},
	{PTP_OFC_MTP_OGG,"OGG"},
	{PTP_OFC_MTP_AAC,"AAC"},
	{PTP_OFC_MTP_AudibleCodec,N_("Audible.com Codec")},
	{PTP_OFC_MTP_FLAC,"FLAC"},
	{PTP_OFC_MTP_UndefinedVideo,N_("Undefined Video")},
	{PTP_OFC_MTP_WMV,"WMV"},
	{PTP_OFC_MTP_MP4,"MP4"},
	{PTP_OFC_MTP_MP2,"MP2"},
	{PTP_OFC_MTP_3GP,"3GP"},
	{PTP_OFC_MTP_UndefinedCollection,N_("Undefined Collection")},
	{PTP_OFC_MTP_AbstractMultimediaAlbum,N_("Abstract Multimedia Album")},
	{PTP_OFC_MTP_AbstractImageAlbum,N_("Abstract Image Album")},
	{PTP_OFC_MTP_AbstractAudioAlbum,N_("Abstract Audio Album")},
	{PTP_OFC_MTP_AbstractVideoAlbum,N_("Abstract Video Album")},
	{PTP_OFC_MTP_AbstractAudioVideoPlaylist,N_("Abstract Audio Video Playlist")},
	{PTP_OFC_MTP_AbstractContactGroup,N_("Abstract Contact Group")},
	{PTP_OFC_MTP_AbstractMessageFolder,N_("Abstract Message Folder")},
	{PTP_OFC_MTP_AbstractChapteredProduction,N_("Abstract Chaptered Production")},
	{PTP_OFC_MTP_AbstractAudioPlaylist,N_("Abstract Audio Playlist")},
	{PTP_OFC_MTP_AbstractVideoPlaylist,N_("Abstract Video Playlist")},
	{PTP_OFC_MTP_AbstractMediacast,N_("Abstract Mediacast")},
	{PTP_OFC_MTP_WPLPlaylist,N_("WPL Playlist")},
	{PTP_OFC_MTP_M3UPlaylist,N_("M3U Playlist")},
	{PTP_OFC_MTP_MPLPlaylist,N_("MPL Playlist")},
	{PTP_OFC_MTP_ASXPlaylist,N_("ASX Playlist")},
	{PTP_OFC_MTP_PLSPlaylist,N_("PLS Playlist")},
	{PTP_OFC_MTP_UndefinedDocument,N_("Undefined Document")},
	{PTP_OFC_MTP_AbstractDocument,N_("Abstract Document")},
	{PTP_OFC_MTP_XMLDocument,N_("XMLDocument")},
	{PTP_OFC_MTP_MSWordDocument,N_("Microsoft Word Document")},
	{PTP_OFC_MTP_MHTCompiledHTMLDocument,N_("MHT Compiled HTML Document")},
	{PTP_OFC_MTP_MSExcelSpreadsheetXLS,N_("Microsoft Excel Spreadsheet (.xls)")},
	{PTP_OFC_MTP_MSPowerpointPresentationPPT,N_("Microsoft Powerpoint (.ppt)")},
	{PTP_OFC_MTP_UndefinedMessage,N_("Undefined Message")},
	{PTP_OFC_MTP_AbstractMessage,N_("Abstract Message")},
	{PTP_OFC_MTP_UndefinedContact,N_("Undefined Contact")},
	{PTP_OFC_MTP_AbstractContact,N_("Abstract Contact")},
	{PTP_OFC_MTP_vCard2,N_("vCard2")},
	{PTP_OFC_MTP_vCard3,N_("vCard3")},
	{PTP_OFC_MTP_UndefinedCalendarItem,N_("Undefined Calendar Item")},
	{PTP_OFC_MTP_AbstractCalendarItem,N_("Abstract Calendar Item")},
	{PTP_OFC_MTP_vCalendar1,N_("vCalendar1")},
	{PTP_OFC_MTP_vCalendar2,N_("vCalendar2")},
	{PTP_OFC_MTP_UndefinedWindowsExecutable,N_("Undefined Windows Executable")},
	{PTP_OFC_MTP_MediaCast,N_("Media Cast")},
	{PTP_OFC_MTP_Section,N_("Section")},
};

int
ptp_render_ofc(PTPParams* params, uint16_t ofc, int spaceleft, char *txt)
{
	int i;
	
	if (!(ofc & 0x8000)) {
		for (i=0;i<sizeof(ptp_ofc_trans)/sizeof(ptp_ofc_trans[0]);i++)
			if (ofc == ptp_ofc_trans[i].ofc)
				return snprintf(txt, spaceleft, "%s", _(ptp_ofc_trans[i].format));
	} else {
		switch (params->deviceinfo.VendorExtensionID) {
		case PTP_VENDOR_EASTMAN_KODAK:
			switch (ofc) {
			case PTP_OFC_EK_M3U:
				return snprintf (txt, spaceleft,"M3U");
			default:
				break;
			}
			break;
		case PTP_VENDOR_CANON:
			switch (ofc) {
			case PTP_OFC_CANON_CRW:
				return snprintf (txt, spaceleft,"CRW");
			default:
				break;
			}
			break;
		case PTP_VENDOR_MICROSOFT:
		case PTP_VENDOR_MTP:		  
			for (i=0;i<sizeof(ptp_ofc_mtp_trans)/sizeof(ptp_ofc_mtp_trans[0]);i++)
				if (ofc == ptp_ofc_mtp_trans[i].ofc)
					return snprintf(txt, spaceleft, "%s", _(ptp_ofc_mtp_trans[i].format));
			break;
		default:break;
		}
	}
	return snprintf (txt, spaceleft,_("Unknown(%04x)"), ofc);
}

struct {
	uint16_t opcode;
	const char *name;
} ptp_opcode_trans[] = {
	{PTP_OC_Undefined,N_("Undefined")},
	{PTP_OC_GetDeviceInfo,N_("get device info")},
	{PTP_OC_OpenSession,N_("Open session")},
	{PTP_OC_CloseSession,N_("Close session")},
	{PTP_OC_GetStorageIDs,N_("Get storage IDs")},
	{PTP_OC_GetStorageInfo,N_("Get storage info")},
	{PTP_OC_GetNumObjects,N_("Get number of objects")},
	{PTP_OC_GetObjectHandles,N_("Get object handles")},
	{PTP_OC_GetObjectInfo,N_("Get object info")},
	{PTP_OC_GetObject,N_("Get object")},
	{PTP_OC_GetThumb,N_("Get thumbnail")},
	{PTP_OC_DeleteObject,N_("Delete object")},
	{PTP_OC_SendObjectInfo,N_("Send object info")},
	{PTP_OC_SendObject,N_("Send object")},
	{PTP_OC_InitiateCapture,N_("Initiate capture")},
	{PTP_OC_FormatStore,N_("Format storage")},
	{PTP_OC_ResetDevice,N_("Reset device")},
	{PTP_OC_SelfTest,N_("Self test device")},
	{PTP_OC_SetObjectProtection,N_("Set object protection")},
	{PTP_OC_PowerDown,N_("Power down device")},
	{PTP_OC_GetDevicePropDesc,N_("Get device property description")},
	{PTP_OC_GetDevicePropValue,N_("Get device property value")},
	{PTP_OC_SetDevicePropValue,N_("Set device property value")},
	{PTP_OC_ResetDevicePropValue,N_("Reset device property value")},
	{PTP_OC_TerminateOpenCapture,N_("Terminate open capture")},
	{PTP_OC_MoveObject,N_("Move object")},
	{PTP_OC_CopyObject,N_("Copy object")},
	{PTP_OC_GetPartialObject,N_("Get partial object")},
	{PTP_OC_InitiateOpenCapture,N_("Initiate open capture")}
};

struct {
	uint16_t opcode;
	const char *name;
} ptp_opcode_mtp_trans[] = {
	{PTP_OC_MTP_GetObjectPropsSupported,N_("Get object properties supported")},
	{PTP_OC_MTP_GetObjectPropDesc,N_("Get object property description")},
	{PTP_OC_MTP_GetObjectPropValue,N_("Get object property value")},
	{PTP_OC_MTP_SetObjectPropValue,N_("Set object property value")},
	{PTP_OC_MTP_GetObjPropList,N_("Get object property list")},
	{PTP_OC_MTP_SetObjPropList,N_("Set object property list")},
	{PTP_OC_MTP_GetInterdependendPropdesc,N_("Get interdependent property description")},
	{PTP_OC_MTP_SendObjectPropList,N_("Send object property list")},
	{PTP_OC_MTP_GetObjectReferences,N_("Get object references")},
	{PTP_OC_MTP_SetObjectReferences,N_("Set object references")},
	{PTP_OC_MTP_UpdateDeviceFirmware,N_("Update device firmware")},
	{PTP_OC_MTP_Skip,N_("Skip to next position in playlist")},

	
	{PTP_OC_MTP_WMDRMPD_GetSecureTimeChallenge,N_("Get secure time challenge")},
	{PTP_OC_MTP_WMDRMPD_GetSecureTimeResponse,N_("Get secure time response")},
	{PTP_OC_MTP_WMDRMPD_SetLicenseResponse,N_("Set license response")},
	{PTP_OC_MTP_WMDRMPD_GetSyncList,N_("Get sync list")},
	{PTP_OC_MTP_WMDRMPD_SendMeterChallengeQuery,N_("Send meter challenge query")},
	{PTP_OC_MTP_WMDRMPD_GetMeterChallenge,N_("Get meter challenge")},
	{PTP_OC_MTP_WMDRMPD_SetMeterResponse,N_("Get meter response")},
	{PTP_OC_MTP_WMDRMPD_CleanDataStore,N_("Clean data store")},
	{PTP_OC_MTP_WMDRMPD_GetLicenseState,N_("Get license state")},
	{PTP_OC_MTP_WMDRMPD_SendWMDRMPDCommand,N_("Send WMDRM-PD Command")},
	{PTP_OC_MTP_WMDRMPD_SendWMDRMPDRequest,N_("Send WMDRM-PD Request")},

	
	{PTP_OC_MTP_WMPPD_ReportAddedDeletedItems,N_("Report Added/Deleted Items")},
	{PTP_OC_MTP_WMPPD_ReportAcquiredItems,N_("Report Acquired Items")},
	{PTP_OC_MTP_WMPPD_PlaylistObjectPref,N_("Get transferable playlist types")},

	
	{PTP_OC_MTP_WMDRMPD_SendWMDRMPDAppRequest,N_("Send WMDRM-PD Application Request")},
	{PTP_OC_MTP_WMDRMPD_GetWMDRMPDAppResponse,N_("Get WMDRM-PD Application Response")},
	{PTP_OC_MTP_WMDRMPD_EnableTrustedFilesOperations,N_("Enable trusted file operations")},
	{PTP_OC_MTP_WMDRMPD_DisableTrustedFilesOperations,N_("Disable trusted file operations")},
	{PTP_OC_MTP_WMDRMPD_EndTrustedAppSession,N_("End trusted application session")},

	
	{PTP_OC_MTP_AAVT_OpenMediaSession,N_("Open Media Session")},
	{PTP_OC_MTP_AAVT_CloseMediaSession,N_("Close Media Session")},
	{PTP_OC_MTP_AAVT_GetNextDataBlock,N_("Get Next Data Block")},
	{PTP_OC_MTP_AAVT_SetCurrentTimePosition,N_("Set Current Time Position")},

	
	{PTP_OC_MTP_WMDRMND_SendRegistrationRequest,N_("Send Registration Request")},
	{PTP_OC_MTP_WMDRMND_GetRegistrationResponse,N_("Get Registration Response")},
	{PTP_OC_MTP_WMDRMND_GetProximityChallenge,N_("Get Proximity Challenge")},
	{PTP_OC_MTP_WMDRMND_SendProximityResponse,N_("Send Proximity Response")},
	{PTP_OC_MTP_WMDRMND_SendWMDRMNDLicenseRequest,N_("Send WMDRM-ND License Request")},
	{PTP_OC_MTP_WMDRMND_GetWMDRMNDLicenseResponse,N_("Get WMDRM-ND License Response")},

	
	{PTP_OC_MTP_WPDWCN_ProcessWFCObject,N_("Process WFC Object")}
};

int
ptp_render_opcode(PTPParams* params, uint16_t opcode, int spaceleft, char *txt)
{
	int i;

	if (!(opcode & 0x8000)) {
		for (i=0;i<sizeof(ptp_opcode_trans)/sizeof(ptp_opcode_trans[0]);i++)
			if (opcode == ptp_opcode_trans[i].opcode)
				return snprintf(txt, spaceleft, "%s", _(ptp_opcode_trans[i].name));
	} else {
		switch (params->deviceinfo.VendorExtensionID) {
		case PTP_VENDOR_MICROSOFT:
		case PTP_VENDOR_MTP:
			for (i=0;i<sizeof(ptp_opcode_mtp_trans)/sizeof(ptp_opcode_mtp_trans[0]);i++)
				if (opcode == ptp_opcode_mtp_trans[i].opcode)
					return snprintf(txt, spaceleft, "%s", _(ptp_opcode_mtp_trans[i].name));
			break;
		default:break;
		}
	}
	return snprintf (txt, spaceleft,_("Unknown (%04x)"), opcode);
}


struct {
	uint16_t id;
	const char *name;
} ptp_opc_trans[] = {
	{PTP_OPC_StorageID,"StorageID"},
	{PTP_OPC_ObjectFormat,"ObjectFormat"},
	{PTP_OPC_ProtectionStatus,"ProtectionStatus"},
	{PTP_OPC_ObjectSize,"ObjectSize"},
	{PTP_OPC_AssociationType,"AssociationType"},
	{PTP_OPC_AssociationDesc,"AssociationDesc"},
	{PTP_OPC_ObjectFileName,"ObjectFileName"},
	{PTP_OPC_DateCreated,"DateCreated"},
	{PTP_OPC_DateModified,"DateModified"},
	{PTP_OPC_Keywords,"Keywords"},
	{PTP_OPC_ParentObject,"ParentObject"},
	{PTP_OPC_AllowedFolderContents,"AllowedFolderContents"},
	{PTP_OPC_Hidden,"Hidden"},
	{PTP_OPC_SystemObject,"SystemObject"},
	{PTP_OPC_PersistantUniqueObjectIdentifier,"PersistantUniqueObjectIdentifier"},
	{PTP_OPC_SyncID,"SyncID"},
	{PTP_OPC_PropertyBag,"PropertyBag"},
	{PTP_OPC_Name,"Name"},
	{PTP_OPC_CreatedBy,"CreatedBy"},
	{PTP_OPC_Artist,"Artist"},
	{PTP_OPC_DateAuthored,"DateAuthored"},
	{PTP_OPC_Description,"Description"},
	{PTP_OPC_URLReference,"URLReference"},
	{PTP_OPC_LanguageLocale,"LanguageLocale"},
	{PTP_OPC_CopyrightInformation,"CopyrightInformation"},
	{PTP_OPC_Source,"Source"},
	{PTP_OPC_OriginLocation,"OriginLocation"},
	{PTP_OPC_DateAdded,"DateAdded"},
	{PTP_OPC_NonConsumable,"NonConsumable"},
	{PTP_OPC_CorruptOrUnplayable,"CorruptOrUnplayable"},
	{PTP_OPC_ProducerSerialNumber,"ProducerSerialNumber"},
	{PTP_OPC_RepresentativeSampleFormat,"RepresentativeSampleFormat"},
	{PTP_OPC_RepresentativeSampleSize,"RepresentativeSampleSize"},
	{PTP_OPC_RepresentativeSampleHeight,"RepresentativeSampleHeight"},
	{PTP_OPC_RepresentativeSampleWidth,"RepresentativeSampleWidth"},
	{PTP_OPC_RepresentativeSampleDuration,"RepresentativeSampleDuration"},
	{PTP_OPC_RepresentativeSampleData,"RepresentativeSampleData"},
	{PTP_OPC_Width,"Width"},
	{PTP_OPC_Height,"Height"},
	{PTP_OPC_Duration,"Duration"},
	{PTP_OPC_Rating,"Rating"},
	{PTP_OPC_Track,"Track"},
	{PTP_OPC_Genre,"Genre"},
	{PTP_OPC_Credits,"Credits"},
	{PTP_OPC_Lyrics,"Lyrics"},
	{PTP_OPC_SubscriptionContentID,"SubscriptionContentID"},
	{PTP_OPC_ProducedBy,"ProducedBy"},
	{PTP_OPC_UseCount,"UseCount"},
	{PTP_OPC_SkipCount,"SkipCount"},
	{PTP_OPC_LastAccessed,"LastAccessed"},
	{PTP_OPC_ParentalRating,"ParentalRating"},
	{PTP_OPC_MetaGenre,"MetaGenre"},
	{PTP_OPC_Composer,"Composer"},
	{PTP_OPC_EffectiveRating,"EffectiveRating"},
	{PTP_OPC_Subtitle,"Subtitle"},
	{PTP_OPC_OriginalReleaseDate,"OriginalReleaseDate"},
	{PTP_OPC_AlbumName,"AlbumName"},
	{PTP_OPC_AlbumArtist,"AlbumArtist"},
	{PTP_OPC_Mood,"Mood"},
	{PTP_OPC_DRMStatus,"DRMStatus"},
	{PTP_OPC_SubDescription,"SubDescription"},
	{PTP_OPC_IsCropped,"IsCropped"},
	{PTP_OPC_IsColorCorrected,"IsColorCorrected"},
	{PTP_OPC_ImageBitDepth,"ImageBitDepth"},
	{PTP_OPC_Fnumber,"Fnumber"},
	{PTP_OPC_ExposureTime,"ExposureTime"},
	{PTP_OPC_ExposureIndex,"ExposureIndex"},
	{PTP_OPC_DisplayName,"DisplayName"},
	{PTP_OPC_BodyText,"BodyText"},
	{PTP_OPC_Subject,"Subject"},
	{PTP_OPC_Priority,"Priority"},
	{PTP_OPC_GivenName,"GivenName"},
	{PTP_OPC_MiddleNames,"MiddleNames"},
	{PTP_OPC_FamilyName,"FamilyName"},

	{PTP_OPC_Prefix,"Prefix"},
	{PTP_OPC_Suffix,"Suffix"},
	{PTP_OPC_PhoneticGivenName,"PhoneticGivenName"},
	{PTP_OPC_PhoneticFamilyName,"PhoneticFamilyName"},
	{PTP_OPC_EmailPrimary,"EmailPrimary"},
	{PTP_OPC_EmailPersonal1,"EmailPersonal1"},
	{PTP_OPC_EmailPersonal2,"EmailPersonal2"},
	{PTP_OPC_EmailBusiness1,"EmailBusiness1"},
	{PTP_OPC_EmailBusiness2,"EmailBusiness2"},
	{PTP_OPC_EmailOthers,"EmailOthers"},
	{PTP_OPC_PhoneNumberPrimary,"PhoneNumberPrimary"},
	{PTP_OPC_PhoneNumberPersonal,"PhoneNumberPersonal"},
	{PTP_OPC_PhoneNumberPersonal2,"PhoneNumberPersonal2"},
	{PTP_OPC_PhoneNumberBusiness,"PhoneNumberBusiness"},
	{PTP_OPC_PhoneNumberBusiness2,"PhoneNumberBusiness2"},
	{PTP_OPC_PhoneNumberMobile,"PhoneNumberMobile"},
	{PTP_OPC_PhoneNumberMobile2,"PhoneNumberMobile2"},
	{PTP_OPC_FaxNumberPrimary,"FaxNumberPrimary"},
	{PTP_OPC_FaxNumberPersonal,"FaxNumberPersonal"},
	{PTP_OPC_FaxNumberBusiness,"FaxNumberBusiness"},
	{PTP_OPC_PagerNumber,"PagerNumber"},
	{PTP_OPC_PhoneNumberOthers,"PhoneNumberOthers"},
	{PTP_OPC_PrimaryWebAddress,"PrimaryWebAddress"},
	{PTP_OPC_PersonalWebAddress,"PersonalWebAddress"},
	{PTP_OPC_BusinessWebAddress,"BusinessWebAddress"},
	{PTP_OPC_InstantMessengerAddress,"InstantMessengerAddress"},
	{PTP_OPC_InstantMessengerAddress2,"InstantMessengerAddress2"},
	{PTP_OPC_InstantMessengerAddress3,"InstantMessengerAddress3"},
	{PTP_OPC_PostalAddressPersonalFull,"PostalAddressPersonalFull"},
	{PTP_OPC_PostalAddressPersonalFullLine1,"PostalAddressPersonalFullLine1"},
	{PTP_OPC_PostalAddressPersonalFullLine2,"PostalAddressPersonalFullLine2"},
	{PTP_OPC_PostalAddressPersonalFullCity,"PostalAddressPersonalFullCity"},
	{PTP_OPC_PostalAddressPersonalFullRegion,"PostalAddressPersonalFullRegion"},
	{PTP_OPC_PostalAddressPersonalFullPostalCode,"PostalAddressPersonalFullPostalCode"},
	{PTP_OPC_PostalAddressPersonalFullCountry,"PostalAddressPersonalFullCountry"},
	{PTP_OPC_PostalAddressBusinessFull,"PostalAddressBusinessFull"},
	{PTP_OPC_PostalAddressBusinessLine1,"PostalAddressBusinessLine1"},
	{PTP_OPC_PostalAddressBusinessLine2,"PostalAddressBusinessLine2"},
	{PTP_OPC_PostalAddressBusinessCity,"PostalAddressBusinessCity"},
	{PTP_OPC_PostalAddressBusinessRegion,"PostalAddressBusinessRegion"},
	{PTP_OPC_PostalAddressBusinessPostalCode,"PostalAddressBusinessPostalCode"},
	{PTP_OPC_PostalAddressBusinessCountry,"PostalAddressBusinessCountry"},
	{PTP_OPC_PostalAddressOtherFull,"PostalAddressOtherFull"},
	{PTP_OPC_PostalAddressOtherLine1,"PostalAddressOtherLine1"},
	{PTP_OPC_PostalAddressOtherLine2,"PostalAddressOtherLine2"},
	{PTP_OPC_PostalAddressOtherCity,"PostalAddressOtherCity"},
	{PTP_OPC_PostalAddressOtherRegion,"PostalAddressOtherRegion"},
	{PTP_OPC_PostalAddressOtherPostalCode,"PostalAddressOtherPostalCode"},
	{PTP_OPC_PostalAddressOtherCountry,"PostalAddressOtherCountry"},
	{PTP_OPC_OrganizationName,"OrganizationName"},
	{PTP_OPC_PhoneticOrganizationName,"PhoneticOrganizationName"},
	{PTP_OPC_Role,"Role"},
	{PTP_OPC_Birthdate,"Birthdate"},
	{PTP_OPC_MessageTo,"MessageTo"},
	{PTP_OPC_MessageCC,"MessageCC"},
	{PTP_OPC_MessageBCC,"MessageBCC"},
	{PTP_OPC_MessageRead,"MessageRead"},
	{PTP_OPC_MessageReceivedTime,"MessageReceivedTime"},
	{PTP_OPC_MessageSender,"MessageSender"},
	{PTP_OPC_ActivityBeginTime,"ActivityBeginTime"},
	{PTP_OPC_ActivityEndTime,"ActivityEndTime"},
	{PTP_OPC_ActivityLocation,"ActivityLocation"},
	{PTP_OPC_ActivityRequiredAttendees,"ActivityRequiredAttendees"},
	{PTP_OPC_ActivityOptionalAttendees,"ActivityOptionalAttendees"},
	{PTP_OPC_ActivityResources,"ActivityResources"},
	{PTP_OPC_ActivityAccepted,"ActivityAccepted"},
	{PTP_OPC_Owner,"Owner"},
	{PTP_OPC_Editor,"Editor"},
	{PTP_OPC_Webmaster,"Webmaster"},
	{PTP_OPC_URLSource,"URLSource"},
	{PTP_OPC_URLDestination,"URLDestination"},
	{PTP_OPC_TimeBookmark,"TimeBookmark"},
	{PTP_OPC_ObjectBookmark,"ObjectBookmark"},
	{PTP_OPC_ByteBookmark,"ByteBookmark"},
	{PTP_OPC_LastBuildDate,"LastBuildDate"},
	{PTP_OPC_TimetoLive,"TimetoLive"},
	{PTP_OPC_MediaGUID,"MediaGUID"},
	{PTP_OPC_TotalBitRate,"TotalBitRate"},
	{PTP_OPC_BitRateType,"BitRateType"},
	{PTP_OPC_SampleRate,"SampleRate"},
	{PTP_OPC_NumberOfChannels,"NumberOfChannels"},
	{PTP_OPC_AudioBitDepth,"AudioBitDepth"},
	{PTP_OPC_ScanDepth,"ScanDepth"},
	{PTP_OPC_AudioWAVECodec,"AudioWAVECodec"},
	{PTP_OPC_AudioBitRate,"AudioBitRate"},
	{PTP_OPC_VideoFourCCCodec,"VideoFourCCCodec"},
	{PTP_OPC_VideoBitRate,"VideoBitRate"},
	{PTP_OPC_FramesPerThousandSeconds,"FramesPerThousandSeconds"},
	{PTP_OPC_KeyFrameDistance,"KeyFrameDistance"},
	{PTP_OPC_BufferSize,"BufferSize"},
	{PTP_OPC_EncodingQuality,"EncodingQuality"},
	{PTP_OPC_EncodingProfile,"EncodingProfile"},
	{PTP_OPC_BuyFlag,"BuyFlag"},
};

int
ptp_render_mtp_propname(uint16_t propid, int spaceleft, char *txt) {
	int i;
	for (i=0;i<sizeof(ptp_opc_trans)/sizeof(ptp_opc_trans[0]);i++)
		if (propid == ptp_opc_trans[i].id)
			return snprintf(txt, spaceleft, "%s", ptp_opc_trans[i].name);
	return snprintf (txt, spaceleft,"unknown(%04x)", propid);
}

MTPProperties *
ptp_get_new_object_prop_entry(MTPProperties **props, int *nrofprops) {
	MTPProperties *newprops;
	MTPProperties *prop;

	if (*props == NULL) {
		newprops = malloc(sizeof(MTPProperties)*(*nrofprops+1));
	} else {
		newprops = realloc(*props,sizeof(MTPProperties)*(*nrofprops+1));
	}
	if (newprops == NULL)
		return NULL;
	prop = &newprops[*nrofprops];
	prop->property = PTP_OPC_StorageID; 
	prop->datatype = PTP_DTC_UNDEF;
	prop->ObjectHandle = 0x00000000U;
	prop->propval.str = NULL;
	
	(*props) = newprops;
	(*nrofprops)++;
	return prop;
}

void 
ptp_destroy_object_prop(MTPProperties *prop)
{
  if (!prop)
    return;
  
  if (prop->datatype == PTP_DTC_STR && prop->propval.str != NULL)
    free(prop->propval.str);
  else if ((prop->datatype == PTP_DTC_AINT8 || prop->datatype == PTP_DTC_AINT16 ||
            prop->datatype == PTP_DTC_AINT32 || prop->datatype == PTP_DTC_AINT64 || prop->datatype == PTP_DTC_AINT128 ||
            prop->datatype == PTP_DTC_AUINT8 || prop->datatype == PTP_DTC_AUINT16 ||
            prop->datatype == PTP_DTC_AUINT32 || prop->datatype == PTP_DTC_AUINT64 || prop->datatype ==  PTP_DTC_AUINT128)
            && prop->propval.a.v != NULL)
    free(prop->propval.a.v);
}

void 
ptp_destroy_object_prop_list(MTPProperties *props, int nrofprops)
{
  int i;
  MTPProperties *prop = props;

  for (i=0;i<nrofprops;i++,prop++)
    ptp_destroy_object_prop(prop);
  free(props);
}

MTPProperties *
ptp_find_object_prop_in_cache(PTPParams *params, uint32_t const handle, uint32_t const attribute_id)
{
	int	i;
	MTPProperties	*prop;
	PTPObject	*ob;
	uint16_t	ret;

	ret = ptp_object_find (params, handle, &ob);
	if (ret != PTP_RC_OK)
		return NULL;
	prop = ob->mtpprops;
	for (i=0;i<ob->nrofmtpprops;i++) {
		if (attribute_id == prop->property)
			return prop;
		prop++;
	}
	return NULL;
}

void
ptp_remove_object_from_cache(PTPParams *params, uint32_t handle)
{
	int i;
	PTPObject	*ob;
	uint16_t	ret;

	ret = ptp_object_find (params, handle, &ob);
	if (ret != PTP_RC_OK)
		return;
	i = ob-params->objects;
	
	ptp_free_object (ob);

	if (i < params->nrofobjects-1)
		memmove (ob,ob+1,(params->nrofobjects-1-i)*sizeof(PTPObject));
	params->nrofobjects--;
	
	params->objects = realloc(params->objects, sizeof(PTPObject)*params->nrofobjects);
}

static int _cmp_ob (const void *a, const void *b) {
	PTPObject *oa = (PTPObject*)a;
	PTPObject *ob = (PTPObject*)b;

	return oa->oid - ob->oid;
}
	
void
ptp_objects_sort (PTPParams *params) {
	qsort (params->objects, params->nrofobjects, sizeof(PTPObject), _cmp_ob);
}

uint16_t
ptp_object_find (PTPParams *params, uint32_t handle, PTPObject **retob) {
	PTPObject	tmpob;

	tmpob.oid = handle;
	*retob = bsearch (&tmpob, params->objects, params->nrofobjects, sizeof(tmpob), _cmp_ob);
	if (!*retob)
		return PTP_RC_GeneralError;
	return PTP_RC_OK;
}

uint16_t
ptp_object_find_or_insert (PTPParams *params, uint32_t handle, PTPObject **retob) {
	int 		begin, end, cursor;
	int		insertat;
	PTPObject	*newobs;

	if (!handle) return PTP_RC_GeneralError;
	*retob = NULL;
	if (!params->nrofobjects) {
		params->objects = calloc(1,sizeof(PTPObject));
		params->nrofobjects = 1;
		params->objects[0].oid = handle;
		*retob = &params->objects[0];
		return PTP_RC_OK;
	}
	begin = 0;
	end = params->nrofobjects-1;
	
	while (1) {
		cursor = (end-begin)/2+begin;
		
		if (params->objects[cursor].oid == handle) {
			*retob = &params->objects[cursor];
			return PTP_RC_OK;
		}
		if (params->objects[cursor].oid < handle)
			begin = cursor;
		else
			end = cursor;
		if ((end - begin) <= 1)
			break;
	}
	if (params->objects[begin].oid == handle) {
		*retob = &params->objects[begin];
		return PTP_RC_OK;
	}
	if (params->objects[end].oid == handle) {
		*retob = &params->objects[end];
		return PTP_RC_OK;
	}
	if ((begin == 0) && (handle < params->objects[0].oid)) {
		insertat=begin;
	} else {
		if ((end == params->nrofobjects-1) && (handle > params->objects[end].oid))
			insertat=end+1;
		else
			insertat=begin+1;
	}
	
	newobs = realloc (params->objects, sizeof(PTPObject)*(params->nrofobjects+1));
	if (!newobs) return PTP_RC_GeneralError;
	params->objects = newobs;
	if (insertat<=params->nrofobjects)
		memmove (&params->objects[insertat+1],&params->objects[insertat],(params->nrofobjects-insertat)*sizeof(PTPObject));
	memset(&params->objects[insertat],0,sizeof(PTPObject));
	params->objects[insertat].oid = handle;
	*retob = &params->objects[insertat];
	params->nrofobjects++;
	return PTP_RC_OK;
}

uint16_t
ptp_object_want (PTPParams *params, uint32_t handle, int want, PTPObject **retob) {
	uint16_t	ret;
	PTPObject	*ob;
	

	*retob = NULL;
	if (!handle) {
		ptp_debug (params, "ptp_object_want: querying handle 0?\n");
		return PTP_RC_GeneralError;
	}
	ret = ptp_object_find_or_insert (params, handle, &ob);
	if (ret != PTP_RC_OK)
		return PTP_RC_GeneralError;
	*retob = ob;
	
	if ((ob->flags & want) == want)
		return PTP_RC_OK;

#define X (PTPOBJECT_OBJECTINFO_LOADED|PTPOBJECT_STORAGEID_LOADED|PTPOBJECT_PARENTOBJECT_LOADED)
	if ((want & X) && ((ob->flags & X) != X)) {
		uint32_t	saveparent = 0;
		
		
		if (ob->flags & PTPOBJECT_PARENTOBJECT_LOADED)
			saveparent = ob->oi.ParentObject;

		ret = ptp_getobjectinfo (params, handle, &ob->oi);
		if (ret != PTP_RC_OK)
			return ret;
		if (!ob->oi.Filename) ob->oi.Filename=strdup("<none>");
		if (ob->flags & PTPOBJECT_PARENTOBJECT_LOADED)
			ob->oi.ParentObject = saveparent;

		
		if (ob->oi.ParentObject == handle)
			ob->oi.ParentObject = 0;
		ob->flags |= X;

		
	}
#undef X
	if (	(want & PTPOBJECT_MTPPROPLIST_LOADED) &&
		(!(ob->flags & PTPOBJECT_MTPPROPLIST_LOADED))
	) {
		int		nrofprops = 0;
		MTPProperties 	*props = NULL;

		if (params->device_flags & DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST) {
			want &= ~PTPOBJECT_MTPPROPLIST_LOADED;
			goto fallback;
		}

		
		if (!ptp_operation_issupported(params,PTP_OC_MTP_GetObjPropList)) {
			want &= ~PTPOBJECT_MTPPROPLIST_LOADED;
			goto fallback;
		}

		ptp_debug (params, "ptp2/mtpfast: reading mtp proplist of %08x", handle);
		ret = ptp_mtp_getobjectproplist (params, handle, &props, &nrofprops);
		if (ret != PTP_RC_OK)
			goto fallback;
		ob->mtpprops = props;
		ob->nrofmtpprops = nrofprops;

#if 0
		MTPProperties 	*xpl;
		int j;
		PTPObjectInfo	oinfo;	

		memset (&oinfo,0,sizeof(oinfo));
		
		for (j=0;j<nrofprops;j++) {
			xpl = &props[j];
			switch (xpl->property) {
			case PTP_OPC_ParentObject:
				if (xpl->datatype != PTP_DTC_UINT32) {
					ptp_debug (params, "ptp2/mtpfast: parentobject has type 0x%x???", xpl->datatype);
					break;
				}
				oinfo.ParentObject = xpl->propval.u32;
				ptp_debug (params, "ptp2/mtpfast: parent 0x%x", xpl->propval.u32);
				break;
			case PTP_OPC_ObjectFormat:
				if (xpl->datatype != PTP_DTC_UINT16) {
					ptp_debug (params, "ptp2/mtpfast: objectformat has type 0x%x???", xpl->datatype);
					break;
				}
				oinfo.ObjectFormat = xpl->propval.u16;
				ptp_debug (params, "ptp2/mtpfast: ofc 0x%x", xpl->propval.u16);
				break;
			case PTP_OPC_ObjectSize:
				switch (xpl->datatype) {
				case PTP_DTC_UINT32:
					oinfo.ObjectCompressedSize = xpl->propval.u32;
					break;
				case PTP_DTC_UINT64:
					oinfo.ObjectCompressedSize = xpl->propval.u64;
					break;
				default:
					ptp_debug (params, "ptp2/mtpfast: objectsize has type 0x%x???", xpl->datatype);
					break;
				}
				ptp_debug (params, "ptp2/mtpfast: objectsize %u", xpl->propval.u32);
				break;
			case PTP_OPC_StorageID:
				if (xpl->datatype != PTP_DTC_UINT32) {
					ptp_debug (params, "ptp2/mtpfast: storageid has type 0x%x???", xpl->datatype);
					break;
				}
				oinfo.StorageID = xpl->propval.u32;
				ptp_debug (params, "ptp2/mtpfast: storageid 0x%x", xpl->propval.u32);
				break;
			case PTP_OPC_ProtectionStatus:
				if (xpl->datatype != PTP_DTC_UINT16) {
					ptp_debug (params, "ptp2/mtpfast: protectionstatus has type 0x%x???", xpl->datatype);
					break;
				}
				oinfo.ProtectionStatus = xpl->propval.u16;
				ptp_debug (params, "ptp2/mtpfast: protection 0x%x", xpl->propval.u16);
				break;
			case PTP_OPC_ObjectFileName:
				if (xpl->datatype != PTP_DTC_STR) {
					ptp_debug (params, "ptp2/mtpfast: filename has type 0x%x???", xpl->datatype);
					break;
				}
				if (xpl->propval.str) {
					ptp_debug (params, "ptp2/mtpfast: filename %s", xpl->propval.str);
					oinfo.Filename = strdup(xpl->propval.str);
				} else {
					oinfo.Filename = NULL;
				}
				break;
			case PTP_OPC_DateCreated:
				if (xpl->datatype != PTP_DTC_STR) {
					ptp_debug (params, "ptp2/mtpfast: datecreated has type 0x%x???", xpl->datatype);
					break;
				}
				ptp_debug (params, "ptp2/mtpfast: capturedate %s", xpl->propval.str);
				oinfo.CaptureDate = ptp_unpack_PTPTIME (xpl->propval.str);
				break;
			case PTP_OPC_DateModified:
				if (xpl->datatype != PTP_DTC_STR) {
					ptp_debug (params, "ptp2/mtpfast: datemodified has type 0x%x???", xpl->datatype);
					break;
				}
				ptp_debug (params, "ptp2/mtpfast: moddate %s", xpl->propval.str);
				oinfo.ModificationDate = ptp_unpack_PTPTIME (xpl->propval.str);
				break;
			default:
				if ((xpl->property & 0xfff0) == 0xdc00)
					ptp_debug (params, "ptp2/mtpfast:case %x type %x unhandled", xpl->property, xpl->datatype);
				break;
			}
		}
		if (!oinfo.Filename)
			
			oinfo.Filename = strdup("<null>");
#endif
		ob->flags |= PTPOBJECT_MTPPROPLIST_LOADED;
fallback:	;
	}
	if ((ob->flags & want) == want)
		return PTP_RC_OK;
	ptp_debug (params, "ptp_object_want: oid 0x%08x, want flags %x, have only %x?", handle, want, ob->flags);
	return PTP_RC_GeneralError;
}


uint16_t
ptp_add_object_to_cache(PTPParams *params, uint32_t handle)
{
	PTPObject *ob;
	return ptp_object_want (params, handle, PTPOBJECT_OBJECTINFO_LOADED|PTPOBJECT_MTPPROPLIST_LOADED, &ob);
}

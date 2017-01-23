/*
 * \file libusb-glue.c
 * Low-level USB interface glue towards libusb.
 *
 * Copyright (C) 2005-2007 Richard A. Low <richard@wentnet.com>
 * Copyright (C) 2005-2008 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2006-2007 Marcus Meissner
 * Copyright (C) 2007 Ted Bullock
 * Copyright (C) 2008 Chris Bagwell <chris@cnpbagwell.com>
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
 *
 * Created by Richard Low on 24/12/2005. (as mtp-utils.c)
 * Modified by Linus Walleij 2006-03-06
 *  (Notice that Anglo-Saxons use little-endian dates and Swedes
 *   use big-endian dates.)
 *
 */
#include "config.h"
#include "libmtp.h"
#include "libusb-glue.h"
#include "device-flags.h"
#include "util.h"
#include "ptp.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usb.h>

#include "ptp-pack.c"

#ifndef USB_CLASS_PTP
#define USB_CLASS_PTP 6
#endif

#ifndef USB_CLASS_MISC
#define USB_CLASS_MISC 0xEF
#endif

#define APPLE_VID 0x05ac


#define USB_TIMEOUT_DEFAULT     10000

#ifndef USB_DP_HTD
#define USB_DP_HTD		(0x00 << 7)	
#endif
#ifndef USB_DP_DTH
#define USB_DP_DTH		(0x01 << 7)	
#endif

#ifndef USB_FEATURE_HALT
#define USB_FEATURE_HALT	0x00
#endif

struct mtpdevice_list_struct {
  struct usb_device *libusb_device;
  PTPParams *params;
  PTP_USB *ptp_usb;
  uint32_t bus_location;
  struct mtpdevice_list_struct *next;
};
typedef struct mtpdevice_list_struct mtpdevice_list_t;

static const LIBMTP_device_entry_t mtp_device_table[] = {
#include "music-players.h"
};
static const int mtp_device_table_size = sizeof(mtp_device_table) / sizeof(LIBMTP_device_entry_t);

static struct usb_bus* init_usb();
static void close_usb(PTP_USB* ptp_usb);
static void find_interface_and_endpoints(struct usb_device *dev,
					 uint8_t *interface,
					 int* inep, 
					 int* inep_maxpacket, 
					 int* outep, 
					 int* outep_maxpacket, 
					 int* intep);
static void clear_stall(PTP_USB* ptp_usb);
static int init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev);
static short ptp_write_func (unsigned long,PTPDataHandler*,void *data,unsigned long*);
static short ptp_read_func (unsigned long,PTPDataHandler*,void *data,unsigned long*,int);
static int usb_clear_stall_feature(PTP_USB* ptp_usb, int ep);
static int usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status);

int LIBMTP_Get_Supported_Devices_List(LIBMTP_device_entry_t ** const devices, int * const numdevs)
{
  *devices = (LIBMTP_device_entry_t *) &mtp_device_table;
  *numdevs = mtp_device_table_size;
  return 0;
}


static struct usb_bus* init_usb()
{
  usb_init();
  usb_find_busses();
  usb_find_devices();
  return (usb_get_busses());
}

static mtpdevice_list_t *append_to_mtpdevice_list(mtpdevice_list_t *devlist,
						  struct usb_device *newdevice,
						  uint32_t bus_location)
{
  mtpdevice_list_t *new_list_entry;
  
  new_list_entry = (mtpdevice_list_t *) malloc(sizeof(mtpdevice_list_t));
  if (new_list_entry == NULL) {
    return NULL;
  }
  
  new_list_entry->libusb_device = newdevice;
  new_list_entry->bus_location = bus_location;
  new_list_entry->next = NULL;
  
  if (devlist == NULL) {
    return new_list_entry;
  } else {
    mtpdevice_list_t *tmp = devlist;
    while (tmp->next != NULL) {
      tmp = tmp->next;
    }
    tmp->next = new_list_entry;
  }
  return devlist;
}

static void free_mtpdevice_list(mtpdevice_list_t *devlist)
{
  mtpdevice_list_t *tmplist = devlist;

  if (devlist == NULL)
    return;
  while (tmplist != NULL) {
    mtpdevice_list_t *tmp = tmplist;
    tmplist = tmplist->next;
    
    free(tmp);
  }
  return;
}

#define MILD_MTP_PROBING

#ifdef MILD_MTP_PROBING
static int probe_device_descriptor(struct usb_device *dev, FILE *dumpfile)
{
  usb_dev_handle *devh;
  unsigned char buf[1024];
  int i;
  int ret;

  if (!(dev->descriptor.bDeviceClass == USB_CLASS_PER_INTERFACE ||
	    dev->descriptor.bDeviceClass == USB_CLASS_PTP ||
	    dev->descriptor.bDeviceClass == USB_CLASS_VENDOR_SPEC) ||
      
      dev->descriptor.idVendor == APPLE_VID) {
    return 0;
  }

  
  devh = usb_open(dev);
  if (devh == NULL) {
    
    return 0;
  }

  if (dev->config) {

    for (i = 0; i < dev->descriptor.bNumConfigurations; i++) {
      uint8_t j;

      for (j = 0; j < dev->config[i].bNumInterfaces; j++) {
        int k;
        for (k = 0; k < dev->config[i].interface[j].num_altsetting; k++) {
	  
	  struct usb_interface_descriptor *intf =
	    &dev->config[i].interface[j].altsetting[k];

          buf[0] = '\0';
          ret = usb_get_string_simple(devh,
				      dev->config[i].interface[j].altsetting[k].iInterface,
				      (char *) buf,
				      1024);

	  if (ret < 3)
	    continue;
          if (strcmp((char *) buf, "MTP") == 0) {
	    if (dumpfile != NULL) {
              fprintf(dumpfile, "Configuration %d, interface %d, altsetting %d:\n", i, j, k);
	      fprintf(dumpfile, "   Interface description contains the string \"MTP\"\n");
	      fprintf(dumpfile, "   Device recognized as MTP, no further probing.\n");
	    }
            usb_close(devh);
            return 1;
          }
       }
      }
    }
  }

  usb_close(devh);
  return 0;
}

#else 
static int probe_device_descriptor(struct usb_device *dev, FILE *dumpfile)
{
  usb_dev_handle *devh;
  unsigned char buf[1024], cmd;
  int i;
  int ret;
  
  
  if (dev->descriptor.bDeviceClass == USB_CLASS_HUB) {
    return 0;
  }
  
  
  devh = usb_open(dev);
  if (devh == NULL) {
    
    return 0;
  }

  if (dev->config) {
    for (i = 0; i < dev->descriptor.bNumConfigurations; i++) {
      uint8_t j;

      for (j = 0; j < dev->config[i].bNumInterfaces; j++) {
        int k;
        for (k = 0; k < dev->config[i].interface[j].num_altsetting; k++) {
	  
	  struct usb_interface_descriptor *intf =
	    &dev->config[i].interface[j].altsetting[k];


          buf[0] = '\0';
          ret = usb_get_string_simple(devh,
				      dev->config[i].interface[j].altsetting[k].iInterface,
				      (char *) buf,
				      1024);
	  if (ret < 3)
	    continue;
          if (strcmp((char *) buf, "MTP") == 0) {
	    if (dumpfile != NULL) {
              fprintf(dumpfile, "Configuration %d, interface %d, altsetting %d:\n", i, j, k);
	      fprintf(dumpfile, "   Interface description contains the string \"MTP\"\n");
	      fprintf(dumpfile, "   Device recognized as MTP, no further probing.\n");
	    }
            usb_close(devh);
            return 1;
          }
  #ifdef LIBUSB_HAS_GET_DRIVER_NP
	  {
	    char devname[0x10];

	    devname[0] = '\0';
	    ret = usb_get_driver_np(devh,
				    dev->config[i].interface[j].altsetting[k].iInterface,
				    devname,
				    sizeof(devname));
	    if (devname[0] != '\0' && strcmp(devname, "usb-storage")) {
	      printf("avoid probing device using kernel interface \"%s\"\n", devname);
	      return 0;
	    }
	  }
  #endif
        }
      }
    }
  } else {
    if (dev->descriptor.bNumConfigurations)
      printf("dev->config is NULL in probe_device_descriptor yet dev->descriptor.bNumConfigurations > 0\n");
  }
  
  
  ret = usb_get_descriptor(devh, 0x03, 0xee, buf, sizeof(buf));

  
  if (dumpfile != NULL && ret > 0) {
    fprintf(dumpfile, "Microsoft device descriptor 0xee:\n");
    data_dump_ascii(dumpfile, buf, ret, 16);
  }
  
  
  if (ret < 10) {
    usb_close(devh);
    return 0;
  }
      
  
  if (!((buf[2] == 'M') && (buf[4] == 'S') &&
	(buf[6] == 'F') && (buf[8] == 'T'))) {
    usb_close(devh);
    return 0;
  }
      
  
  cmd = buf[16];
  ret = usb_control_msg (devh,
			 USB_ENDPOINT_IN|USB_RECIP_DEVICE|USB_TYPE_VENDOR,
			 cmd,
			 0,
			 4,
			 (char *) buf,
			 sizeof(buf),
                         USB_TIMEOUT_DEFAULT);

  
  if (dumpfile != NULL && ret > 0) {
    fprintf(dumpfile, "Microsoft device response to control message 1, CMD 0x%02x:\n", cmd);
    data_dump_ascii(dumpfile, buf, ret, 16);
  }
  
  
  if (ret <= 0x15) {
    
    
    usb_close(devh);
    return 0;
  }
  
  if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
    usb_close(devh);
    return 0;
  }
      
  

  
  ret = usb_control_msg (devh,
			 USB_ENDPOINT_IN|USB_RECIP_DEVICE|USB_TYPE_VENDOR,
			 cmd,
			 0,
			 5,
			 (char *) buf,
			 sizeof(buf),
                         USB_TIMEOUT_DEFAULT);

  
  if (dumpfile != NULL && ret > 0) {
    fprintf(dumpfile, "Microsoft device response to control message 2, CMD 0x%02x:\n", cmd);
    data_dump_ascii(dumpfile, buf, ret, 16);
  }
  
  
  if (ret == -1) {
    fprintf(stderr, "Potential MTP Device with VendorID:%04x and "
	    "ProductID:%04x encountered an error responding to "
	    "control message 2.\n"
	    "Problems may arrise but continuing\n",
	    dev->descriptor.idVendor, dev->descriptor.idProduct);
  } else if (ret <= 0x15) {
    fprintf(stderr, "Potential MTP Device with VendorID:%04x and "
	    "ProductID:%04x responded to control message 2 with a "
	    "response that was too short. Problems may arrise but "
	    "continuing\n",
	    dev->descriptor.idVendor, dev->descriptor.idProduct);
  } else if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
    fprintf(stderr, "Potential MTP Device with VendorID:%04x and "
	    "ProductID:%04x encountered an error responding to "
	    "control message 2\n"
	    "Problems may arrise but continuing\n",
	    dev->descriptor.idVendor, dev->descriptor.idProduct);
  }
  
  
  usb_close(devh);
  return 1;
}
#endif 

static LIBMTP_error_number_t get_mtp_usb_device_list(mtpdevice_list_t ** mtp_device_list)
{
  struct usb_bus *bus = init_usb();
  for (; bus != NULL; bus = bus->next) {
    struct usb_device *dev = bus->devices;
    for (; dev != NULL; dev = dev->next) {
      if (dev->descriptor.bDeviceClass != USB_CLASS_HUB) {
	int i;
        int found = 0;

	
	
	
        for(i = 0; i < mtp_device_table_size; i++) {
          if(dev->descriptor.idVendor == mtp_device_table[i].vendor_id &&
            dev->descriptor.idProduct == mtp_device_table[i].product_id) {
            
            *mtp_device_list = append_to_mtpdevice_list(*mtp_device_list, 
							dev, 
							bus->location);
            found = 1;
            break;
          }
        }
	
        if (!found) {
          if (probe_device_descriptor(dev, NULL)) {
            
            *mtp_device_list = append_to_mtpdevice_list(*mtp_device_list, 
							dev,
							bus->location);
          }
        }
      }
    }
  }
  
  
  if(*mtp_device_list == NULL) {
    return LIBMTP_ERROR_NO_DEVICE_ATTACHED;
  }
  return LIBMTP_ERROR_NONE;
}

LIBMTP_error_number_t LIBMTP_Detect_Raw_Devices(LIBMTP_raw_device_t ** devices, 
			      int * numdevs)
{
  mtpdevice_list_t *devlist = NULL;
  mtpdevice_list_t *dev;
  LIBMTP_error_number_t ret;
  LIBMTP_raw_device_t *retdevs;
  int devs = 0;
  int i, j;

  ret = get_mtp_usb_device_list(&devlist);
  if (ret == LIBMTP_ERROR_NO_DEVICE_ATTACHED) {
    *devices = NULL;
    *numdevs = 0;
    return ret;
  } else if (ret != LIBMTP_ERROR_NONE) {
    fprintf(stderr, "LIBMTP PANIC: get_mtp_usb_device_list() "
	    "error code: %d on line %d\n", ret, __LINE__);
    return ret;
  }

  
  dev = devlist;
  while (dev != NULL) {
    devs++;
    dev = dev->next;
  }
  if (devs == 0) {
    *devices = NULL;
    *numdevs = 0;
    return LIBMTP_ERROR_NONE;
  }
  
  retdevs = (LIBMTP_raw_device_t *) malloc(sizeof(LIBMTP_raw_device_t) * devs);
  if (retdevs == NULL) {
    
    *devices = NULL;
    *numdevs = 0;
    return LIBMTP_ERROR_MEMORY_ALLOCATION;
  }
  dev = devlist;
  i = 0;
  while (dev != NULL) {
    int device_known = 0;

    
    retdevs[i].device_entry.vendor = NULL;
    retdevs[i].device_entry.vendor_id = dev->libusb_device->descriptor.idVendor;
    retdevs[i].device_entry.product = NULL;
    retdevs[i].device_entry.product_id = dev->libusb_device->descriptor.idProduct;
    retdevs[i].device_entry.device_flags = 0x00000000U;
    
    for(j = 0; j < mtp_device_table_size; j++) {
      if(dev->libusb_device->descriptor.idVendor == mtp_device_table[j].vendor_id &&
	 dev->libusb_device->descriptor.idProduct == mtp_device_table[j].product_id) {
	device_known = 1;
	retdevs[i].device_entry.vendor = mtp_device_table[j].vendor;
	retdevs[i].device_entry.product = mtp_device_table[j].product;
	retdevs[i].device_entry.device_flags = mtp_device_table[j].device_flags;

#ifdef _AFT_BUILD
    
	retdevs[i].device_entry.device_flags |= DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST|
                                            DEVICE_FLAG_BROKEN_SET_OBJECT_PROPLIST|
                                            DEVICE_FLAG_BROKEN_SEND_OBJECT_PROPLIST;
#endif

#ifdef ENABLE_USB_BULK_DEBUG
	
	fprintf(stderr, "Device %d (VID=%04x and PID=%04x) is a %s %s.\n", 
		i,
		dev->libusb_device->descriptor.idVendor,
		dev->libusb_device->descriptor.idProduct,
		mtp_device_table[j].vendor,
		mtp_device_table[j].product);
#endif
	break;
      }
    }
    if (!device_known) {
      
      fprintf(stderr, "Device %d (VID=%04x and PID=%04x) is UNKNOWN.\n", 
	      i,
	      dev->libusb_device->descriptor.idVendor,
	      dev->libusb_device->descriptor.idProduct);
      fprintf(stderr, "Please report this VID/PID and the device model to the "
	      "libmtp development team\n");
    }
    
    retdevs[i].bus_location = dev->bus_location;
    retdevs[i].devnum = dev->libusb_device->devnum;
    i++;
    dev = dev->next;
  }  
  *devices = retdevs;
  *numdevs = i;
  free_mtpdevice_list(devlist);
  return LIBMTP_ERROR_NONE;
}

void dump_usbinfo(PTP_USB *ptp_usb)
{
  struct usb_device *dev;

#ifdef LIBUSB_HAS_GET_DRIVER_NP
  char devname[0x10];
  int res;
  
  devname[0] = '\0';
  res = usb_get_driver_np(ptp_usb->handle, (int) ptp_usb->interface, devname, sizeof(devname));
  if (devname[0] != '\0') {
    printf("   Using kernel interface \"%s\"\n", devname);
  }
#endif
  dev = usb_device(ptp_usb->handle);
  printf("   bcdUSB: %d\n", dev->descriptor.bcdUSB);
  printf("   bDeviceClass: %d\n", dev->descriptor.bDeviceClass);
  printf("   bDeviceSubClass: %d\n", dev->descriptor.bDeviceSubClass);
  printf("   bDeviceProtocol: %d\n", dev->descriptor.bDeviceProtocol);
  printf("   idVendor: %04x\n", dev->descriptor.idVendor);
  printf("   idProduct: %04x\n", dev->descriptor.idProduct);
  printf("   IN endpoint maxpacket: %d bytes\n", ptp_usb->inep_maxpacket);
  printf("   OUT endpoint maxpacket: %d bytes\n", ptp_usb->outep_maxpacket);
  printf("   Raw device info:\n");
  printf("      Bus location: %d\n", ptp_usb->rawdevice.bus_location);
  printf("      Device number: %d\n", ptp_usb->rawdevice.devnum);
  printf("      Device entry info:\n");
  printf("         Vendor: %s\n", ptp_usb->rawdevice.device_entry.vendor);
  printf("         Vendor id: 0x%04x\n", ptp_usb->rawdevice.device_entry.vendor_id);
  printf("         Product: %s\n", ptp_usb->rawdevice.device_entry.product);
  printf("         Vendor id: 0x%04x\n", ptp_usb->rawdevice.device_entry.product_id);
  printf("         Device flags: 0x%08x\n", ptp_usb->rawdevice.device_entry.device_flags);
  (void) probe_device_descriptor(dev, stdout);
}

const char *get_playlist_extension(PTP_USB *ptp_usb)
{
  struct usb_device *dev;
  static char creative_pl_extension[] = ".zpl";
  static char default_pl_extension[] = ".pla";

  dev = usb_device(ptp_usb->handle);
  if (dev->descriptor.idVendor == 0x041e) {
    return creative_pl_extension;
  }
  return default_pl_extension;
}

static void
libusb_glue_debug (PTPParams *params, const char *format, ...)
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
libusb_glue_error (PTPParams *params, const char *format, ...)
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


#define CONTEXT_BLOCK_SIZE_1	0x3e00
#define CONTEXT_BLOCK_SIZE_2  0x200
#define CONTEXT_BLOCK_SIZE    CONTEXT_BLOCK_SIZE_1+CONTEXT_BLOCK_SIZE_2
static short
ptp_read_func (
	unsigned long size, PTPDataHandler *handler,void *data,
	unsigned long *readbytes,
	int readzero
) {
  PTP_USB *ptp_usb = (PTP_USB *)data;
  unsigned long toread = 0;
  int result = 0;
  unsigned long curread = 0;
  unsigned long written;
  unsigned char *bytes;
  int expect_terminator_byte = 0;

  
  bytes = malloc(CONTEXT_BLOCK_SIZE);
  while (curread < size) {
    
#ifdef ENABLE_USB_BULK_DEBUG
    printf("Remaining size to read: 0x%04lx bytes\n", size - curread);
#endif
    
    if (size - curread < CONTEXT_BLOCK_SIZE)
    {
      
      toread = size - curread;
      
      if (readzero && FLAG_NO_ZERO_READS(ptp_usb) && toread % 64 == 0) {
        toread += 1;
        expect_terminator_byte = 1;
      }
    }
    else if (curread == 0)
      
      toread = CONTEXT_BLOCK_SIZE_1;
    else if (toread == CONTEXT_BLOCK_SIZE_1)
      toread = CONTEXT_BLOCK_SIZE_2;
    else if (toread == CONTEXT_BLOCK_SIZE_2)
      toread = CONTEXT_BLOCK_SIZE_1;
    else
      printf("unexpected toread size 0x%04x, 0x%04x remaining bytes\n", 
	     (unsigned int) toread, (unsigned int) (size-curread));

#ifdef ENABLE_USB_BULK_DEBUG
    printf("Reading in 0x%04lx bytes\n", toread);
#endif
    result = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep, (char*)bytes, toread, ptp_usb->timeout);
#ifdef ENABLE_USB_BULK_DEBUG
    printf("Result of read: 0x%04x\n", result);
#endif
        
    if (result < 0) {
      return PTP_ERROR_IO;
    }
#ifdef ENABLE_USB_BULK_DEBUG
    printf("<==USB IN\n");
    if (result == 0)
      printf("Zero Read\n");
    else if (result < 0) 
      fprintf(stderr, "USB_BULK_READ result=%#x\n", result);
    else 
      data_dump_ascii (stdout,bytes,result,16);
#endif
    
    
    if (expect_terminator_byte && result == toread)
    {
#ifdef ENABLE_USB_BULK_DEBUG
      printf("<==USB IN\nDiscarding extra byte\n");
#endif
      result--;
    }
    
    int putfunc_ret = handler->putfunc(NULL, handler->priv, result, bytes, &written);
    if (putfunc_ret != PTP_RC_OK)
      return putfunc_ret;
    
    ptp_usb->current_transfer_complete += result;
    curread += result;

    
    if (ptp_usb->callback_active) {
      if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total) {
	
	ptp_usb->current_transfer_complete = ptp_usb->current_transfer_total;
	ptp_usb->callback_active = 0;
      }
      if (ptp_usb->current_transfer_callback != NULL) {
	int ret;
	ret = ptp_usb->current_transfer_callback(ptp_usb->current_transfer_complete,
						 ptp_usb->current_transfer_total,
						 ptp_usb->current_transfer_callback_data);
	if (ret != 0) {
	  return PTP_ERROR_CANCEL;
	}
      }
    }  

    if (result < toread) 
      break;
  }
  if (readbytes) *readbytes = curread;
  free (bytes);
  
  
  if (readzero && 
      !FLAG_NO_ZERO_READS(ptp_usb) && 
      curread % ptp_usb->outep_maxpacket == 0) {
    char temp;
    int zeroresult = 0;

#ifdef ENABLE_USB_BULK_DEBUG
    printf("<==USB IN\n");
    printf("Zero Read\n");
#endif
    zeroresult = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep, &temp, 0, ptp_usb->timeout);
    if (zeroresult != 0)
      printf("LIBMTP panic: unable to read in zero packet, response 0x%04x", zeroresult);
  }
  
  return PTP_RC_OK;
}

static short
ptp_write_func (
        unsigned long   size,
        PTPDataHandler  *handler,
        void            *data,
        unsigned long   *written
) {
  PTP_USB *ptp_usb = (PTP_USB *)data;
  unsigned long towrite = 0;
  int result = 0;
  unsigned long curwrite = 0;
  unsigned char *bytes;

  
  bytes = malloc(CONTEXT_BLOCK_SIZE);
  if (!bytes) {
    return PTP_ERROR_IO;
  }
  while (curwrite < size) {
    unsigned long usbwritten = 0;
    towrite = size-curwrite;
    if (towrite > CONTEXT_BLOCK_SIZE) {
      towrite = CONTEXT_BLOCK_SIZE;
    } else {
      
      if (towrite > ptp_usb->outep_maxpacket && towrite % ptp_usb->outep_maxpacket != 0) {
        towrite -= towrite % ptp_usb->outep_maxpacket;
      }
    }
    int getfunc_ret = handler->getfunc(NULL, handler->priv,towrite,bytes,&towrite);
    if (getfunc_ret != PTP_RC_OK)
      return getfunc_ret;
    while (usbwritten < towrite) {
	    result = USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,((char*)bytes+usbwritten),towrite-usbwritten,ptp_usb->timeout);
#ifdef ENABLE_USB_BULK_DEBUG
	    printf("USB OUT==>\n");
        if (result > 0) { 
            data_dump_ascii (stdout,bytes+usbwritten,result,16);
        } else {
            fprintf(stderr, "USB_BULK_WRITE: result=%#x\n", result);
        }
#endif
	    if (result < 0) {
	      return PTP_ERROR_IO;
	    }
	    
	    
	    ptp_usb->current_transfer_complete += result;
	    curwrite += result;
	    usbwritten += result;
    }
    
    if (ptp_usb->callback_active) {
      if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total) {
	
	ptp_usb->current_transfer_complete = ptp_usb->current_transfer_total;
	ptp_usb->callback_active = 0;
      }
      if (ptp_usb->current_transfer_callback != NULL) {
	int ret;
	ret = ptp_usb->current_transfer_callback(ptp_usb->current_transfer_complete,
						 ptp_usb->current_transfer_total,
						 ptp_usb->current_transfer_callback_data);
	if (ret != 0) {
	  return PTP_ERROR_CANCEL;
	}
      }
    }
    if (result < towrite) 
      break;
  }
  free (bytes);
  if (written) {
    *written = curwrite;
  }
  

  
  if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total) {
    if ((towrite % ptp_usb->outep_maxpacket) == 0) {
#ifdef ENABLE_USB_BULK_DEBUG
      printf("USB OUT==>\n");
      printf("Zero Write\n");
#endif
      result=USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char *)"x",0,ptp_usb->timeout);
    }
  }
    
  if (result < 0)
    return PTP_ERROR_IO;
  return PTP_RC_OK;
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


uint16_t
ptp_usb_sendreq (PTPParams* params, PTPContainer* req)
{
	uint16_t ret;
	PTPUSBBulkContainer usbreq;
	PTPDataHandler	memhandler;
	unsigned long written = 0;
	unsigned long towrite;
#ifdef ENABLE_USB_BULK_DEBUG
	char txt[256];

	(void) ptp_render_opcode (params, req->Code, sizeof(txt), txt);
	printf("REQUEST: 0x%04x, %s\n", req->Code, txt);
#endif
	
	usbreq.length=htod32(PTP_USB_BULK_REQ_LEN-
		(sizeof(uint32_t)*(5-req->Nparam)));
	usbreq.type=htod16(PTP_USB_CONTAINER_COMMAND);
	usbreq.code=htod16(req->Code);
	usbreq.trans_id=htod32(req->Transaction_ID);
	usbreq.payload.params.param1=htod32(req->Param1);
	usbreq.payload.params.param2=htod32(req->Param2);
	usbreq.payload.params.param3=htod32(req->Param3);
	usbreq.payload.params.param4=htod32(req->Param4);
	usbreq.payload.params.param5=htod32(req->Param5);
	
	towrite = PTP_USB_BULK_REQ_LEN-(sizeof(uint32_t)*(5-req->Nparam));
	ptp_init_send_memory_handler (&memhandler, (unsigned char*)&usbreq, towrite);
	ret=ptp_write_func(
		towrite,
		&memhandler,
		params->data,
		&written
	);
	ptp_exit_send_memory_handler (&memhandler);
	if (ret!=PTP_RC_OK && ret!=PTP_ERROR_CANCEL) {
		ret = PTP_ERROR_IO;
	}
	if (written != towrite && ret != PTP_ERROR_CANCEL && ret != PTP_ERROR_IO) {
		libusb_glue_error (params, 
			"PTP: request code 0x%04x sending req wrote only %ld bytes instead of %d",
			req->Code, written, towrite
		);
		ret = PTP_ERROR_IO;
	}
	return ret;
}

uint16_t
ptp_usb_senddata (PTPParams* params, PTPContainer* ptp,
		  unsigned long size, PTPDataHandler *handler
) {
	uint16_t ret;
	int wlen, datawlen;
	unsigned long written;
	PTPUSBBulkContainer usbdata;
	uint32_t bytes_left_to_transfer;
	PTPDataHandler memhandler;

#ifdef ENABLE_USB_BULK_DEBUG
	printf("SEND DATA PHASE\n");
#endif
	
	usbdata.length	= htod32(PTP_USB_BULK_HDR_LEN+size);
	usbdata.type	= htod16(PTP_USB_CONTAINER_DATA);
	usbdata.code	= htod16(ptp->Code);
	usbdata.trans_id= htod32(ptp->Transaction_ID);
  
	((PTP_USB*)params->data)->current_transfer_complete = 0;
	((PTP_USB*)params->data)->current_transfer_total = size+PTP_USB_BULK_HDR_LEN;

	if (params->split_header_data) {
		datawlen = 0;
		wlen = PTP_USB_BULK_HDR_LEN;
	} else {
		unsigned long gotlen;
		
		datawlen = (size<PTP_USB_BULK_PAYLOAD_LEN_WRITE)?size:PTP_USB_BULK_PAYLOAD_LEN_WRITE;
		wlen = PTP_USB_BULK_HDR_LEN + datawlen;
    
		ret = handler->getfunc(params, handler->priv, datawlen, usbdata.payload.data, &gotlen);
		if (ret != PTP_RC_OK)
			return ret;
		if (gotlen != datawlen)
			return PTP_RC_GeneralError;
	}
	ptp_init_send_memory_handler (&memhandler, (unsigned char *)&usbdata, wlen);
	
	ret = ptp_write_func(wlen, &memhandler, params->data, &written);
	ptp_exit_send_memory_handler (&memhandler);
	if (ret!=PTP_RC_OK) {
		return ret;
	}
	if (size <= datawlen) return ret;
	
	bytes_left_to_transfer = size-datawlen;
	ret = PTP_RC_OK;
	while(bytes_left_to_transfer > 0) {
		ret = ptp_write_func (bytes_left_to_transfer, handler, params->data, &written);
		if (ret != PTP_RC_OK)
			break;
		if (written == 0) {
			ret = PTP_ERROR_IO;
			break;
		}
		bytes_left_to_transfer -= written;
	}
	if (ret!=PTP_RC_OK && ret!=PTP_ERROR_CANCEL)
		ret = PTP_ERROR_IO;
	return ret;
}

static uint16_t ptp_usb_getpacket(PTPParams *params,
		PTPUSBBulkContainer *packet, unsigned long *rlen)
{
	PTPDataHandler	memhandler;
	uint16_t	ret;
	unsigned char	*x = NULL;

	
	if (params->response_packet_size > 0) {
		
		memcpy(packet, params->response_packet, params->response_packet_size);
		*rlen = params->response_packet_size;
		free(params->response_packet);
		params->response_packet = NULL;
		params->response_packet_size = 0;
		
		return PTP_RC_OK;
	}
	ptp_init_recv_memory_handler (&memhandler);
	ret = ptp_read_func(PTP_USB_BULK_HS_MAX_PACKET_LEN_READ, &memhandler, params->data, rlen, 0);
	ptp_exit_recv_memory_handler (&memhandler, &x, rlen);
	if (x) {
		memcpy (packet, x, *rlen);
		free (x);
	}
	return ret;
}

uint16_t
ptp_usb_getdata (PTPParams* params, PTPContainer* ptp, PTPDataHandler *handler)
{
	uint16_t ret;
	PTPUSBBulkContainer usbdata;
	unsigned long	written;
	PTP_USB *ptp_usb = (PTP_USB *) params->data;

#ifdef ENABLE_USB_BULK_DEBUG
	printf("GET DATA PHASE\n");
#endif
	memset(&usbdata,0,sizeof(usbdata));
	do {
		unsigned long len, rlen;

		ret = ptp_usb_getpacket(params, &usbdata, &rlen);
		if (ret!=PTP_RC_OK) {
			ret = PTP_ERROR_IO;
			break;
		}
		if (dtoh16(usbdata.type)!=PTP_USB_CONTAINER_DATA) {
			ret = PTP_ERROR_DATA_EXPECTED;
			break;
		}
		if (dtoh16(usbdata.code)!=ptp->Code) {
			if (FLAG_IGNORE_HEADER_ERRORS(ptp_usb)) {
				libusb_glue_debug (params, "ptp2/ptp_usb_getdata: detected a broken "
					   "PTP header, code field insane, expect problems! (But continuing)");
				
				
				usbdata.code	 = htod16(ptp->Code);
				usbdata.trans_id = htod32(ptp->Transaction_ID);
				ret = PTP_RC_OK;
			} else {
				ret = dtoh16(usbdata.code);
				
				
				
				
				if (ret < PTP_RC_Undefined || ret > PTP_RC_SpecificationOfDestinationUnsupported) {
					libusb_glue_debug (params, "ptp2/ptp_usb_getdata: detected a broken "
						   "PTP header, code field insane.");
					ret = PTP_ERROR_IO;
				}
				break;
			}
		}
		if (usbdata.length == 0xffffffffU) {
			
      int putfunc_ret = 
			handler->putfunc(
				params, handler->priv, rlen - PTP_USB_BULK_HDR_LEN, usbdata.payload.data,
				&written
			);
      if (putfunc_ret != PTP_RC_OK)
        return putfunc_ret;
			
			while (1) {
				unsigned long readdata;
				uint16_t xret;

				xret = ptp_read_func(
					PTP_USB_BULK_HS_MAX_PACKET_LEN_READ,
					handler,
					params->data,
					&readdata,
					0
				);
				if (xret != PTP_RC_OK)
					return xret;
				if (readdata < PTP_USB_BULK_HS_MAX_PACKET_LEN_READ)
					break;
			}
			return PTP_RC_OK;
		}
		if (rlen > dtoh32(usbdata.length)) {
			unsigned int packlen = dtoh32(usbdata.length);
			unsigned int surplen = rlen - packlen;

			if (surplen >= PTP_USB_BULK_HDR_LEN) {
				params->response_packet = malloc(surplen);
				memcpy(params->response_packet,
				       (uint8_t *) &usbdata + packlen, surplen);
				params->response_packet_size = surplen;
			
			} else if(!FLAG_NO_ZERO_READS(ptp_usb) &&
				  (rlen - dtoh32(usbdata.length) == 1)) {
			  libusb_glue_debug (params, "ptp2/ptp_usb_getdata: read %d bytes "
				     "too much, expect problems!", 
				     rlen - dtoh32(usbdata.length));
			}
			rlen = packlen;
		}

		
		len=dtoh32(usbdata.length)-PTP_USB_BULK_HDR_LEN;

		
		if (dtoh32(usbdata.length) > 12 && (rlen==12))
			params->split_header_data = 1;

		
    int putfunc_ret = 
		handler->putfunc(
			params, handler->priv, rlen - PTP_USB_BULK_HDR_LEN, usbdata.payload.data,
			&written
		);
    if (putfunc_ret != PTP_RC_OK)
      return putfunc_ret;

		if (FLAG_NO_ZERO_READS(ptp_usb) &&
		    len+PTP_USB_BULK_HDR_LEN == PTP_USB_BULK_HS_MAX_PACKET_LEN_READ) {
#ifdef ENABLE_USB_BULK_DEBUG
		  printf("Reading in extra terminating byte\n");
#endif
		  
		  int result = 0;
		  char byte = 0;
                  result = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep, &byte, 1, ptp_usb->timeout);

		  if (result != 1)
		    printf("Could not read in extra byte for PTP_USB_BULK_HS_MAX_PACKET_LEN_READ long file, return value 0x%04x\n", result);
		} else if (len+PTP_USB_BULK_HDR_LEN == PTP_USB_BULK_HS_MAX_PACKET_LEN_READ && params->split_header_data == 0) {
		  int zeroresult = 0;
		  char zerobyte = 0;

#ifdef ENABLE_USB_BULK_DEBUG
		  printf("Reading in zero packet after header\n");
#endif
                  zeroresult = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep, &zerobyte, 0, ptp_usb->timeout);
		  
		  if (zeroresult != 0)
		    printf("LIBMTP panic: unable to read in zero packet, response 0x%04x", zeroresult);
		}
		
		
		if (len+PTP_USB_BULK_HDR_LEN<=rlen) {
		  break;
		}
		
		ret = ptp_read_func(len - (rlen - PTP_USB_BULK_HDR_LEN),
				    handler,
				    params->data, &rlen, 1);
		
		if (ret!=PTP_RC_OK) {
		  break;
		}
	} while (0);
	return ret;
}

uint16_t
ptp_usb_getresp (PTPParams* params, PTPContainer* resp)
{
	uint16_t ret;
	unsigned long rlen;
	PTPUSBBulkContainer usbresp;
	PTP_USB *ptp_usb = (PTP_USB *)(params->data);

#ifdef ENABLE_USB_BULK_DEBUG
	printf("RESPONSE: ");
#endif
	memset(&usbresp,0,sizeof(usbresp));
	
	ret = ptp_usb_getpacket(params, &usbresp, &rlen);

	
	
	
	
	while (ret==PTP_RC_OK && rlen<PTP_USB_BULK_HDR_LEN && usbresp.length==0) {
	  libusb_glue_debug (params, "ptp_usb_getresp: detected short response "
		     "of %d bytes, expect problems! (re-reading "
		     "response), rlen");
	  ret = ptp_usb_getpacket(params, &usbresp, &rlen);
	}

	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
	} else
	if (dtoh16(usbresp.type)!=PTP_USB_CONTAINER_RESPONSE) {
		ret = PTP_ERROR_RESP_EXPECTED;
	} else
	if (dtoh16(usbresp.code)!=resp->Code) {
		ret = dtoh16(usbresp.code);
	}
#ifdef ENABLE_USB_BULK_DEBUG
	printf("%04x\n", ret);
#endif
	if (ret!=PTP_RC_OK) {
		return ret;
	}
	
	resp->Code=dtoh16(usbresp.code);
	resp->SessionID=params->session_id;
	resp->Transaction_ID=dtoh32(usbresp.trans_id);
	if (FLAG_IGNORE_HEADER_ERRORS(ptp_usb)) {
		if (resp->Transaction_ID != params->transaction_id-1) {
			libusb_glue_debug (params, "ptp_usb_getresp: detected a broken "
				   "PTP header, transaction ID insane, expect "
				   "problems! (But continuing)");
			
			resp->Transaction_ID = params->transaction_id-1;
		}
	}
	resp->Param1=dtoh32(usbresp.payload.params.param1);
	resp->Param2=dtoh32(usbresp.payload.params.param2);
	resp->Param3=dtoh32(usbresp.payload.params.param3);
	resp->Param4=dtoh32(usbresp.payload.params.param4);
	resp->Param5=dtoh32(usbresp.payload.params.param5);
	return ret;
}


#define PTP_EVENT_CHECK			0x0000	
#define PTP_EVENT_CHECK_FAST		0x0001	

static inline uint16_t
ptp_usb_event (PTPParams* params, PTPContainer* event, int wait)
{
	uint16_t ret;
	int result;
	unsigned long rlen;
	PTPUSBEventContainer usbevent;
	PTP_USB *ptp_usb = (PTP_USB *)(params->data);

	memset(&usbevent,0,sizeof(usbevent));

	if ((params==NULL) || (event==NULL)) 
		return PTP_ERROR_BADPARAM;
	ret = PTP_RC_OK;
	switch(wait) {
	case PTP_EVENT_CHECK:
                result=USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *)&usbevent,sizeof(usbevent),ptp_usb->timeout);
		if (result==0)
                        result = USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *) &usbevent, sizeof(usbevent), ptp_usb->timeout);
		if (result < 0) ret = PTP_ERROR_IO;
		break;
	case PTP_EVENT_CHECK_FAST:
                result=USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *)&usbevent,sizeof(usbevent),ptp_usb->timeout);
		if (result==0)
                        result = USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *) &usbevent, sizeof(usbevent), ptp_usb->timeout);
		if (result < 0) ret = PTP_ERROR_IO;
		break;
	default:
		ret=PTP_ERROR_BADPARAM;
		break;
	}
	if (ret!=PTP_RC_OK) {
		libusb_glue_error (params,
			"PTP: reading event an error 0x%04x occurred", ret);
		return PTP_ERROR_IO;
	}
	rlen = result;
	if (rlen < 8) {
		libusb_glue_error (params,
			"PTP: reading event an short read of %ld bytes occurred", rlen);
		return PTP_ERROR_IO;
	}
	
	
	event->Code=dtoh16(usbevent.code);
	event->SessionID=params->session_id;
	event->Transaction_ID=dtoh32(usbevent.trans_id);
	event->Param1=dtoh32(usbevent.param1);
	event->Param2=dtoh32(usbevent.param2);
	event->Param3=dtoh32(usbevent.param3);
	return ret;
}

uint16_t
ptp_usb_event_check (PTPParams* params, PTPContainer* event) {

	return ptp_usb_event (params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_usb_event_wait (PTPParams* params, PTPContainer* event) {

	return ptp_usb_event (params, event, PTP_EVENT_CHECK);
}

uint16_t
ptp_usb_control_cancel_request (PTPParams *params, uint32_t transactionid) {
	PTP_USB *ptp_usb = (PTP_USB *)(params->data);
	int ret;
	unsigned char buffer[6];

	htod16a(&buffer[0],PTP_EC_CancelTransaction);
	htod32a(&buffer[2],transactionid);
	ret = usb_control_msg(ptp_usb->handle, 
			      USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                              0x64, 0x0000, 0x0000, (char *) buffer, sizeof(buffer), ptp_usb->timeout);
	if (ret < sizeof(buffer))
		return PTP_ERROR_IO;
	return PTP_RC_OK;
}

static int init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev)
{
  usb_dev_handle *device_handle;
  
  params->sendreq_func=ptp_usb_sendreq;
  params->senddata_func=ptp_usb_senddata;
  params->getresp_func=ptp_usb_getresp;
  params->getdata_func=ptp_usb_getdata;
  params->cancelreq_func=ptp_usb_control_cancel_request;
  params->data=ptp_usb;
  params->transaction_id=0;
  params->byteorder = PTP_DL_LE;
  
  ptp_usb->timeout = USB_TIMEOUT_DEFAULT;
  
  device_handle = usb_open(dev);
  if (!device_handle) {
    perror("usb_open()");
    return -1;
  }

  ptp_usb->handle = device_handle;
#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
  if (FLAG_UNLOAD_DRIVER(ptp_usb)) {
    if (usb_detach_kernel_driver_np(device_handle, (int) ptp_usb->interface)) {
	  
	  
    }
  }
#endif
#ifdef __WIN32__
  
  if (usb_set_configuration(device_handle, dev->config->bConfigurationValue)) {
    perror("usb_set_configuration()");
    return -1;
  }
#endif
  if (usb_claim_interface(device_handle, (int) ptp_usb->interface)) {
    perror("usb_claim_interface()");
    return -1;
  }

  return 0;
}

static void clear_stall(PTP_USB* ptp_usb)
{
  uint16_t status;
  int ret;
  
  
  status = 0;
  ret = usb_get_endpoint_status(ptp_usb,ptp_usb->inep,&status);
  if (ret<0) {
    perror ("inep: usb_get_endpoint_status()");
  } else if (status) {
    printf("Clearing stall on IN endpoint\n");
    ret = usb_clear_stall_feature(ptp_usb,ptp_usb->inep);
    if (ret<0) {
      perror ("usb_clear_stall_feature()");
    }
  }
  
  
  status=0;
  ret = usb_get_endpoint_status(ptp_usb,ptp_usb->outep,&status);
  if (ret<0) {
    perror("outep: usb_get_endpoint_status()");
  } else if (status) {
    printf("Clearing stall on OUT endpoint\n");
    ret = usb_clear_stall_feature(ptp_usb,ptp_usb->outep);
    if (ret<0) {
      perror("usb_clear_stall_feature()");
    }
  }

  
}

static void clear_halt(PTP_USB* ptp_usb)
{
  int ret;

  ret = usb_clear_halt(ptp_usb->handle,ptp_usb->inep);
  if (ret<0) {
    perror("usb_clear_halt() on IN endpoint");
  }
  ret = usb_clear_halt(ptp_usb->handle,ptp_usb->outep);
  if (ret<0) {
    perror("usb_clear_halt() on OUT endpoint");
  }
  ret = usb_clear_halt(ptp_usb->handle,ptp_usb->intep);
  if (ret<0) {
    perror("usb_clear_halt() on INTERRUPT endpoint");
  }
}

static void close_usb(PTP_USB* ptp_usb)
{
  
  
  if (!FLAG_NO_RELEASE_INTERFACE(ptp_usb)) {

    clear_stall(ptp_usb);
    
    clear_halt(ptp_usb);
    
    
    
    usb_resetep(ptp_usb->handle, ptp_usb->outep);
    usb_release_interface(ptp_usb->handle, (int) ptp_usb->interface);
  }

  usb_close(ptp_usb->handle);
}

static void find_interface_and_endpoints(struct usb_device *dev, 
					 uint8_t *interface,
					 int* inep, 
					 int* inep_maxpacket, 
					 int* outep, 
					 int *outep_maxpacket, 
					 int* intep)
{
  int i;

  
  for (i = 0; i < dev->descriptor.bNumConfigurations; i++) {
    uint8_t j;

    for (j = 0; j < dev->config[i].bNumInterfaces; j++) {
      uint8_t k;
      uint8_t no_ep;
      struct usb_endpoint_descriptor *ep;
      
      if (dev->descriptor.bNumConfigurations > 1 || dev->config[i].bNumInterfaces > 1) {
	
	
	
	
      }

      *interface = dev->config[i].interface[j].altsetting->bInterfaceNumber;
      ep = dev->config[i].interface[j].altsetting->endpoint;
      no_ep = dev->config[i].interface[j].altsetting->bNumEndpoints;
      
      for (k = 0; k < no_ep; k++) {
	if (ep[k].bmAttributes==USB_ENDPOINT_TYPE_BULK)	{
	  if ((ep[k].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==
	      USB_ENDPOINT_DIR_MASK)
	    {
	      *inep=ep[k].bEndpointAddress;
	      *inep_maxpacket=ep[k].wMaxPacketSize;
	    }
	  if ((ep[k].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==0)
	    {
	      *outep=ep[k].bEndpointAddress;
	      *outep_maxpacket=ep[k].wMaxPacketSize;
	    }
	} else if (ep[k].bmAttributes==USB_ENDPOINT_TYPE_INTERRUPT){
	  if ((ep[k].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==
	      USB_ENDPOINT_DIR_MASK)
	    {
	      *intep=ep[k].bEndpointAddress;
	    }
	}
      }
      
      return;
    }
  }
}

LIBMTP_error_number_t configure_usb_device(LIBMTP_raw_device_t *device, 
					   PTPParams *params,
					   void **usbinfo)
{
  PTP_USB *ptp_usb;
  struct usb_device *libusb_device;
  uint16_t ret = 0;
  struct usb_bus *bus;
  int found = 0;

  
  bus = init_usb();
  for (; bus != NULL; bus = bus->next) {
    if (bus->location == device->bus_location) {
      struct usb_device *dev = bus->devices;

      for (; dev != NULL; dev = dev->next) {
	if(dev->devnum == device->devnum &&
	   dev->descriptor.idVendor == device->device_entry.vendor_id &&
	   dev->descriptor.idProduct == device->device_entry.product_id ) {
	  libusb_device = dev;
	  found = 1;
	  break;
	}
      }
      if (found)
	break;
    }
  }
  
  if (!found) {
    return LIBMTP_ERROR_NO_DEVICE_ATTACHED;
  }

  
  ptp_usb = (PTP_USB *) malloc(sizeof(PTP_USB));
  if (ptp_usb == NULL) {
    return LIBMTP_ERROR_MEMORY_ALLOCATION;
  }
  
  memset(ptp_usb, 0, sizeof(PTP_USB));

  
  memcpy(&ptp_usb->rawdevice, device, sizeof(LIBMTP_raw_device_t));

  if (FLAG_ALWAYS_PROBE_DESCRIPTOR(ptp_usb)) {
    
    (void) probe_device_descriptor(libusb_device, NULL);
  }
  

  
  find_interface_and_endpoints(libusb_device,
		   &ptp_usb->interface,
		   &ptp_usb->inep,
		   &ptp_usb->inep_maxpacket,
		   &ptp_usb->outep,
		   &ptp_usb->outep_maxpacket,
		   &ptp_usb->intep);
    
  
  if (init_ptp_usb(params, ptp_usb, libusb_device) < 0) {
    fprintf(stderr, "LIBMTP PANIC: Unable to initialize device\n");
    return LIBMTP_ERROR_CONNECTING;
  }
  
  if ((ret = ptp_opensession(params, 1)) == PTP_ERROR_IO) {
    fprintf(stderr, "PTP_ERROR_IO: Trying again after re-initializing USB interface\n");
    close_usb(ptp_usb);
      
    if(init_ptp_usb(params, ptp_usb, libusb_device) <0) {
      fprintf(stderr, "LIBMTP PANIC: Could not open session on device\n");
      return LIBMTP_ERROR_CONNECTING;
    }
    
    
    ret = ptp_opensession(params, 1);
  }
  
  
  if (ret == PTP_RC_InvalidTransactionID) {
    fprintf(stderr, "LIBMTP WARNING: Transaction ID was invalid, increment and try again\n");
    params->transaction_id += 10;
    ret = ptp_opensession(params, 1);
  }

  if (ret != PTP_RC_SessionAlreadyOpened && ret != PTP_RC_OK) {
    fprintf(stderr, "LIBMTP PANIC: Could not open session! "
	    "(Return code %d)\n  Try to reset the device.\n",
	    ret);
    usb_release_interface(ptp_usb->handle,
			  (int) ptp_usb->interface);
    return LIBMTP_ERROR_CONNECTING;
  }

  
  *usbinfo = (void *) ptp_usb;
  return LIBMTP_ERROR_NONE;
}


void close_device (PTP_USB *ptp_usb, PTPParams *params)
{
  if (ptp_closesession(params)!=PTP_RC_OK)
    fprintf(stderr,"ERROR: Could not close session!\n");
  close_usb(ptp_usb);
}

void set_usb_device_timeout(PTP_USB *ptp_usb, int timeout)
{
    ptp_usb->timeout = timeout;
}

void get_usb_device_timeout(PTP_USB *ptp_usb, int *timeout)
{
    *timeout = ptp_usb->timeout;
}

static int usb_clear_stall_feature(PTP_USB* ptp_usb, int ep)
{
  
  return (usb_control_msg(ptp_usb->handle,
			  USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE, USB_FEATURE_HALT,
                          ep, NULL, 0, ptp_usb->timeout));
}

static int usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status)
{
  return (usb_control_msg(ptp_usb->handle,
			  USB_DP_DTH|USB_RECIP_ENDPOINT, USB_REQ_GET_STATUS,
                          USB_FEATURE_HALT, ep, (char *)status, 2, ptp_usb->timeout));
}

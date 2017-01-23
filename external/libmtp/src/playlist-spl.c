/**
 * \File playlist-spl.c
 *
 * Playlist_t to Samsung (.spl) and back conversion functions.
 *
 * Copyright (C) 2008 Alistair Boyle <alistair.js.boyle@gmail.com>
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#include <fcntl.h>

#include <string.h>

#include "libmtp.h"
#include "libusb-glue.h"
#include "ptp.h"
#include "unicode.h"

#include "playlist-spl.h"

#define DEBUG_ENABLED 0

#define IF_DEBUG() if(DEBUG_ENABLED) {\
                     printf("%s:%u:%s(): ", __FILE__, __LINE__, __func__); \
                   } \
                   if(DEBUG_ENABLED)

typedef struct text_struct {
  char* text; 
  struct text_struct *next; 
} text_t;


static text_t* read_into_spl_text_t(LIBMTP_mtpdevice_t *device, const int fd);
static void write_from_spl_text_t(LIBMTP_mtpdevice_t *device, const int fd, text_t* p);
static void free_spl_text_t(text_t* p);
static void print_spl_text_t(text_t* p);
static uint32_t trackno_spl_text_t(text_t* p);
static void tracks_from_spl_text_t(text_t* p, uint32_t* tracks, LIBMTP_folder_t* folders, LIBMTP_file_t* files);
static void spl_text_t_from_tracks(text_t** p, uint32_t* tracks, const uint32_t trackno, const uint32_t ver_major, const uint32_t ver_minor, char* dnse, LIBMTP_folder_t* folders, LIBMTP_file_t* files);

static uint32_t discover_id_from_filepath(const char* s, LIBMTP_folder_t* folders, LIBMTP_file_t* files); 
static void discover_filepath_from_id(char** p, uint32_t track, LIBMTP_folder_t* folders, LIBMTP_file_t* files);
static void find_folder_name(LIBMTP_folder_t* folders, uint32_t* id, char** name);
static uint32_t find_folder_id(LIBMTP_folder_t* folders, uint32_t parent, char* name);

static void append_text_t(text_t** t, char* s);




int is_spl_playlist(PTPObjectInfo *oi)
{
  return (oi->ObjectFormat == PTP_OFC_Undefined) &&
         (strlen(oi->Filename) > 4) &&
         (strcmp((oi->Filename + strlen(oi->Filename) -4), ".spl") == 0);
}

#ifndef HAVE_MKSTEMP
# ifdef __WIN32__
#  include <fcntl.h>
#  define mkstemp(_pattern) _open(_mktemp(_pattern), _O_CREAT | _O_SHORT_LIVED | _O_EXCL)
# else
#  error Missing mkstemp() function.
# endif
#endif


void spl_to_playlist_t(LIBMTP_mtpdevice_t* device, PTPObjectInfo *oi,
                       const uint32_t id, LIBMTP_playlist_t * const pl)
{
  
  
  pl->name = malloc(sizeof(char)*(strlen(oi->Filename) -4 +1));
  memcpy(pl->name, oi->Filename, strlen(oi->Filename) -4);
  
  pl->name[strlen(oi->Filename) - 4] = 0;
  pl->playlist_id = id;
  pl->parent_id = oi->ParentObject;
  pl->storage_id = oi->StorageID;
  pl->tracks = NULL;
  pl->no_tracks = 0;

  IF_DEBUG() printf("pl->name='%s'\n",pl->name);

  
  char tmpname[] = "/tmp/mtp-spl2pl-XXXXXX";
  int fd = mkstemp(tmpname);
  if(fd < 0) {
    printf("failed to make temp file for %s.spl -> %s, errno=%s\n", pl->name, tmpname, strerror(errno));
    return;
  }
  
  if(unlink(tmpname) < 0)
    printf("failed to delete temp file for %s.spl -> %s, errno=%s\n", pl->name, tmpname, strerror(errno));
  int ret = LIBMTP_Get_File_To_File_Descriptor(device, pl->playlist_id, fd, NULL, NULL, NULL);
  if( ret < 0 ) {
    
    close(fd);
    printf("FIXME closed\n");
  }

  text_t* p = read_into_spl_text_t(device, fd);
  close(fd);

  
  LIBMTP_folder_t *folders;
  LIBMTP_file_t *files;
  folders = LIBMTP_Get_Folder_List(device);
  files = LIBMTP_Get_Filelisting_With_Callback(device, NULL, NULL);

  
  pl->no_tracks = trackno_spl_text_t(p);
  IF_DEBUG() printf("%u track%s found\n", pl->no_tracks, pl->no_tracks==1?"":"s");
  pl->tracks = malloc(sizeof(uint32_t)*(pl->no_tracks));
  tracks_from_spl_text_t(p, pl->tracks, folders, files);

  free_spl_text_t(p);

  
  IF_DEBUG() printf("------------\n\n");
}


int playlist_t_to_spl(LIBMTP_mtpdevice_t *device,
                      LIBMTP_playlist_t * const pl)
{
  text_t* t;
  LIBMTP_folder_t *folders;
  LIBMTP_file_t *files;
  folders = LIBMTP_Get_Folder_List(device);
  files = LIBMTP_Get_Filelisting_With_Callback(device, NULL, NULL);

  char tmpname[] = "/tmp/mtp-spl2pl-XXXXXX"; 

  IF_DEBUG() printf("pl->name='%s'\n",pl->name);

  
  int fd = mkstemp(tmpname);
  if(fd < 0) {
    printf("failed to make temp file for %s.spl -> %s, errno=%s\n", pl->name, tmpname, strerror(errno));
    return -1;
  }
  
  if(unlink(tmpname) < 0)
    printf("failed to delete temp file for %s.spl -> %s, errno=%s\n", pl->name, tmpname, strerror(errno));

  
  uint32_t ver_major;
  uint32_t ver_minor = 0;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  if(FLAG_PLAYLIST_SPL_V2(ptp_usb)) ver_major = 2;
  else ver_major = 1; 

  IF_DEBUG() printf("%u track%s\n", pl->no_tracks, pl->no_tracks==1?"":"s");
  IF_DEBUG() printf(".spl version %d.%02d\n", ver_major, ver_minor);

  
  spl_text_t_from_tracks(&t, pl->tracks, pl->no_tracks, ver_major, ver_minor, NULL, folders, files);
  write_from_spl_text_t(device, fd, t);
  free_spl_text_t(t); 

  
  LIBMTP_file_t* f = malloc(sizeof(LIBMTP_file_t));
  f->item_id = 0;
  f->parent_id = pl->parent_id;
  f->storage_id = pl->storage_id;
  f->filename = malloc(sizeof(char)*(strlen(pl->name)+5));
  strcpy(f->filename, pl->name);
  strcat(f->filename, ".spl"); 
  f->filesize = lseek(fd, 0, SEEK_CUR); 
  f->filetype = LIBMTP_FILETYPE_UNKNOWN;
  f->next = NULL;

  IF_DEBUG() printf("%s is %dB\n", f->filename, (int)f->filesize);

  
  lseek(fd, 0, SEEK_SET); 
  int ret = LIBMTP_Send_File_From_File_Descriptor(device, fd, f, NULL, NULL);
  pl->playlist_id = f->item_id;
  free(f->filename);
  free(f);

  
  close(fd);
  
  IF_DEBUG() printf("------------\n\n");

  return ret;
}



int update_spl_playlist(LIBMTP_mtpdevice_t *device,
			  LIBMTP_playlist_t * const newlist)
{
  IF_DEBUG() printf("pl->name='%s'\n",newlist->name);

  
  LIBMTP_playlist_t * old = LIBMTP_Get_Playlist(device, newlist->playlist_id);
  
  
  if (!old)
    return -1;

  
  int delta = 0;
  int i;
  if(old->no_tracks != newlist->no_tracks)
    delta++;
  for(i=0;i<newlist->no_tracks && delta==0;i++) {
    if(old->tracks[i] != newlist->tracks[i])
      delta++;
  }

  
  if(delta) {
    IF_DEBUG() printf("new tracks detected:\n");
    IF_DEBUG() printf("delete old playlist and build a new one\n");
    IF_DEBUG() printf(" NOTE: new playlist_id will result!\n");
    if(LIBMTP_Delete_Object(device, old->playlist_id) != 0)
      return -1;

    IF_DEBUG() {
      if(strcmp(old->name,newlist->name) == 0)
        printf("name unchanged\n");
      else
        printf("name is changing too -> %s\n",newlist->name);
    }

    return LIBMTP_Create_New_Playlist(device, newlist);
  }


  
  if(strcmp(old->name,newlist->name) != 0) {
    IF_DEBUG() printf("ONLY name is changing -> %s\n",newlist->name);
    IF_DEBUG() printf("playlist_id will remain unchanged\n");
    char* s = malloc(sizeof(char)*(strlen(newlist->name)+5));
    strcpy(s, newlist->name);
    strcat(s,".spl"); 
    int ret = LIBMTP_Set_Playlist_Name(device, newlist, s);
    free(s);
    return ret;
  }

  IF_DEBUG() printf("no change\n");
  return 0; 
}


static text_t* read_into_spl_text_t(LIBMTP_mtpdevice_t *device, const int fd)
{
  
  const size_t MAXREAD = 1024*2;
  char t[MAXREAD];
  
  
  const size_t WSIZE = MAXREAD/2*3+1;
  char w[WSIZE];
  char* it = t; 
  char* iw = w;
  ssize_t rdcnt;
  off_t offcnt;
  text_t* head = NULL;
  text_t* tail = NULL;
  int eof = 0;

  
  offcnt = lseek(fd, 0, SEEK_SET);

  while(!eof) {
    
    
    
    offcnt = lseek(fd, 0, SEEK_CUR);
    
    
    
    it = t; 
    rdcnt = read(fd, it, sizeof(char)*MAXREAD);
    if(rdcnt < 0)
      printf("load_spl_fd read err %s\n", strerror(errno));
    else if(rdcnt == 0) { 
      if(it-t == MAXREAD)
        printf("error -- buffer too small to read in .spl playlist entry\n");

      rdcnt = lseek(fd, 0, SEEK_CUR) - offcnt;
      eof = 1;
    }

    IF_DEBUG() printf("read buff= {%dB new, %dB old/left-over}%s\n",(int)rdcnt, (int)(iw-w), eof?", EOF":"");

    
    char* it_end = t + rdcnt;
    while(it < it_end) {
      
      if(*it == '\r' || *it == '\n')
        *iw = '\0';
      else
        *iw = *it;

      it++;
      iw++;

      
      if( (iw-w) >= 2 && 
          *(iw-1) == '\0' && *(iw-2) == '\0' && 
          
          
          !((iw-w)%2) ) {

        
        
        if(ucs2_strlen((uint16_t*)w) == 0) {
          iw = w;
          continue;
        }

        
        if(head == NULL) {
          head = malloc(sizeof(text_t));
          tail = head;
        }
        else {
          tail->next = malloc(sizeof(text_t));
          tail = tail->next;
        }
        
        
        tail->text = utf16_to_utf8(device, (uint16_t*) w);
        iw = w; 

        IF_DEBUG() printf("line: %s\n", tail->text);
      }

      
      if(iw >= w + WSIZE) {
        
        
        
        
        printf("ERROR %s:%u:%s(): buffer overflow! .spl line too long @ %zuB\n",
               __FILE__, __LINE__, __func__, WSIZE);
        iw = w; 
      }
    }

    
    
    
    
    
    

  }

  
  
  if(head != NULL)
    tail->next = NULL;

  
  return head;
}


static void write_from_spl_text_t(LIBMTP_mtpdevice_t *device,
                                  const int fd,
                                  text_t* p) {
  ssize_t ret;
  
  ret = write(fd,"\xff\xfe",2);
  while(p != NULL) {
    char *const t = (char*) utf8_to_utf16(device, p->text);
    
    const size_t len = ucs2_strlen((uint16_t*)t)*sizeof(uint16_t);
    int i;

    IF_DEBUG() {
      printf("\nutf8=%s ",p->text);
      for(i=0;i<strlen(p->text);i++)
        printf("%02x ", p->text[i] & 0xff);
      printf("\n");
      printf("ucs2=");
      for(i=0;i<ucs2_strlen((uint16_t*)t)*sizeof(uint16_t);i++)
        printf("%02x ", t[i] & 0xff);
      printf("\n");
    }

    
    ret += write(fd, t, len);

    
    free(t);

    
    if(ret < 0)
      printf("write spl file failed: %s\n", strerror(errno));
    else if(ret != len +2)
      printf("write spl file wrong number of bytes ret=%d len=%d '%s'\n", (int)ret, (int)len, p->text);

    
    ret = write(fd, "\r\0\n\0", 4);
    if(ret < 0)
      printf("write spl file failed: %s\n", strerror(errno));
    else if(ret != 4)
      printf("failed to write the correct number of bytes '\\n'!\n");

    
    ret = 2;

    
    p = p->next;
  }
}

static void free_spl_text_t(text_t* p)
{
  text_t* d;
  while(p != NULL) {
    d = p;
    free(p->text);
    p = p->next;
    free(d);
  }
}

static void print_spl_text_t(text_t* p)
{
  while(p != NULL) {
    printf("%s\n",p->text);
    p = p->next;
  }
}

static uint32_t trackno_spl_text_t(text_t* p) {
  uint32_t c = 0;
  while(p != NULL) {
    if(p->text[0] == '\\' ) c++;
    p = p->next;
  }

  return c;
}

static void tracks_from_spl_text_t(text_t* p,
                                   uint32_t* tracks,
                                   LIBMTP_folder_t* folders,
                                   LIBMTP_file_t* files)
{
  uint32_t c = 0;
  while(p != NULL) {
    if(p->text[0] == '\\' ) {
      tracks[c] = discover_id_from_filepath(p->text, folders, files);
      IF_DEBUG()
        printf("track %d = %s (%u)\n", c+1, p->text, tracks[c]);
      c++;
    }
    p = p->next;
  }
}


static void spl_text_t_from_tracks(text_t** p,
                                   uint32_t* tracks,
                                   const uint32_t trackno,
                                   const uint32_t ver_major,
                                   const uint32_t ver_minor,
                                   char* dnse,
                                   LIBMTP_folder_t* folders,
                                   LIBMTP_file_t* files)
{

  
  text_t* c = NULL;
  append_text_t(&c, "SPL PLAYLIST");
  *p = c; 

  char vs[14]; 
  sprintf(vs,"VERSION %d.%02d",ver_major,ver_minor);

  append_text_t(&c, vs);
  append_text_t(&c, "");

  
  int i;
  char* f;
  for(i=0;i<trackno;i++) {
    discover_filepath_from_id(&f, tracks[i], folders, files);

    if(f != NULL) {
      append_text_t(&c, f);
      IF_DEBUG()
        printf("track %d = %s (%u)\n", i+1, f, tracks[i]);
    }
    else
      printf("failed to find filepath for track=%d\n", tracks[i]);
  }

  
  append_text_t(&c, "");
  append_text_t(&c, "END PLAYLIST");
  if(ver_major == 2) {
    append_text_t(&c, "");
    append_text_t(&c, "myDNSe DATA");
    if(dnse != NULL) {
      append_text_t(&c, dnse);
    }
    else {
      append_text_t(&c, "");
      append_text_t(&c, "");
    }
    append_text_t(&c, "END myDNSe");
  }

  c->next = NULL;

  
  IF_DEBUG() {
    printf(".spl playlist:\n");
    print_spl_text_t(*p);
  }
}



static void discover_filepath_from_id(char** p,
                                      uint32_t track,
                                      LIBMTP_folder_t* folders,
                                      LIBMTP_file_t* files)
{
  
  const int M = 1024;
  char w[M];
  char* iw = w + M; 

  
  *p = NULL;


  
  while(files != NULL && files->item_id != track) {
    files = files->next;
  }
  
  if(files == NULL)
    return;

  
  
  iw = iw - (strlen(files->filename) +1); 
  strcpy(iw,files->filename);

  
  
  uint32_t id = files->parent_id;
  char* f = NULL;
  while(id != 0) {
    find_folder_name(folders, &id, &f);
    if(f == NULL) return; 
    iw = iw - (strlen(f) +1);
    
    strcpy(iw, f);
    iw[strlen(f)] = '\\';
    free(f);
  }

  
  iw--;
  iw[0] = '\\';

  
  *p = strdup(iw);
}


static uint32_t discover_id_from_filepath(const char* s, LIBMTP_folder_t* folders, LIBMTP_file_t* files)
{
  
  if(s[0] != '\\')
    return 0;

  int i;
  uint32_t id = 0;
  char* sc = strdup(s);
  char* sci = sc +1; 
  

  
  size_t len = strlen(s);
  for(i=0;i<len;i++) {
    if(sc[i] == '\\') {
      sc[i] = '\0';
    }
  }

  
  while(sci != sc + len +1) {
    
    if(sci + strlen(sci) == sc + len) {

      while(files != NULL) {
        
        if( (files->parent_id == id) &&
            (strcmp(files->filename, sci) == 0) ) { 
          id = files->item_id;
          break;
        }
        files = files->next;
      }
    }
    else { 
      id = find_folder_id(folders, id, sci);
    }

    
    sci += strlen(sci) +1;
  }

  
  free(sc);

  

  return id;
}



static void find_folder_name(LIBMTP_folder_t* folders, uint32_t* id, char** name)
{

  

  LIBMTP_folder_t* f = LIBMTP_Find_Folder(folders, *id);
  if(f == NULL) {
    *name = NULL;
  }
  else { 
    *name = strdup(f->name);
    *id = f->parent_id;
  }
}


static uint32_t find_folder_id(LIBMTP_folder_t* folders, uint32_t parent, char* name) {

  if(folders == NULL)
    return 0;

  
  else if( (folders->parent_id == parent) &&
           (strcmp(folders->name, name) == 0) )
    return folders->folder_id;

  
  else {
    uint32_t id = 0;

    if(folders->sibling != NULL)
      id = find_folder_id(folders->sibling, parent, name);
    if( (id == 0) && (folders->child != NULL) )
      id = find_folder_id(folders->child, parent, name);

    return id;
  }
}


static void append_text_t(text_t** t, char* s)
{
  if(*t == NULL) {
    *t = malloc(sizeof(text_t));
  }
  else {
    (*t)->next = malloc(sizeof(text_t));
    (*t) = (*t)->next;
  }
  (*t)->text = strdup(s);
}

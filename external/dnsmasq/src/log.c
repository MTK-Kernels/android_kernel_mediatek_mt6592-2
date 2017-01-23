/* dnsmasq is Copyright (c) 2000-2009 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
     
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dnsmasq.h"

#ifdef __ANDROID__
#include <android/log.h>
#endif



#define MAX_MESSAGE 1024

static int log_fac = LOG_DAEMON;
static int log_stderr = 0; 
static int log_fd = -1;
static int log_to_file = 0;
static int entries_alloced = 0;
static int entries_lost = 0;
static int connection_good = 1;
static int max_logs = 0;
static int connection_type = SOCK_DGRAM;

struct log_entry {
  int offset, length;
  pid_t pid; 
  struct log_entry *next;
  char payload[MAX_MESSAGE];
};

static struct log_entry *entries = NULL;
static struct log_entry *free_entries = NULL;


int log_start(struct passwd *ent_pw, int errfd)
{
  int ret = 0;

  log_stderr = !!(daemon->options & OPT_DEBUG);

  if (daemon->log_fac != -1)
    log_fac = daemon->log_fac;
#ifdef LOG_LOCAL0
  else if (daemon->options & OPT_DEBUG)
    log_fac = LOG_LOCAL0;
#endif

  if (daemon->log_file)
    { 
      log_to_file = 1;
      daemon->max_logs = 0;
    }
  
  max_logs = daemon->max_logs;

  if (!log_reopen(daemon->log_file))
    {
      send_event(errfd, EVENT_LOG_ERR, errno);
      _exit(0);
    }

  if (max_logs == 0)
    {  
      free_entries = safe_malloc(sizeof(struct log_entry));
      free_entries->next = NULL;
      entries_alloced = 1;
    }

  if (log_to_file && ent_pw && ent_pw->pw_uid != 0 && 
      fchown(log_fd, ent_pw->pw_uid, -1) != 0)
    ret = errno;

  return ret;
}

int log_reopen(char *log_file)
{
  if (log_fd != -1)
    close(log_fd);

  
     
  if (log_file)
    {
      log_fd = open(log_file, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
      return log_fd != -1;
    }
  else
#if defined(HAVE_SOLARIS_NETWORK) || defined(__ANDROID__)
#   define _PATH_LOG ""  
    log_fd = -1;
#else
    {
       int flags;
       log_fd = socket(AF_UNIX, connection_type, 0);
      
      if (log_fd == -1)
	return 0;
      
      
      if (max_logs != 0 && (flags = fcntl(log_fd, F_GETFL)) != -1)
	fcntl(log_fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif

  return 1;
}

static void free_entry(void)
{
  struct log_entry *tmp = entries;
  entries = tmp->next;
  tmp->next = free_entries;
  free_entries = tmp;
}      

static void log_write(void)
{
  ssize_t rc;
   
  while (entries)
    {
      
      if (entries->pid != getpid())
	{
	  free_entry();
	  continue;
	}

      connection_good = 1;

      if ((rc = write(log_fd, entries->payload + entries->offset, entries->length)) != -1)
	{
	  entries->length -= rc;
	  entries->offset += rc;
	  if (entries->length == 0)
	    {
	      free_entry();
	      if (entries_lost != 0)
		{
		  int e = entries_lost;
		  entries_lost = 0; 
		  my_syslog(LOG_WARNING, _("overflow: %d log entries lost"), e);
		}	  
	    }
	  continue;
	}
      
      if (errno == EINTR)
	continue;

      if (errno == EAGAIN)
	return; 
      
      if (errno == ENOBUFS)
	{
	  connection_good = 0;
	  return;
	}

       
      if (!log_to_file)
	{
	  if (errno == EPIPE)
	    {
	      if (log_reopen(NULL))
		continue;
	    }
	  else if (errno == ECONNREFUSED || 
		   errno == ENOTCONN || 
		   errno == EDESTADDRREQ || 
		   errno == ECONNRESET)
	    {
	      
	      struct sockaddr_un logaddr;
	      
#ifdef HAVE_SOCKADDR_SA_LEN
	      logaddr.sun_len = sizeof(logaddr) - sizeof(logaddr.sun_path) + strlen(_PATH_LOG) + 1; 
#endif
	      logaddr.sun_family = AF_UNIX;
	      strncpy(logaddr.sun_path, _PATH_LOG, sizeof(logaddr.sun_path));
	      
	      
	      if (connect(log_fd, (struct sockaddr *)&logaddr, sizeof(logaddr)) != -1)
		continue;
	      
	      
	      if (errno == ENOENT || 
		  errno == EALREADY || 
		  errno == ECONNREFUSED ||
		  errno == EISCONN || 
		  errno == EINTR ||
		  errno == EAGAIN)
		{
		  
		  connection_good = 0;
		  return;
		}
	      
	      
	      if (errno == EPROTOTYPE)
		{
		  connection_type = connection_type == SOCK_DGRAM ? SOCK_STREAM : SOCK_DGRAM;
		  if (log_reopen(NULL))
		    continue;
		}
	    }
	}

      log_fd = -1;
      my_syslog(LOG_CRIT, _("log failed: %s"), strerror(errno));
      return;
    }
}

void my_syslog(int priority, const char *format, ...)
{
  va_list ap;
  struct log_entry *entry;
  time_t time_now;
  char *p;
  size_t len;
  pid_t pid = getpid();
  char *func = "";
#ifdef __ANDROID__
  int alog_lvl;
#endif

  if ((LOG_FACMASK & priority) == MS_TFTP)
    func = "-tftp";
  else if ((LOG_FACMASK & priority) == MS_DHCP)
    func = "-dhcp";
      
  priority = LOG_PRI(priority);
  
  if (log_stderr) 
    {
      fprintf(stderr, "dnsmasq%s: ", func);
      va_start(ap, format);
      vfprintf(stderr, format, ap);
      va_end(ap);
      fputc('\n', stderr);
    }

#ifdef __ANDROID__
    if (priority <= LOG_ERR)
      alog_lvl = ANDROID_LOG_ERROR;
    else if (priority == LOG_WARNING)
      alog_lvl = ANDROID_LOG_WARN;
    else if (priority <= LOG_INFO)
      alog_lvl = ANDROID_LOG_INFO;
    else
      alog_lvl = ANDROID_LOG_DEBUG;
    va_start(ap, format);
    __android_log_vprint(alog_lvl, "dnsmasq", format, ap);
    va_end(ap);
#else

  if (log_fd == -1)
    {
      
      static int isopen = 0;
      if (!isopen)
	{
	  openlog("dnsmasq", LOG_PID, log_fac);
	  isopen = 1;
	}
      va_start(ap, format);  
      vsyslog(priority, format, ap);
      va_end(ap);
      return;
    }
  
  if ((entry = free_entries))
    free_entries = entry->next;
  else if (entries_alloced < max_logs && (entry = malloc(sizeof(struct log_entry))))
    entries_alloced++;
  
  if (!entry)
    entries_lost++;
  else
    {
      
      entry->next = NULL;
      if (!entries)
	entries = entry;
      else
	{
	  struct log_entry *tmp;
	  for (tmp = entries; tmp->next; tmp = tmp->next);
	  tmp->next = entry;
	}
      
      time(&time_now);
      p = entry->payload;
      if (!log_to_file)
	p += sprintf(p, "<%d>", priority | log_fac);

      p += sprintf(p, "%.15s dnsmasq%s[%d]: ", ctime(&time_now) + 4, func, (int)pid);
        
      len = p - entry->payload;
      va_start(ap, format);  
      len += vsnprintf(p, MAX_MESSAGE - len, format, ap) + 1; 
      va_end(ap);
      entry->length = len > MAX_MESSAGE ? MAX_MESSAGE : len;
      entry->offset = 0;
      entry->pid = pid;

      
      if (log_to_file)
	entry->payload[entry->length - 1] = '\n';
    }
  
  log_write();
  

  if (entries && max_logs != 0)
    {
      int d;
      
      for (d = 0,entry = entries; entry; entry = entry->next, d++);
      
      if (d == max_logs)
	d = 0;
      else if (max_logs > 8)
	d -= max_logs - 8;

      if (d > 0)
	{
	  struct timespec waiter;
	  waiter.tv_sec = 0;
	  waiter.tv_nsec = 1000000 << (d - 1); 
	  nanosleep(&waiter, NULL);
      
	  
	  log_write();
	}
    }
#endif
}

void set_log_writer(fd_set *set, int *maxfdp)
{
  if (entries && log_fd != -1 && connection_good)
    {
      FD_SET(log_fd, set);
      bump_maxfd(log_fd, maxfdp);
    }
}

void check_log_writer(fd_set *set)
{
  if (log_fd != -1 && (!set || FD_ISSET(log_fd, set)))
    log_write();
}

void flush_log(void)
{
  
  if (log_fd != -1)
    {
      int flags;
      if ((flags = fcntl(log_fd, F_GETFL)) != -1)
	fcntl(log_fd, F_SETFL, flags & ~O_NONBLOCK);
      log_write();
      close(log_fd);
    }
}

void die(char *message, char *arg1, int exit_code)
{
  char *errmess = strerror(errno);
  
  if (!arg1)
    arg1 = errmess;

  log_stderr = 1; 
  fputc('\n', stderr); 
  my_syslog(LOG_CRIT, message, arg1, errmess);
  
  log_stderr = 0;
  my_syslog(LOG_CRIT, _("FAILED to start up"));
  flush_log();
  
  exit(exit_code);
}

/* Test child for parent backtrace test.
   Copyright (C) 2013 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#include <config.h>
#include <assert.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#ifndef __linux__

int
main (int argc __attribute__ ((unused)), char **argv)
{
  fprintf (stderr, "%s: Unwinding not supported for this architecture\n",
           argv[0]);
  return 77;
}

#else 

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define NOINLINE_NOCLONE __attribute__ ((noinline, noclone))
#else
#define NOINLINE_NOCLONE __attribute__ ((noinline))
#endif

#define NORETURN __attribute__ ((noreturn))
#define UNUSED __attribute__ ((unused))
#define USED __attribute__ ((used))

static int ptraceme, gencore;


static NOINLINE_NOCLONE void
sigusr2 (int signo)
{
  assert (signo == SIGUSR2);
  if (! gencore)
    {
      raise (SIGUSR1);
      pthread_exit (NULL);
      
      abort ();
    }
  
  raise (SIGABRT);
  
  asm volatile ("");
}

static NOINLINE_NOCLONE void
dummy1 (void)
{
  asm volatile ("");
}

#ifdef __x86_64__
static NOINLINE_NOCLONE USED void
jmp (void)
{
  
  abort ();
}
#endif

static NOINLINE_NOCLONE void
dummy2 (void)
{
  asm volatile ("");
}

static NOINLINE_NOCLONE NORETURN void
stdarg (int f UNUSED, ...)
{
  sighandler_t sigusr2_orig = signal (SIGUSR2, sigusr2);
  assert (sigusr2_orig == SIG_DFL);
  errno = 0;
  if (ptraceme)
    {
      long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
      assert_perror (errno);
      assert (l == 0);
    }
#ifdef __x86_64__
  if (! gencore)
    {
      
      raise (SIGUSR1);
    }
#endif
  sigusr2 (SIGUSR2);
  
  abort ();
}

static NOINLINE_NOCLONE void
dummy3 (void)
{
  asm volatile ("");
}

static NOINLINE_NOCLONE void
backtracegen (void)
{
  stdarg (1);
}

static NOINLINE_NOCLONE void
dummy4 (void)
{
  asm volatile ("");
}

static void *
start (void *arg UNUSED)
{
  backtracegen ();
  
  abort ();
}

int
main (int argc UNUSED, char **argv)
{
  setbuf (stdout, NULL);
  assert (*argv++);
  ptraceme = (*argv && strcmp (*argv, "--ptraceme") == 0);
  argv += ptraceme;
  gencore = (*argv && strcmp (*argv, "--gencore") == 0);
  argv += gencore;
  assert (!*argv);
  dummy1 ();
  dummy2 ();
  dummy3 ();
  dummy4 ();
  if (gencore)
    printf ("%ld\n", (long) getpid ());
  pthread_t thread;
  int i = pthread_create (&thread, NULL, start, NULL);
  
  assert (i == 0);
  if (ptraceme)
    {
      errno = 0;
      long l = ptrace (PTRACE_TRACEME, 0, NULL, NULL);
      assert_perror (errno);
      assert (l == 0);
    }
  if (gencore)
    pthread_join (thread, NULL);
  else
    raise (SIGUSR2);
  return 0;
}

#endif 


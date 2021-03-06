// Copyright 2013 Google Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license

#ifndef VPX_THREAD_H_
#define VPX_THREAD_H_

#include "./vpx_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_DECODE_THREADS 8

#if CONFIG_MULTITHREAD

#if defined(_WIN32) && !HAVE_PTHREAD_H
#include <errno.h>  
#include <process.h>  
#include <windows.h>  
typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_mutex_t;
typedef struct {
  HANDLE waiting_sem_;
  HANDLE received_sem_;
  HANDLE signal_event_;
} pthread_cond_t;


#define THREADFN unsigned int __stdcall
#define THREAD_RETURN(val) (unsigned int)((DWORD_PTR)val)

static INLINE int pthread_create(pthread_t* const thread, const void* attr,
                                 unsigned int (__stdcall *start)(void*),
                                 void* arg) {
  (void)attr;
  *thread = (pthread_t)_beginthreadex(NULL,   
                                      0,      
                                      start,
                                      arg,
                                      0,      
                                      NULL);  
  if (*thread == NULL) return 1;
  SetThreadPriority(*thread, THREAD_PRIORITY_ABOVE_NORMAL);
  return 0;
}

static INLINE int pthread_join(pthread_t thread, void** value_ptr) {
  (void)value_ptr;
  return (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0 ||
          CloseHandle(thread) == 0);
}

static INLINE int pthread_mutex_init(pthread_mutex_t *const mutex,
                                     void* mutexattr) {
  (void)mutexattr;
  InitializeCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_trylock(pthread_mutex_t *const mutex) {
  return TryEnterCriticalSection(mutex) ? 0 : EBUSY;
}

static INLINE int pthread_mutex_lock(pthread_mutex_t *const mutex) {
  EnterCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_unlock(pthread_mutex_t *const mutex) {
  LeaveCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_mutex_destroy(pthread_mutex_t *const mutex) {
  DeleteCriticalSection(mutex);
  return 0;
}

static INLINE int pthread_cond_destroy(pthread_cond_t *const condition) {
  int ok = 1;
  ok &= (CloseHandle(condition->waiting_sem_) != 0);
  ok &= (CloseHandle(condition->received_sem_) != 0);
  ok &= (CloseHandle(condition->signal_event_) != 0);
  return !ok;
}

static INLINE int pthread_cond_init(pthread_cond_t *const condition,
                                    void* cond_attr) {
  (void)cond_attr;
  condition->waiting_sem_ = CreateSemaphore(NULL, 0, MAX_DECODE_THREADS, NULL);
  condition->received_sem_ = CreateSemaphore(NULL, 0, MAX_DECODE_THREADS, NULL);
  condition->signal_event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (condition->waiting_sem_ == NULL ||
      condition->received_sem_ == NULL ||
      condition->signal_event_ == NULL) {
    pthread_cond_destroy(condition);
    return 1;
  }
  return 0;
}

static INLINE int pthread_cond_signal(pthread_cond_t *const condition) {
  int ok = 1;
  if (WaitForSingleObject(condition->waiting_sem_, 0) == WAIT_OBJECT_0) {
    
    ok = SetEvent(condition->signal_event_);
    
    
    ok &= (WaitForSingleObject(condition->received_sem_, INFINITE) !=
           WAIT_OBJECT_0);
  }
  return !ok;
}

static INLINE int pthread_cond_wait(pthread_cond_t *const condition,
                                    pthread_mutex_t *const mutex) {
  int ok;
  
  
  if (!ReleaseSemaphore(condition->waiting_sem_, 1, NULL))
    return 1;
  
  pthread_mutex_unlock(mutex);
  ok = (WaitForSingleObject(condition->signal_event_, INFINITE) ==
        WAIT_OBJECT_0);
  ok &= ReleaseSemaphore(condition->received_sem_, 1, NULL);
  pthread_mutex_lock(mutex);
  return !ok;
}
#else  
#include <pthread.h> 
# define THREADFN void*
# define THREAD_RETURN(val) val
#endif

#endif  

typedef enum {
  NOT_OK = 0,   
  OK,           
  WORK          
} VPxWorkerStatus;

typedef int (*VPxWorkerHook)(void*, void*);

typedef struct VPxWorkerImpl VPxWorkerImpl;

typedef struct {
  VPxWorkerImpl *impl_;
  VPxWorkerStatus status_;
  VPxWorkerHook hook;     
  void *data1;            
  void *data2;            
  int had_error;          
} VPxWorker;

typedef struct {
  
  void (*init)(VPxWorker *const worker);
  
  
  int (*reset)(VPxWorker *const worker);
  
  
  int (*sync)(VPxWorker *const worker);
  
  
  
  void (*launch)(VPxWorker *const worker);
  
  
  
  
  void (*execute)(VPxWorker *const worker);
  
  
  void (*end)(VPxWorker *const worker);
} VPxWorkerInterface;

int vpx_set_worker_interface(const VPxWorkerInterface *const winterface);

const VPxWorkerInterface *vpx_get_worker_interface(void);


#ifdef __cplusplus
}    
#endif

#endif  

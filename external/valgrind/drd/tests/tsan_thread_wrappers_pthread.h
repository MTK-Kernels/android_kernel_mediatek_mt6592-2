/*
  This file is part of Valgrind, a dynamic binary instrumentation
  framework.

  Copyright (C) 2008-2008 Google Inc
     opensource@google.com

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307, USA.

  The GNU General Public License is contained in the file COPYING.
*/


#ifndef THREAD_WRAPPERS_PTHREAD_H
#define THREAD_WRAPPERS_PTHREAD_H

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <queue>
#include <stdio.h>
#include <limits.h>   

#ifdef VGO_darwin
#include <libkern/OSAtomic.h>
#define NO_BARRIER
#define NO_TLS
#endif

#include <string>
using namespace std;

#include <sys/time.h>
#include <time.h>

#include "../../drd/drd.h"
#define ANNOTATE_NO_OP(arg) do { } while(0)
#define ANNOTATE_EXPECT_RACE(addr, descr)                \
    ANNOTATE_BENIGN_RACE_SIZED(addr, 4, "expected race")
static inline bool RunningOnValgrind() { return RUNNING_ON_VALGRIND; }

#include <assert.h>
#ifdef NDEBUG
# error "Pleeease, do not define NDEBUG"
#endif
#define CHECK assert

const bool kMallocUsesMutex = false;

static inline int64_t GetCurrentTimeMillis() {
  struct timeval now;
  gettimeofday(&now, NULL);
  return now.tv_sec * 1000 + now.tv_usec / 1000;
}

static inline void timeval2timespec(timeval *const tv,
                                     timespec *ts,
                                     int64_t offset_milli) {
  const int64_t ten_9 = 1000000000LL;
  const int64_t ten_6 = 1000000LL;
  const int64_t ten_3 = 1000LL;
  int64_t now_nsec = (int64_t)tv->tv_sec * ten_9;
  now_nsec += (int64_t)tv->tv_usec * ten_3;
  int64_t then_nsec = now_nsec + offset_milli * ten_6;
  ts->tv_sec  = then_nsec / ten_9;
  ts->tv_nsec = then_nsec % ten_9;
}


class CondVar;

#ifndef NO_SPINLOCK

#ifndef VGO_darwin
class SpinLock {
 public:
  SpinLock() {
    CHECK(0 == pthread_spin_init(&mu_, 0));
    ANNOTATE_RWLOCK_CREATE((void*)&mu_);
  }
  ~SpinLock() {
    ANNOTATE_RWLOCK_DESTROY((void*)&mu_);
    CHECK(0 == pthread_spin_destroy(&mu_));
  }
  void Lock() {
    CHECK(0 == pthread_spin_lock(&mu_));
    ANNOTATE_RWLOCK_ACQUIRED((void*)&mu_, 1);
  }
  void Unlock() {
    ANNOTATE_RWLOCK_RELEASED((void*)&mu_, 1);
    CHECK(0 == pthread_spin_unlock(&mu_));
  }
 private:
  pthread_spinlock_t mu_;
};

#else

class SpinLock {
 public:
  
  SpinLock() : mu_(OS_SPINLOCK_INIT) {
    ANNOTATE_RWLOCK_CREATE((void*)&mu_);
  }
  ~SpinLock() {
    ANNOTATE_RWLOCK_DESTROY((void*)&mu_);
  }
  void Lock() {
    OSSpinLockLock(&mu_);
    ANNOTATE_RWLOCK_ACQUIRED((void*)&mu_, 1);
  }
  void Unlock() {
    ANNOTATE_RWLOCK_RELEASED((void*)&mu_, 1);
    OSSpinLockUnlock(&mu_);
  }
 private:
  OSSpinLock mu_;
};
#endif 

#endif 

class Condition {
 public:
  typedef bool (*func_t)(void*);

  template <typename T>
  Condition(bool (*func)(T*), T* arg)
  : func_(reinterpret_cast<func_t>(func)), arg_(arg) {}

  Condition(bool (*func)())
  : func_(reinterpret_cast<func_t>(func)), arg_(NULL) {}

  bool Eval() { return func_(arg_); }
 private:
  func_t func_;
  void *arg_;

};


class Mutex {
  friend class CondVar;
 public:
  Mutex() {
    CHECK(0 == pthread_mutex_init(&mu_, NULL));
    CHECK(0 == pthread_cond_init(&cv_, NULL));
    signal_at_unlock_ = true;  
                               
  }
  ~Mutex() {
    CHECK(0 == pthread_cond_destroy(&cv_));
    CHECK(0 == pthread_mutex_destroy(&mu_));
  }
  void Lock()          { CHECK(0 == pthread_mutex_lock(&mu_));}
  bool TryLock()       { return (0 == pthread_mutex_trylock(&mu_));}
  void Unlock() {
    if (signal_at_unlock_) {
      CHECK(0 == pthread_cond_signal(&cv_));
    }
    CHECK(0 == pthread_mutex_unlock(&mu_));
  }
  void ReaderLock()    { Lock(); }
  bool ReaderTryLock() { return TryLock();}
  void ReaderUnlock()  { Unlock(); }

  void LockWhen(Condition cond)            { Lock(); WaitLoop(cond); }
  void ReaderLockWhen(Condition cond)      { Lock(); WaitLoop(cond); }
  void Await(Condition cond)               { WaitLoop(cond); }

  bool ReaderLockWhenWithTimeout(Condition cond, int millis)
    { Lock(); return WaitLoopWithTimeout(cond, millis); }
  bool LockWhenWithTimeout(Condition cond, int millis)
    { Lock(); return WaitLoopWithTimeout(cond, millis); }
  bool AwaitWithTimeout(Condition cond, int millis)
    { return WaitLoopWithTimeout(cond, millis); }

 private:

  void WaitLoop(Condition cond) {
    signal_at_unlock_ = true;
    while(cond.Eval() == false) {
      pthread_cond_wait(&cv_, &mu_);
    }
    ANNOTATE_CONDVAR_LOCK_WAIT(&cv_, &mu_);
  }

  bool WaitLoopWithTimeout(Condition cond, int millis) {
    struct timeval now;
    struct timespec timeout;
    int retcode = 0;
    gettimeofday(&now, NULL);
    timeval2timespec(&now, &timeout, millis);

    signal_at_unlock_ = true;
    while (cond.Eval() == false && retcode == 0) {
      retcode = pthread_cond_timedwait(&cv_, &mu_, &timeout);
    }
    if(retcode == 0) {
      ANNOTATE_CONDVAR_LOCK_WAIT(&cv_, &mu_);
    }
    return cond.Eval();
  }

  
  
  
  pthread_cond_t  cv_;
  pthread_mutex_t mu_;
  bool            signal_at_unlock_;  
};


class MutexLock {  
 public:
  MutexLock(Mutex *mu)
    : mu_(mu) {
    mu_->Lock();
  }
  ~MutexLock() {
    mu_->Unlock();
  }
 private:
  Mutex *mu_;
};


class CondVar {
 public:
  CondVar()   { CHECK(0 == pthread_cond_init(&cv_, NULL)); }
  ~CondVar()  { CHECK(0 == pthread_cond_destroy(&cv_)); }
  void Wait(Mutex *mu) { CHECK(0 == pthread_cond_wait(&cv_, &mu->mu_)); }
  bool WaitWithTimeout(Mutex *mu, int millis) {
    struct timeval now;
    struct timespec timeout;
    gettimeofday(&now, NULL);
    timeval2timespec(&now, &timeout, millis);
    return 0 != pthread_cond_timedwait(&cv_, &mu->mu_, &timeout);
  }
  void Signal() { CHECK(0 == pthread_cond_signal(&cv_)); }
  void SignalAll() { CHECK(0 == pthread_cond_broadcast(&cv_)); }
 private:
  pthread_cond_t cv_;
};


#define NEEDS_SEPERATE_RW_LOCK
class RWLock {
 public:
  RWLock() { CHECK(0 == pthread_rwlock_init(&mu_, NULL)); }
  ~RWLock() { CHECK(0 == pthread_rwlock_destroy(&mu_)); }
  void Lock() { CHECK(0 == pthread_rwlock_wrlock(&mu_)); }
  void ReaderLock() { CHECK(0 == pthread_rwlock_rdlock(&mu_)); }
  void Unlock() { CHECK(0 == pthread_rwlock_unlock(&mu_)); }
  void ReaderUnlock() { CHECK(0 == pthread_rwlock_unlock(&mu_)); }
 private:
  pthread_cond_t dummy; 
  pthread_rwlock_t mu_;
};

class ReaderLockScoped {  
 public:
  ReaderLockScoped(RWLock *mu)
    : mu_(mu) {
    mu_->ReaderLock();
  }
  ~ReaderLockScoped() {
    mu_->ReaderUnlock();
  }
 private:
  RWLock *mu_;
};

class WriterLockScoped {  
 public:
  WriterLockScoped(RWLock *mu)
    : mu_(mu) {
    mu_->Lock();
  }
  ~WriterLockScoped() {
    mu_->Unlock();
  }
 private:
  RWLock *mu_;
};




class MyThread {
 public:
  typedef void *(*worker_t)(void*);

  MyThread(worker_t worker, void *arg = NULL, const char *name = NULL)
      :w_(worker), arg_(arg), name_(name) {}
  MyThread(void (*worker)(void), void *arg = NULL, const char *name = NULL)
      :w_(reinterpret_cast<worker_t>(worker)), arg_(arg), name_(name) {}
  MyThread(void (*worker)(void *), void *arg = NULL, const char *name = NULL)
      :w_(reinterpret_cast<worker_t>(worker)), arg_(arg), name_(name) {}

  ~MyThread(){ w_ = NULL; arg_ = NULL;}
  void Start() { CHECK(0 == pthread_create(&t_, NULL, (worker_t)ThreadBody, this));}
  void Join()  { CHECK(0 == pthread_join(t_, NULL));}
  pthread_t tid() const { return t_; }
 private:
  static void ThreadBody(MyThread *my_thread) {
    if (my_thread->name_) {
      ANNOTATE_THREAD_NAME(my_thread->name_);
    }
    my_thread->w_(my_thread->arg_);
  }
  pthread_t t_;
  worker_t  w_;
  void     *arg_;
  const char *name_;
};


class ProducerConsumerQueue {
 public:
  ProducerConsumerQueue(int unused) {
    
  }
  ~ProducerConsumerQueue() {
    CHECK(q_.empty());
    
  }

  
  void Put(void *item) {
    mu_.Lock();
      q_.push(item);
      ANNOTATE_CONDVAR_SIGNAL(&mu_); 
      
    mu_.Unlock();
  }

  
  
  void *Get() {
    mu_.LockWhen(Condition(IsQueueNotEmpty, &q_));
      void * item = NULL;
      bool ok = TryGetInternal(&item);
      CHECK(ok);
    mu_.Unlock();
    return item;
  }

  
  
  
  bool TryGet(void **res) {
    mu_.Lock();
      bool ok = TryGetInternal(res);
    mu_.Unlock();
    return ok;
  }

 private:
  Mutex mu_;
  std::queue<void*> q_; 

  
  bool TryGetInternal(void ** item_ptr) {
    if (q_.empty())
      return false;
    *item_ptr = q_.front();
    q_.pop();
    
    return true;
  }

  static bool IsQueueNotEmpty(std::queue<void*> * queue) {
     return !queue->empty();
  }
};



struct Closure {
  typedef void (*F0)();
  typedef void (*F1)(void *arg1);
  typedef void (*F2)(void *arg1, void *arg2);
  int  n_params;
  void *f;
  void *param1;
  void *param2;

  void Execute() {
    if (n_params == 0) {
      (F0(f))();
    } else if (n_params == 1) {
      (F1(f))(param1);
    } else {
      CHECK(n_params == 2);
      (F2(f))(param1, param2);
    }
    delete this;
  }
};

Closure *NewCallback(void (*f)()) {
  Closure *res = new Closure;
  res->n_params = 0;
  res->f = (void*)(f);
  res->param1 = NULL;
  res->param2 = NULL;
  return res;
}

template <class P1>
Closure *NewCallback(void (*f)(P1), P1 p1) {
  CHECK(sizeof(P1) <= sizeof(void*));
  Closure *res = new Closure;
  res->n_params = 1;
  res->f = (void*)(f);
  res->param1 = (void*)p1;
  res->param2 = NULL;
  return res;
}

template <class T, class P1, class P2>
Closure *NewCallback(void (*f)(P1, P2), P1 p1, P2 p2) {
  CHECK(sizeof(P1) <= sizeof(void*));
  Closure *res = new Closure;
  res->n_params = 2;
  res->f = (void*)(f);
  res->param1 = (void*)p1;
  res->param2 = (void*)p2;
  return res;
}

class ThreadPool {
 public:
  
  explicit ThreadPool(int n_threads)
    : queue_(INT_MAX) {
    for (int i = 0; i < n_threads; i++) {
      MyThread *thread = new MyThread(&ThreadPool::Worker, this);
      workers_.push_back(thread);
    }
  }

  
  void StartWorkers() {
    for (size_t i = 0; i < workers_.size(); i++) {
      workers_[i]->Start();
    }
  }

  
  void Add(Closure *closure) {
    queue_.Put(closure);
  }

  int num_threads() { return workers_.size();}

  
  ~ThreadPool() {
    for (size_t i = 0; i < workers_.size(); i++) {
      Add(NULL);
    }
    for (size_t i = 0; i < workers_.size(); i++) {
      workers_[i]->Join();
      delete workers_[i];
    }
  }
 private:
  std::vector<MyThread*>   workers_;
  ProducerConsumerQueue  queue_;

  static void *Worker(void *p) {
    ThreadPool *pool = reinterpret_cast<ThreadPool*>(p);
    while (true) {
      Closure *closure = reinterpret_cast<Closure*>(pool->queue_.Get());
      if(closure == NULL) {
        return NULL;
      }
      closure->Execute();
    }
  }
};

#ifndef NO_BARRIER
class Barrier{
 public:
  explicit Barrier(int n_threads) {CHECK(0 == pthread_barrier_init(&b_, 0, n_threads));}
  ~Barrier()                      {CHECK(0 == pthread_barrier_destroy(&b_));}
  void Block() {
    
    
    
    pthread_barrier_wait(&b_);
    
  }
 private:
  pthread_barrier_t b_;
};

#endif 

class BlockingCounter {
 public:
  explicit BlockingCounter(int initial_count) :
    count_(initial_count) {}
  bool DecrementCount() {
    MutexLock lock(&mu_);
    count_--;
    return count_ == 0;
  }
  void Wait() {
    mu_.LockWhen(Condition(&IsZero, &count_));
    mu_.Unlock();
  }
 private:
  static bool IsZero(int *arg) { return *arg == 0; }
  Mutex mu_;
  int count_;
};

int AtomicIncrement(volatile int *value, int increment);

#ifndef VGO_darwin
inline int AtomicIncrement(volatile int *value, int increment) {
  return __sync_add_and_fetch(value, increment);
}

#else
inline int AtomicIncrement(volatile int *value, int increment) {
  return OSAtomicAdd32(increment, value);
}

#define memalign(A,B) malloc(B)

int posix_memalign(void **out, size_t al, size_t size) {
  *out = memalign(al, size);
  return (*out == 0);
}
#endif 

#endif 

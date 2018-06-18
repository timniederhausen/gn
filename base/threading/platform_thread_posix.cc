// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "base/logging.h"
#include "util/build_config.h"

#if defined(OS_LINUX)
#include <sys/syscall.h>
#endif

#include <sys/resource.h>

namespace base {

void InitThreading();
void TerminateOnThread();
size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes);

namespace {

struct ThreadParams {
  ThreadParams() : delegate(nullptr), joinable(false) {}

  PlatformThread::Delegate* delegate;
  bool joinable;
};

void* ThreadFunc(void* params) {
  PlatformThread::Delegate* delegate = nullptr;

  {
    std::unique_ptr<ThreadParams> thread_params(
        static_cast<ThreadParams*>(params));

    delegate = thread_params->delegate;
  }

  delegate->ThreadMain();

  base::TerminateOnThread();
  return nullptr;
}

bool CreateThread(size_t stack_size,
                  bool joinable,
                  PlatformThread::Delegate* delegate,
                  PlatformThreadHandle* thread_handle) {
  DCHECK(thread_handle);
  base::InitThreading();

  pthread_attr_t attributes;
  pthread_attr_init(&attributes);

  // Pthreads are joinable by default, so only specify the detached
  // attribute if the thread should be non-joinable.
  if (!joinable)
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);

  // Get a better default if available.
  if (stack_size == 0)
    stack_size = base::GetDefaultThreadStackSize(attributes);

  if (stack_size > 0)
    pthread_attr_setstacksize(&attributes, stack_size);

  std::unique_ptr<ThreadParams> params(new ThreadParams);
  params->delegate = delegate;
  params->joinable = joinable;

  pthread_t handle;
  int err = pthread_create(&handle, &attributes, ThreadFunc, params.get());
  bool success = !err;
  if (success) {
    // ThreadParams should be deleted on the created thread after used.
    ignore_result(params.release());
  } else {
    // Value of |handle| is undefined if pthread_create fails.
    handle = 0;
    errno = err;
    PLOG(ERROR) << "pthread_create";
  }
  *thread_handle = PlatformThreadHandle(handle);

  pthread_attr_destroy(&attributes);

  return success;
}

}  // namespace

// static
PlatformThreadId PlatformThread::CurrentId() {
// Pthreads doesn't have the concept of a thread ID, so we have to reach down
// into the kernel.
#if defined(OS_MACOSX)
  return pthread_mach_thread_np(pthread_self());
#elif defined(OS_LINUX)
  return syscall(__NR_gettid);
#else
#error
#endif
}

// static
PlatformThreadRef PlatformThread::CurrentRef() {
  return PlatformThreadRef(pthread_self());
}

// static
PlatformThreadHandle PlatformThread::CurrentHandle() {
  return PlatformThreadHandle(pthread_self());
}

// static
void PlatformThread::YieldCurrentThread() {
  sched_yield();
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  struct timespec sleep_time, remaining;

  // Break the duration into seconds and nanoseconds.
  // NOTE: TimeDelta's microseconds are int64s while timespec's
  // nanoseconds are longs, so this unpacking must prevent overflow.
  sleep_time.tv_sec = duration.InSeconds();
  duration -= TimeDelta::FromSeconds(sleep_time.tv_sec);
  sleep_time.tv_nsec = duration.InMicroseconds() * 1000;  // nanoseconds

  while (nanosleep(&sleep_time, &remaining) == -1 && errno == EINTR)
    sleep_time = remaining;
}

// static
bool PlatformThread::Create(size_t stack_size,
                            Delegate* delegate,
                            PlatformThreadHandle* thread_handle) {
  return CreateThread(stack_size, true /* joinable thread */, delegate,
                      thread_handle);
}

// static
bool PlatformThread::CreateNonJoinable(size_t stack_size, Delegate* delegate) {
  PlatformThreadHandle unused;

  bool result = CreateThread(stack_size, false /* non-joinable thread */,
                             delegate, &unused);
  return result;
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  CHECK_EQ(0, pthread_join(thread_handle.platform_handle(), nullptr));
}

// static
void PlatformThread::Detach(PlatformThreadHandle thread_handle) {
  CHECK_EQ(0, pthread_detach(thread_handle.platform_handle()));
}

}  // namespace base

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"

#include <windows.h>

namespace base {

namespace {

// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD kVCThreadNameException = 0x406D1388;

typedef struct tagTHREADNAME_INFO {
  DWORD dwType;      // Must be 0x1000.
  LPCSTR szName;     // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;     // Reserved for future use, must be zero.
} THREADNAME_INFO;

// The SetThreadDescription API was brought in version 1607 of Windows 10.
typedef HRESULT(WINAPI* SetThreadDescription)(HANDLE hThread,
                                              PCWSTR lpThreadDescription);

// This function has try handling, so it is separated out of its caller.
void SetNameInternal(PlatformThreadId thread_id, const char* name) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = thread_id;
  info.dwFlags = 0;

  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info) / sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR*>(&info));
  } __except (EXCEPTION_CONTINUE_EXECUTION) {
  }
}

struct ThreadParams {
  PlatformThread::Delegate* delegate;
  bool joinable;
};

DWORD __stdcall ThreadFunc(void* params) {
  ThreadParams* thread_params = static_cast<ThreadParams*>(params);
  PlatformThread::Delegate* delegate = thread_params->delegate;

  // Retrieve a copy of the thread handle to use as the key in the
  // thread name mapping.
  PlatformThreadHandle::Handle platform_handle;
  BOOL did_dup = DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                                 GetCurrentProcess(), &platform_handle, 0,
                                 FALSE, DUPLICATE_SAME_ACCESS);

  win::ScopedHandle scoped_platform_handle;

  if (did_dup) {
    scoped_platform_handle.Set(platform_handle);
  }

  delete thread_params;
  delegate->ThreadMain();

  return 0;
}

// CreateThreadInternal() matches PlatformThread::CreateWithPriority(), except
// that |out_thread_handle| may be nullptr, in which case a non-joinable thread
// is created.
bool CreateThreadInternal(size_t stack_size,
                          PlatformThread::Delegate* delegate,
                          PlatformThreadHandle* out_thread_handle) {
  unsigned int flags = 0;
  if (stack_size > 0) {
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
  }

  ThreadParams* params = new ThreadParams;
  params->delegate = delegate;
  params->joinable = out_thread_handle != nullptr;

  void* thread_handle;
  {
    // Using CreateThread here vs _beginthreadex makes thread creation a bit
    // faster and doesn't require the loader lock to be available.  Our code
    // will  have to work running on CreateThread() threads anyway, since we run
    // code on the Windows thread pool, etc.  For some background on the
    // difference:
    //   http://www.microsoft.com/msj/1099/win32/win321099.aspx
    thread_handle =
        ::CreateThread(nullptr, stack_size, ThreadFunc, params, flags, nullptr);
  }

  if (!thread_handle) {
    delete params;
    return false;
  }

  if (out_thread_handle)
    *out_thread_handle = PlatformThreadHandle(thread_handle);
  else
    CloseHandle(thread_handle);
  return true;
}

}  // namespace

// static
PlatformThreadId PlatformThread::CurrentId() {
  return ::GetCurrentThreadId();
}

// static
PlatformThreadRef PlatformThread::CurrentRef() {
  return PlatformThreadRef(::GetCurrentThreadId());
}

// static
PlatformThreadHandle PlatformThread::CurrentHandle() {
  return PlatformThreadHandle(::GetCurrentThread());
}

// static
void PlatformThread::YieldCurrentThread() {
  ::Sleep(0);
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  // When measured with a high resolution clock, Sleep() sometimes returns much
  // too early. We may need to call it repeatedly to get the desired duration.
  TimeTicks end = TimeTicks::Now() + duration;
  for (TimeTicks now = TimeTicks::Now(); now < end; now = TimeTicks::Now())
    ::Sleep(static_cast<DWORD>((end - now).InMillisecondsRoundedUp()));
}

// static
bool PlatformThread::Create(size_t stack_size,
                            Delegate* delegate,
                            PlatformThreadHandle* thread_handle) {
  DCHECK(thread_handle);
  return CreateThreadInternal(stack_size, delegate, thread_handle);
}

// static
bool PlatformThread::CreateNonJoinable(size_t stack_size, Delegate* delegate) {
  return CreateThreadInternal(stack_size, delegate, nullptr /* non-joinable */
                              );
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  DCHECK(thread_handle.platform_handle());
  // TODO(willchan): Enable this check once I can get it to work for Windows
  // shutdown.
  // Joining another thread may block the current thread for a long time, since
  // the thread referred to by |thread_handle| may still be running long-lived /
  // blocking tasks.
  // AssertBlockingAllowed();

  DWORD thread_id = 0;
  thread_id = ::GetThreadId(thread_handle.platform_handle());
  DWORD last_error = 0;
  if (!thread_id)
    last_error = ::GetLastError();

  // Wait for the thread to exit.  It should already have terminated but make
  // sure this assumption is valid.
  CHECK_EQ(WAIT_OBJECT_0,
           WaitForSingleObject(thread_handle.platform_handle(), INFINITE));
  CloseHandle(thread_handle.platform_handle());
}

// static
void PlatformThread::Detach(PlatformThreadHandle thread_handle) {
  CloseHandle(thread_handle.platform_handle());
}

}  // namespace base

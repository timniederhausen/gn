// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// WARNING: You should *NOT* be using this class directly.  PlatformThread is
// the low-level platform-specific abstraction to the OS's threading interface.
// You should instead be using a message-loop driven Thread, see thread.h.

#ifndef BASE_THREADING_PLATFORM_THREAD_H_
#define BASE_THREADING_PLATFORM_THREAD_H_

#include <stddef.h>

#include "base/base_export.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "build_config.h"

#if defined(OS_WIN)
#include "base/win/windows_types.h"
#elif defined(OS_MACOSX)
#include <mach/mach_types.h>
#elif defined(OS_POSIX)
#include <pthread.h>
#include <unistd.h>
#endif

namespace base {

// Used for logging. Always an integer value.
#if defined(OS_WIN)
typedef DWORD PlatformThreadId;
#elif defined(OS_MACOSX)
typedef mach_port_t PlatformThreadId;
#elif defined(OS_POSIX)
typedef pid_t PlatformThreadId;
#endif

// Used for thread checking and debugging.
// Meant to be as fast as possible.
// These are produced by PlatformThread::CurrentRef(), and used to later
// check if we are on the same thread or not by using ==. These are safe
// to copy between threads, but can't be copied to another process as they
// have no meaning there. Also, the internal identifier can be re-used
// after a thread dies, so a PlatformThreadRef cannot be reliably used
// to distinguish a new thread from an old, dead thread.
class PlatformThreadRef {
 public:
#if defined(OS_WIN)
  typedef DWORD RefType;
#else  //  OS_POSIX
  typedef pthread_t RefType;
#endif
  constexpr PlatformThreadRef() : id_(0) {}

  explicit constexpr PlatformThreadRef(RefType id) : id_(id) {}

  bool operator==(PlatformThreadRef other) const {
    return id_ == other.id_;
  }

  bool operator!=(PlatformThreadRef other) const { return id_ != other.id_; }

  bool is_null() const {
    return id_ == 0;
  }
 private:
  RefType id_;
};

// Used to operate on threads.
class PlatformThreadHandle {
 public:
#if defined(OS_WIN)
  typedef void* Handle;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  typedef pthread_t Handle;
#endif

  constexpr PlatformThreadHandle() : handle_(0) {}

  explicit constexpr PlatformThreadHandle(Handle handle) : handle_(handle) {}

  bool is_equal(const PlatformThreadHandle& other) const {
    return handle_ == other.handle_;
  }

  bool is_null() const {
    return !handle_;
  }

  Handle platform_handle() const {
    return handle_;
  }

 private:
  Handle handle_;
};

const PlatformThreadId kInvalidThreadId(0);

// A namespace for low-level thread functions.
class BASE_EXPORT PlatformThread {
 public:
  // Implement this interface to run code on a background thread.  Your
  // ThreadMain method will be called on the newly created thread.
  class BASE_EXPORT Delegate {
   public:
    virtual void ThreadMain() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Gets the current thread id, which may be useful for logging purposes.
  static PlatformThreadId CurrentId();

  // Gets the current thread reference, which can be used to check if
  // we're on the right thread quickly.
  static PlatformThreadRef CurrentRef();

  // Get the handle representing the current thread. On Windows, this is a
  // pseudo handle constant which will always represent the thread using it and
  // hence should not be shared with other threads nor be used to differentiate
  // the current thread from another.
  static PlatformThreadHandle CurrentHandle();

  // Yield the current thread so another thread can be scheduled.
  static void YieldCurrentThread();

  // Sleeps for the specified duration.
  static void Sleep(base::TimeDelta duration);

  // Creates a new thread.  The |stack_size| parameter can be 0 to indicate
  // that the default stack size should be used.  Upon success,
  // |*thread_handle| will be assigned a handle to the newly created thread,
  // and |delegate|'s ThreadMain method will be executed on the newly created
  // thread.
  // NOTE: When you are done with the thread handle, you must call Join to
  // release system resources associated with the thread.  You must ensure that
  // the Delegate object outlives the thread.
  static bool Create(size_t stack_size,
                     Delegate* delegate,
                     PlatformThreadHandle* thread_handle);

  // CreateNonJoinable() does the same thing as Create() except the thread
  // cannot be Join()'d.  Therefore, it also does not output a
  // PlatformThreadHandle.
  static bool CreateNonJoinable(size_t stack_size, Delegate* delegate);

  // Joins with a thread created via the Create function.  This function blocks
  // the caller until the designated thread exits.  This will invalidate
  // |thread_handle|.
  static void Join(PlatformThreadHandle thread_handle);

  // Detaches and releases the thread handle. The thread is no longer joinable
  // and |thread_handle| is invalidated after this call.
  static void Detach(PlatformThreadHandle thread_handle);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PlatformThread);
};

}  // namespace base

#endif  // BASE_THREADING_PLATFORM_THREAD_H_

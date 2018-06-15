// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_LOCK_H_
#define BASE_SYNCHRONIZATION_LOCK_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/synchronization/lock_impl.h"
#include "build_config.h"

namespace base {

// A convenient wrapper for an OS specific critical section.  The only real
// intelligence in this class is in debug mode for the support for the
// AssertAcquired() method.
class Lock {
 public:
   // Optimized wrapper implementation
  Lock() : lock_() {}
  ~Lock() {}
  void Acquire() { lock_.Lock(); }
  void Release() { lock_.Unlock(); }

  // If the lock is not held, take it and return true. If the lock is already
  // held by another thread, immediately return false. This must not be called
  // by a thread already holding the lock (what happens is undefined and an
  // assertion may fail).
  bool Try() { return lock_.Try(); }

  // Null implementation if not debug.
  void AssertAcquired() const {}

  // Whether Lock mitigates priority inversion when used from different thread
  // priorities.
  static bool HandlesMultipleThreadPriorities() {
#if defined(OS_WIN)
    // Windows mitigates priority inversion by randomly boosting the priority of
    // ready threads.
    // https://msdn.microsoft.com/library/windows/desktop/ms684831.aspx
    return true;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    // POSIX mitigates priority inversion by setting the priority of a thread
    // holding a Lock to the maximum priority of any other thread waiting on it.
    return internal::LockImpl::PriorityInheritanceAvailable();
#else
#error Unsupported platform
#endif
  }

  // Both Windows and POSIX implementations of ConditionVariable need to be
  // able to see our lock and tweak our debugging counters, as they release and
  // acquire locks inside of their condition variable APIs.
  friend class ConditionVariable;

 private:
  // Platform specific underlying lock implementation.
  internal::LockImpl lock_;

  DISALLOW_COPY_AND_ASSIGN(Lock);
};

// A helper class that acquires the given Lock while the AutoLock is in scope.
class AutoLock {
 public:
  struct AlreadyAcquired {};

  explicit AutoLock(Lock& lock) : lock_(lock) {
    lock_.Acquire();
  }

  AutoLock(Lock& lock, const AlreadyAcquired&) : lock_(lock) {
    lock_.AssertAcquired();
  }

  ~AutoLock() {
    lock_.AssertAcquired();
    lock_.Release();
  }

 private:
  Lock& lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoLock);
};

// AutoUnlock is a helper that will Release() the |lock| argument in the
// constructor, and re-Acquire() it in the destructor.
class AutoUnlock {
 public:
  explicit AutoUnlock(Lock& lock) : lock_(lock) {
    // We require our caller to have the lock.
    lock_.AssertAcquired();
    lock_.Release();
  }

  ~AutoUnlock() {
    lock_.Acquire();
  }

 private:
  Lock& lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoUnlock);
};

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_LOCK_H_

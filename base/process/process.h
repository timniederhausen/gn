// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROCESS_PROCESS_H_
#define BASE_PROCESS_PROCESS_H_

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "build_config.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base {

// Provides a move-only encapsulation of a process.
//
// This object is not tied to the lifetime of the underlying process: the
// process may be killed and this object may still around, and it will still
// claim to be valid. The actual behavior in that case is OS dependent like so:
//
// Windows: The underlying ProcessHandle will be valid after the process dies
// and can be used to gather some information about that process, but most
// methods will obviously fail.
//
// POSIX: The underlying ProcessHandle is not guaranteed to remain valid after
// the process dies, and it may be reused by the system, which means that it may
// end up pointing to the wrong process.
class Process {
 public:
  // On Windows, this takes ownership of |handle|. On POSIX, this does not take
  // ownership of |handle|.
  explicit Process(ProcessHandle handle = kNullProcessHandle);

  Process(Process&& other);

  // The destructor does not terminate the process.
  ~Process();

  Process& operator=(Process&& other);

  // Returns an object for the current process.
  static Process Current();

  // Returns a Process for the given |pid|.
  static Process Open(ProcessId pid);

  // Returns a Process for the given |pid|. On Windows the handle is opened
  // with more access rights and must only be used by trusted code (can read the
  // address space and duplicate handles).
  static Process OpenWithExtraPrivileges(ProcessId pid);

#if defined(OS_WIN)
  // Returns a Process for the given |pid|, using some |desired_access|.
  // See ::OpenProcess documentation for valid |desired_access|.
  static Process OpenWithAccess(ProcessId pid, DWORD desired_access);
#endif

  // Terminates the current process immediately with |exit_code|.
  [[noreturn]] static void TerminateCurrentProcessImmediately(int exit_code);

  // Returns true if this objects represents a valid process.
  bool IsValid() const;

  // Returns a handle for this process. There is no guarantee about when that
  // handle becomes invalid because this object retains ownership.
  ProcessHandle Handle() const;

  // Returns a second object that represents this process.
  Process Duplicate() const;

  // Get the PID for this process.
  ProcessId Pid() const;

  // Returns true if this process is the current process.
  bool is_current() const;

  // Close the process handle. This will not terminate the process.
  void Close();

// Returns true if this process is still running. This is only safe on Windows
// (and maybe Fuchsia?), because the ProcessHandle will keep the zombie
// process information available until itself has been released. But on Posix,
// the OS may reuse the ProcessId.
#if defined(OS_WIN)
  bool IsRunning() const {
    return !WaitForExitWithTimeout(base::TimeDelta(), nullptr);
  }
#endif

  // Terminates the process with extreme prejudice. The given |exit_code| will
  // be the exit code of the process. If |wait| is true, this method will wait
  // for up to one minute for the process to actually terminate.
  // Returns true if the process terminates within the allowed time.
  // NOTE: On POSIX |exit_code| is ignored.
  bool Terminate(int exit_code, bool wait) const;

  // Waits for the process to exit. Returns true on success.
  // On POSIX, if the process has been signaled then |exit_code| is set to -1.
  // On Linux this must be a child process, however on Mac and Windows it can be
  // any process.
  // NOTE: |exit_code| is optional, nullptr can be passed if the exit code is
  // not required.
  bool WaitForExit(int* exit_code) const;

  // Same as WaitForExit() but only waits for up to |timeout|.
  // NOTE: |exit_code| is optional, nullptr can be passed if the exit code
  // is not required.
  bool WaitForExitWithTimeout(TimeDelta timeout, int* exit_code) const;

  // Indicates that the process has exited with the specified |exit_code|.
  // This should be called if process exit is observed outside of this class.
  // (i.e. Not because Terminate or WaitForExit, above, was called.)
  // Note that nothing prevents this being called multiple times for a dead
  // process though that should be avoided.
  void Exited(int exit_code) const;

  // Returns an integer representing the priority of a process. The meaning
  // of this value is OS dependent.
  int GetPriority() const;

 private:
#if defined(OS_WIN)
  win::ScopedHandle process_;
#else
  ProcessHandle process_;
#endif

#if defined(OS_WIN)
  bool is_current_process_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Process);
};

}  // namespace base

#endif  // BASE_PROCESS_PROCESS_H_

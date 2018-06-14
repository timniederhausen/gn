// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include <errno.h>
#include <sys/resource.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "build_config.h"

namespace base {

namespace {

const int kForegroundPriority = 0;

const int kBackgroundPriority = 5;

bool CanReraisePriority() {
  // We won't be able to raise the priority if we don't have the right rlimit.
  // The limit may be adjusted in /etc/security/limits.conf for PAM systems.
  struct rlimit rlim;
  return (getrlimit(RLIMIT_NICE, &rlim) == 0) &&
         (20 - kForegroundPriority) <= static_cast<int>(rlim.rlim_cur);
}

}  // namespace

// static
bool Process::CanBackgroundProcesses() {
  static const bool can_reraise_priority = CanReraisePriority();
  return can_reraise_priority;
}

bool Process::IsProcessBackgrounded() const {
  DCHECK(IsValid());

  return GetPriority() == kBackgroundPriority;
}

bool Process::SetProcessBackgrounded(bool background) {
  DCHECK(IsValid());

  if (!CanBackgroundProcesses())
    return false;

  int priority = background ? kBackgroundPriority : kForegroundPriority;
  int result = setpriority(PRIO_PROCESS, process_, priority);
  DPCHECK(result == 0);
  return result == 0;
}

}  // namespace base

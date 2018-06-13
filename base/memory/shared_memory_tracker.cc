// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_tracker.h"

#include "base/memory/shared_memory.h"
#include "base/strings/string_number_conversions.h"

namespace base {

const char SharedMemoryTracker::kDumpRootName[] = "shared_memory";

// static
SharedMemoryTracker* SharedMemoryTracker::GetInstance() {
  static SharedMemoryTracker* instance = new SharedMemoryTracker;
  return instance;
}

// static
std::string SharedMemoryTracker::GetDumpNameForTracing(
    const UnguessableToken& id) {
  DCHECK(!id.is_empty());
  return std::string(kDumpRootName) + "/" + id.ToString();
}

void SharedMemoryTracker::IncrementMemoryUsage(
    const SharedMemory& shared_memory) {
  AutoLock hold(usages_lock_);
  DCHECK(usages_.find(shared_memory.memory()) == usages_.end());
  usages_.emplace(shared_memory.memory(), UsageInfo(shared_memory.mapped_size(),
                                                    shared_memory.mapped_id()));
}

void SharedMemoryTracker::IncrementMemoryUsage(
    const SharedMemoryMapping& mapping) {
  AutoLock hold(usages_lock_);
  DCHECK(usages_.find(mapping.raw_memory_ptr()) == usages_.end());
  usages_.emplace(mapping.raw_memory_ptr(),
                  UsageInfo(mapping.mapped_size(), mapping.guid()));
}

void SharedMemoryTracker::DecrementMemoryUsage(
    const SharedMemory& shared_memory) {
  AutoLock hold(usages_lock_);
  DCHECK(usages_.find(shared_memory.memory()) != usages_.end());
  usages_.erase(shared_memory.memory());
}

void SharedMemoryTracker::DecrementMemoryUsage(
    const SharedMemoryMapping& mapping) {
  AutoLock hold(usages_lock_);
  DCHECK(usages_.find(mapping.raw_memory_ptr()) != usages_.end());
  usages_.erase(mapping.raw_memory_ptr());
}

SharedMemoryTracker::SharedMemoryTracker() {
}

SharedMemoryTracker::~SharedMemoryTracker() = default;

}  // namespace

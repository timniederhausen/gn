// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory.h"

#include "build_config.h"

namespace base {

namespace {
void oom_killer_new() {
  TerminateBecauseOutOfMemory(0);
}
}  // namespace

void EnableTerminationOnHeapCorruption() {
#if !ARCH_CPU_64_BITS
  DLOG(WARNING) << "EnableTerminationOnHeapCorruption only works on 64-bit";
#endif
}

bool UncheckedMalloc(size_t size, void** result) {
  *result = malloc(size);
  return *result != nullptr;
}

bool UncheckedCalloc(size_t num_items, size_t size, void** result) {
  *result = calloc(num_items, size);
  return *result != nullptr;
}

void EnableTerminationOnOutOfMemory() {
  // Step 1: Enable OOM killer on C++ failures.
  std::set_new_handler(oom_killer_new);

  // Step 2: Enable OOM killer on all other malloc zones (or just "all" without
  // "other" if shim is disabled).
  allocator::InterceptAllocationsMac();
}

}  // namespace base

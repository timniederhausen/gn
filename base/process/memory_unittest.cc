// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _CRT_SECURE_NO_WARNINGS

#include "base/process/memory.h"

#include <stddef.h>

#include <limits>

#include "base/allocator/allocator_check.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/memory/aligned_memory.h"
#include "base/strings/stringprintf.h"
#include "build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#endif
#if defined(OS_POSIX)
#include <errno.h>
#endif
#if defined(OS_MACOSX)
#include <malloc/malloc.h>
#include "base/allocator/allocator_interception_mac.h"
#include "base/allocator/allocator_shim.h"
#include "base/process/memory_unittest_mac.h"
#endif
#if defined(OS_LINUX)
#include <malloc.h>
#include "base/test/malloc_wrapper.h"
#endif

#if defined(OS_WIN)

#if defined(COMPILER_MSVC)
// ssize_t needed for OutOfMemoryTest.
#if defined(_WIN64)
typedef __int64 ssize_t;
#else
typedef long ssize_t;
#endif
#endif

// HeapQueryInformation function pointer.
typedef BOOL (WINAPI* HeapQueryFn)  \
    (HANDLE, HEAP_INFORMATION_CLASS, PVOID, SIZE_T, PSIZE_T);

#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)

// For the following Mac tests:
// Note that base::EnableTerminationOnHeapCorruption() is called as part of
// test suite setup and does not need to be done again, else mach_override
// will fail.

TEST(ProcessMemoryTest, MacTerminateOnHeapCorruption) {
  // Assert that freeing an unallocated pointer will crash the process.
  char buf[9];
  asm("" : "=r" (buf));  // Prevent clang from being too smart.
#if ARCH_CPU_64_BITS
  // On 64 bit Macs, the malloc system automatically abort()s on heap corruption
  // but does not output anything.
  ASSERT_DEATH(free(buf), "");
#elif defined(ADDRESS_SANITIZER)
  // AddressSanitizer replaces malloc() and prints a different error message on
  // heap corruption.
  ASSERT_DEATH(free(buf), "attempting free on address which "
      "was not malloc\\(\\)-ed");
#else
  ADD_FAILURE() << "This test is not supported in this build configuration.";
#endif
}

#endif  // defined(OS_MACOSX)

TEST(MemoryTest, AllocatorShimWorking) {
#if defined(OS_MACOSX)
  base::allocator::InterceptAllocationsMac();
#endif
  ASSERT_TRUE(base::allocator::IsAllocatorInitialized());

#if defined(OS_MACOSX)
  base::allocator::UninterceptMallocZonesForTesting();
#endif
}

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_UTIL_ALIGNED_ALLOC_H_
#define TOOLS_UTIL_ALIGNED_ALLOC_H_

#ifdef __APPLE__
#include <Availability.h>
#endif

#include <cstdlib>

#define IMPL_ALIGNED_ALLOC_CXX17 1
#define IMPL_ALIGNED_ALLOC_WIN32 2
#define IMPL_ALIGNED_ALLOC_MALLOC 3

#ifndef IMPL_ALIGNED_ALLOC
#ifdef _WIN32
#define IMPL_ALIGNED_ALLOC IMPL_ALIGNED_ALLOC_WIN32
#elif defined(__APPLE__) && defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && \
    __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
// Note that aligned_alloc() is only available at runtime starting from
// OSX 10.15, so use malloc() when compiling binaries that must run on older
// releases.
#define IMPL_ALIGNED_ALLOC IMPL_ALIGNED_ALLOC_MALLOC
#else
#define IMPL_ALIGNED_ALLOC IMPL_ALIGNED_ALLOC_CXX17
#endif
#endif

#if IMPL_ALIGNED_ALLOC == IMPL_ALIGNED_ALLOC_WIN32
#include <malloc.h>  // for _aligned_malloc() and _aligned_free()
#endif

#if IMPL_ALIGNED_ALLOC == IMPL_ALIGNED_ALLOC_MALLOC
#include "base/logging.h"  // For DCHECK()
#endif

// AlignedAlloc<N> provides Alloc() and Free() methods that can be
// used to allocate and release blocks of heap memory aligned with
// N bytes.
//
// The implementation uses std::aligned_alloc() when it is available,
// or uses fallbacks otherwise. On Win32, _aligned_malloc() and
// _aligned_free() are used, while for older MacOS releases, ::malloc() is
// used directly with a small trick.
template <size_t ALIGNMENT>
struct AlignedAlloc {
  static void* Alloc(size_t size) {
    static_assert((ALIGNMENT & (ALIGNMENT - 1)) == 0,
                  "ALIGNMENT must be a power of 2");
#if IMPL_ALIGNED_ALLOC == IMPL_ALIGNED_ALLOC_WIN32
    return _aligned_malloc(size, ALIGNMENT);
#elif IMPL_ALIGNED_ALLOC == IMPL_ALIGNED_ALLOC_MALLOC
    if (ALIGNMENT <= sizeof(void*)) {
      return ::malloc(size);
    } else if (size == 0) {
      return nullptr;
    } else {
      // Allocation size must be a multiple of ALIGNMENT
      DCHECK((size % ALIGNMENT) == 0);

      // Allocate block and store its address just before just before the
      // result's address, as in:
      //    ________________________________________
      //   |    |          |                        |
      //   |    | real_ptr |                        |
      //   |____|__________|________________________|
      //
      //   ^               ^
      //   real_ptr         result
      //
      // Note that malloc() guarantees that results are aligned on sizeof(void*)
      // if the allocation size if larger than sizeof(void*). Hence, only
      // |ALIGNMENT - sizeof(void*)| extra bytes are required.
      void* real_block = ::malloc(size + ALIGNMENT - sizeof(void*));
      auto addr = reinterpret_cast<uintptr_t>(real_block) + sizeof(void*);
      uintptr_t padding = (ALIGNMENT - addr) % ALIGNMENT;
      addr += padding;
      reinterpret_cast<void**>(addr - sizeof(void*))[0] = real_block;
      return reinterpret_cast<void*>(addr);
    }
#else  // IMPL_ALIGNED_ALLOC_CXX17
    return std::aligned_alloc(ALIGNMENT, size);
#endif
  }

  static void Free(void* block) {
#if IMPL_ALIGNED_ALLOC == IMPL_ALIGNED_ALLOC_WIN32
    _aligned_free(block);
#elif IMPL_ALIGNED_ALLOC == IMPL_ALIGNED_ALLOC_MALLOC
    if (ALIGNMENT <= sizeof(void*)) {
      ::free(block);
    } else if (block) {
      if (ALIGNMENT > sizeof(void*)) {
        // Read address of real block just before the aligned block.
        block = *(reinterpret_cast<void**>(block) - 1);
      }
      ::free(block);
    }
#else
    // Allocation came from std::aligned_alloc()
    return std::free(block);
#endif
  }
};

#endif  // TOOLS_UTIL_ALIGNED_ALLOC_H_

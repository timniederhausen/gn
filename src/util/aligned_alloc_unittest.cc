// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/aligned_alloc.h"
#include "util/test/test.h"

using AlignedAllocPtrSize = AlignedAlloc<sizeof(void*)>;
using AlignedAlloc32 = AlignedAlloc<32>;

TEST(AlignedAllocTest, PtrSized) {
  void* ptr = AlignedAllocPtrSize::Alloc(2 * sizeof(void*));
  ASSERT_TRUE(ptr);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(ptr) % sizeof(void*), 0u);
  AlignedAllocPtrSize::Free(ptr);
}

TEST(AlignedAllocTest, Align32) {
  void* ptr = AlignedAlloc32::Alloc(64);
  ASSERT_TRUE(ptr);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(ptr) % 32u, 0u);
  AlignedAlloc32::Free(ptr);
}

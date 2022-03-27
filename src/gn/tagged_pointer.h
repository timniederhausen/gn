// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TAGGED_POINTER_H_
#define TOOLS_GN_TAGGED_POINTER_H_

#include "base/logging.h"

// A TaggedPointer<T.N> is a compact encoding of a (pointer, tag) pair
// when all |tag| values are guaranteed to be less than N bits long, and
// all pointer values are guaranteed to be aligned to at least N bits.
template <typename T, size_t BITS>
class TaggedPointer {
 public:
  TaggedPointer() = default;
  TaggedPointer(T* ptr, unsigned tag)
      : value_(reinterpret_cast<uintptr_t>(ptr)) {
    CheckPointerValue(ptr);
    CheckTagValue(tag);
    value_ |= static_cast<uintptr_t>(tag);
  }

  T* ptr() const { return reinterpret_cast<T*>(value_ & ~kTagMask); }
  unsigned tag() const { return static_cast<unsigned>(value_ & kTagMask); }

  void set_ptr(T* ptr) {
    CheckPointerValue(ptr);
    value_ = reinterpret_cast<uintptr_t>(ptr) | (value_ & kTagMask);
  }

  void set_tag(unsigned tag) {
    CheckTagValue(tag);
    value_ = (value_ & ~kTagMask) | tag;
  }

  bool operator==(TaggedPointer other) const { return value_ == other.value_; }

  bool operator!=(TaggedPointer other) const { return !(*this == other); }

  bool operator<(TaggedPointer other) const { return value_ < other.value_; }

 private:
  static const uintptr_t kTagMask = (uintptr_t(1) << BITS) - 1u;

  static void CheckPointerValue(T* ptr) {
    DCHECK((reinterpret_cast<uintptr_t>(ptr) & kTagMask) == 0)
        << "Pointer is not aligned to " << BITS << " bits: " << ptr;
  }
  static void CheckTagValue(unsigned tag) {
    DCHECK(tag <= kTagMask)
        << "Tag value is larger than " << BITS << " bits: " << tag;
  }

  uintptr_t value_ = 0;
};

#endif  // TOOLS_GN_TAGGED_POINTER_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING16_H_
#define BASE_STRINGS_STRING16_H_

#include <string>

namespace base {

// Temporary definitions. These should be removed and code using the standard
// ones.
using char16 = char16_t;
using string16 = std::u16string;

}  // namespace base

#endif  // BASE_STRINGS_STRING16_H_

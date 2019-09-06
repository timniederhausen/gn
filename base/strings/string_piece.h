// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copied from strings/stringpiece.h with modifications
//
// A string-like object that points to a sized piece of memory.
//
// You can use StringPiece as a function or method parameter.  A StringPiece
// parameter can receive a double-quoted string literal argument, a "const
// char*" argument, a string argument, or a StringPiece argument with no data
// copying.  Systematic use of StringPiece for arguments reduces data
// copies and strlen() calls.
//
// Prefer passing StringPieces by value:
//   void MyFunction(StringPiece arg);
// If circumstances require, you may also pass by const reference:
//   void MyFunction(const StringPiece& arg);  // not preferred
// Both of these have the same lifetime semantics.  Passing by value
// generates slightly smaller code.  For more discussion, Googlers can see
// the thread go/stringpiecebyvalue on c-users.

#ifndef BASE_STRINGS_STRING_PIECE_H_
#define BASE_STRINGS_STRING_PIECE_H_

#include <string_view>

namespace base {

// Temporary definitions. These should be removed and code using the standard
// ones.
using StringPiece = std::string_view;
using StringPiece16 = std::u16string_view;
using WStringPiece = std::wstring_view;

}  // namespace base

#endif  // BASE_STRINGS_STRING_PIECE_H_

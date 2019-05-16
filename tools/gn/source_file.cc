// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/source_file.h"

#include "base/logging.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/source_dir.h"
#include "util/build_config.h"

namespace {

void AssertValueSourceFileString(const std::string& s) {
#if defined(OS_WIN)
  DCHECK(s[0] == '/' ||
         (s.size() > 2 && s[0] != '/' && s[1] == ':' && IsSlash(s[2])));
#else
  DCHECK(s[0] == '/');
#endif
  DCHECK(!EndsWithSlash(s)) << s;
}

SourceFile::Type GetSourceFileType(const std::string& file) {
  base::StringPiece extension = FindExtension(&file);
  if (extension == "cc" || extension == "cpp" || extension == "cxx")
    return SourceFile::SOURCE_CPP;
  if (extension == "h" || extension == "hpp" || extension == "hxx" ||
      extension == "hh" || extension == "inc")
    return SourceFile::SOURCE_H;
  if (extension == "c")
    return SourceFile::SOURCE_C;
  if (extension == "m")
    return SourceFile::SOURCE_M;
  if (extension == "mm")
    return SourceFile::SOURCE_MM;
  if (extension == "rc")
    return SourceFile::SOURCE_RC;
  if (extension == "S" || extension == "s" || extension == "asm")
    return SourceFile::SOURCE_S;
  if (extension == "o" || extension == "obj")
    return SourceFile::SOURCE_O;
  if (extension == "def")
    return SourceFile::SOURCE_DEF;
  if (extension == "rs")
    return SourceFile::SOURCE_RS;
  if (extension == "go")
    return SourceFile::SOURCE_GO;

  return SourceFile::SOURCE_UNKNOWN;
}

}  // namespace

SourceFile::SourceFile() : type_(SOURCE_UNKNOWN) {}

SourceFile::SourceFile(const base::StringPiece& p)
    : value_(p.data(), p.size()) {
  DCHECK(!value_.empty());
  AssertValueSourceFileString(value_);
  NormalizePath(&value_);
  type_ = GetSourceFileType(value_);
}

SourceFile::SourceFile(SwapIn, std::string* value) {
  value_.swap(*value);
  DCHECK(!value_.empty());
  AssertValueSourceFileString(value_);
  NormalizePath(&value_);
  type_ = GetSourceFileType(value_);
}

SourceFile::~SourceFile() = default;

std::string SourceFile::GetName() const {
  if (is_null())
    return std::string();

  DCHECK(value_.find('/') != std::string::npos);
  size_t last_slash = value_.rfind('/');
  return std::string(&value_[last_slash + 1], value_.size() - last_slash - 1);
}

SourceDir SourceFile::GetDir() const {
  if (is_null())
    return SourceDir();

  DCHECK(value_.find('/') != std::string::npos);
  size_t last_slash = value_.rfind('/');
  return SourceDir(base::StringPiece(&value_[0], last_slash + 1));
}

base::FilePath SourceFile::Resolve(const base::FilePath& source_root) const {
  return ResolvePath(value_, true, source_root);
}

void SourceFile::SetValue(const std::string& value) {
  value_ = value;
  type_ = GetSourceFileType(value_);
}

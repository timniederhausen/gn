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
      extension == "hh" || extension == "inc" || extension == "ipp" ||
      extension == "inl")
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

SourceFile::SourceFile(const std::string& value) : value_(value) {
  DCHECK(!value_.empty());
  AssertValueSourceFileString(value_);
  NormalizePath(&value_);
  type_ = GetSourceFileType(value_);
}

SourceFile::SourceFile(std::string&& value) : value_(std::move(value)) {
  DCHECK(!value_.empty());
  AssertValueSourceFileString(value_);
  NormalizePath(&value_);
  type_ = GetSourceFileType(value_);
}

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
  return SourceDir(value_.substr(0, last_slash + 1));
}

base::FilePath SourceFile::Resolve(const base::FilePath& source_root) const {
  return ResolvePath(value_, true, source_root);
}

void SourceFile::SetValue(const std::string& value) {
  value_ = value;
  type_ = GetSourceFileType(value_);
}

SourceFileTypeSet::SourceFileTypeSet() : empty_(true) {
  memset(flags_, 0,
         sizeof(bool) * static_cast<int>(SourceFile::SOURCE_NUMTYPES));
}

bool SourceFileTypeSet::CSourceUsed() const {
  return empty_ || Get(SourceFile::SOURCE_CPP) || Get(SourceFile::SOURCE_H) ||
         Get(SourceFile::SOURCE_C) || Get(SourceFile::SOURCE_M) ||
         Get(SourceFile::SOURCE_MM) || Get(SourceFile::SOURCE_RC) ||
         Get(SourceFile::SOURCE_S) || Get(SourceFile::SOURCE_O) ||
         Get(SourceFile::SOURCE_DEF);
}

bool SourceFileTypeSet::RustSourceUsed() const {
  return Get(SourceFile::SOURCE_RS);
}

bool SourceFileTypeSet::GoSourceUsed() const {
  return Get(SourceFile::SOURCE_GO);
}

bool SourceFileTypeSet::MixedSourceUsed() const {
  return (1 << static_cast<int>(CSourceUsed())
            << static_cast<int>(RustSourceUsed())
            << static_cast<int>(GoSourceUsed())) > 2;
}

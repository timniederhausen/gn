// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_
#define TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_

#include "base/macros.h"
#include "tools/gn/c_tool.h"
#include "tools/gn/config_values.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/toolchain.h"
#include "tools/gn/unique_vector.h"

struct EscapeOptions;
class SourceFileTypeSet;

// Writes a .ninja file for a binary target type (an executable, a shared
// library, or a static library).
class NinjaBinaryTargetWriter : public NinjaTargetWriter {
 public:
  // Represents a set of tool types.
  class SourceFileTypeSet {
   public:
    SourceFileTypeSet() {
      memset(flags_, 0,
             sizeof(bool) * static_cast<int>(SourceFile::SOURCE_NUMTYPES));
    }

    void Set(SourceFile::Type type) { flags_[static_cast<int>(type)] = true; }
    bool Get(SourceFile::Type type) const {
      return flags_[static_cast<int>(type)];
    }

    bool CSourceUsed();
    bool RustSourceUsed();
    bool GoSourceUsed();

   private:
    bool flags_[static_cast<int>(SourceFile::SOURCE_NUMTYPES)];
  };

  NinjaBinaryTargetWriter(const Target* target, std::ostream& out);
  ~NinjaBinaryTargetWriter() override;

  void Run() override;

 protected:
  typedef std::set<OutputFile> OutputFileSet;

  // Cached version of the prefix used for rule types for this toolchain.
  std::string rule_prefix_;

  DISALLOW_COPY_AND_ASSIGN(NinjaBinaryTargetWriter);
};

#endif  // TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_

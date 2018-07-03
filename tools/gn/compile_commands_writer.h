// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_COMPILE_COMMANDS_WRITER_H_
#define TOOLS_GN_COMPILE_COMMANDS_WRITER_H_

#include "tools/gn/err.h"
#include "tools/gn/target.h"

class Builder;
class BuildSettings;

class CompileCommandsWriter {
 public:
  static bool RunAndWriteFiles(const BuildSettings* build_setting,
                               const Builder& builder,
                               const std::string& file_name,
                               bool quiet,
                               Err* err);
};

#endif  // TOOLS_GN_COMPILE_COMMANDS_WRITER_H_

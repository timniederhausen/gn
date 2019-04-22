// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_binary_target_writer.h"

#include <stddef.h>
#include <string.h>

#include <cstring>
#include <set>
#include <sstream>
#include <unordered_set>

#include "base/strings/string_util.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/err.h"
#include "tools/gn/escape.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/ninja_c_binary_target_writer.h"
#include "tools/gn/ninja_target_command_util.h"
#include "tools/gn/ninja_utils.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/settings.h"
#include "tools/gn/source_file_type.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/target.h"

bool NinjaBinaryTargetWriter::SourceFileTypeSet::CSourceUsed() {
  return Get(SOURCE_CPP) || Get(SOURCE_H) || Get(SOURCE_C) || Get(SOURCE_M) ||
         Get(SOURCE_MM) || Get(SOURCE_RC) || Get(SOURCE_S);
}

bool NinjaBinaryTargetWriter::SourceFileTypeSet::RustSourceUsed() {
  return Get(SOURCE_RS);
}

bool NinjaBinaryTargetWriter::SourceFileTypeSet::GoSourceUsed() {
  return Get(SOURCE_GO);
}

NinjaBinaryTargetWriter::NinjaBinaryTargetWriter(const Target* target,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, out),
      rule_prefix_(GetNinjaRulePrefixForToolchain(settings_)) {}

NinjaBinaryTargetWriter::~NinjaBinaryTargetWriter() = default;

void NinjaBinaryTargetWriter::Run() {
  SourceFileTypeSet used_types;
  for (const auto& source : target_->sources())
    used_types.Set(GetSourceFileType(source));

  NinjaCBinaryTargetWriter writer(target_, out_);
  writer.Run();
}

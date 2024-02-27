// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/swift_values.h"

#include "gn/deps_iterator.h"
#include "gn/err.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"

SwiftValues::SwiftValues() = default;

SwiftValues::~SwiftValues() = default;

// static
bool SwiftValues::OnTargetResolved(Target* target, Err* err) {
  return FillModuleOutputFile(target, err);
}

// static
bool SwiftValues::FillModuleOutputFile(Target* target, Err* err) {
  if (!target->builds_swift_module())
    return true;

  const Tool* tool =
      target->toolchain()->GetToolForSourceType(SourceFile::SOURCE_SWIFT);

  std::vector<OutputFile> outputs;
  SubstitutionWriter::ApplyListToLinkerAsOutputFile(target, tool,
                                                    tool->outputs(), &outputs);

  bool swiftmodule_output_found = false;
  SwiftValues& swift_values = target->swift_values();
  for (const OutputFile& output : outputs) {
    const SourceFile output_as_source =
        output.AsSourceFile(target->settings()->build_settings());
    if (!output_as_source.IsSwiftModuleType()) {
      continue;
    }

    if (swiftmodule_output_found) {
      *err = Err(tool->defined_from(), "Incorrect outputs for tool",
                 "The outputs of tool " + std::string(tool->name()) +
                     " must list exactly one .swiftmodule file");
      return false;
    }

    swift_values.module_output_file_ = output;
    swift_values.module_output_dir_ = output_as_source.GetDir();

    swiftmodule_output_found = true;
  }

  if (!swiftmodule_output_found) {
    *err = Err(tool->defined_from(), "Incorrect outputs for tool",
               "The outputs of tool " + std::string(tool->name()) +
                   " must list exactly one .swiftmodule file");
    return false;
  }

  return true;
}

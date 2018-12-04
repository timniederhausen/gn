// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_generated_file_target_writer.h"

#include "base/strings/string_util.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/output_conversion.h"
#include "tools/gn/output_file.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/settings.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

NinjaGeneratedFileTargetWriter::NinjaGeneratedFileTargetWriter(
    const Target* target,
    std::ostream& out)
    : NinjaTargetWriter(target, out) {}

NinjaGeneratedFileTargetWriter::~NinjaGeneratedFileTargetWriter() = default;

void NinjaGeneratedFileTargetWriter::Run() {
  // Write the file.
  GenerateFile();

  // A generated_file target should generate a stamp file with dependencies
  // on each of the deps and data_deps in the target. The actual collection is
  // done at gen time, and so ninja doesn't need to know about it.
  std::vector<OutputFile> output_files;
  for (const auto& pair : target_->GetDeps(Target::DEPS_LINKED))
    output_files.push_back(pair.ptr->dependency_output_file());

  std::vector<OutputFile> data_output_files;
  const LabelTargetVector& data_deps = target_->data_deps();
  for (const auto& pair : data_deps)
    data_output_files.push_back(pair.ptr->dependency_output_file());

  WriteStampForTarget(output_files, data_output_files);
}

void NinjaGeneratedFileTargetWriter::GenerateFile() {
  std::vector<SourceFile> outputs_as_sources;
  target_->action_values().GetOutputsAsSourceFiles(target_,
                                                   &outputs_as_sources);
  CHECK(outputs_as_sources.size() == 1);

  base::FilePath output =
      settings_->build_settings()->GetFullPath(outputs_as_sources[0]);
  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE, outputs_as_sources[0].value());

  // Compute output.
  Err err;
  std::ostringstream contents;
  ConvertValueToOutput(settings_, target_->contents(),
                       target_->output_conversion(), contents, &err);

  if (err.has_error()) {
    g_scheduler->FailWithError(err);
    return;
  }

  WriteFileIfChanged(output, contents.str(), &err);

  if (err.has_error()) {
    g_scheduler->FailWithError(err);
    return;
  }
}

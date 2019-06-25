// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_binary_target_writer.h"

#include <sstream>

#include "base/strings/string_util.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/general_tool.h"
#include "tools/gn/ninja_c_binary_target_writer.h"
#include "tools/gn/ninja_rust_binary_target_writer.h"
#include "tools/gn/ninja_utils.h"
#include "tools/gn/settings.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/target.h"

NinjaBinaryTargetWriter::NinjaBinaryTargetWriter(const Target* target,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, out),
      rule_prefix_(GetNinjaRulePrefixForToolchain(settings_)) {}

NinjaBinaryTargetWriter::~NinjaBinaryTargetWriter() = default;

void NinjaBinaryTargetWriter::Run() {
  if (target_->source_types_used().RustSourceUsed()) {
    NinjaRustBinaryTargetWriter writer(target_, out_);
    writer.Run();
    return;
  }

  NinjaCBinaryTargetWriter writer(target_, out_);
  writer.Run();
}

OutputFile NinjaBinaryTargetWriter::WriteInputsStampAndGetDep() const {
  CHECK(target_->toolchain()) << "Toolchain not set on target "
                              << target_->label().GetUserVisibleName(true);

  UniqueVector<const SourceFile*> inputs;
  for (ConfigValuesIterator iter(target_); !iter.done(); iter.Next()) {
    for (const auto& input : iter.cur().inputs()) {
      inputs.push_back(&input);
    }
  }

  if (inputs.size() == 0)
    return OutputFile();  // No inputs

  // If we only have one input, return it directly instead of writing a stamp
  // file for it.
  if (inputs.size() == 1)
    return OutputFile(settings_->build_settings(), *inputs[0]);

  // Make a stamp file.
  OutputFile stamp_file =
      GetBuildDirForTargetAsOutputFile(target_, BuildDirType::OBJ);
  stamp_file.value().append(target_->label().name());
  stamp_file.value().append(".inputs.stamp");

  out_ << "build ";
  path_output_.WriteFile(out_, stamp_file);
  out_ << ": " << GetNinjaRulePrefixForToolchain(settings_)
       << GeneralTool::kGeneralToolStamp;

  // File inputs.
  for (const auto* input : inputs) {
    out_ << " ";
    path_output_.WriteFile(out_, *input);
  }

  out_ << std::endl;
  return stamp_file;
}

void NinjaBinaryTargetWriter::WriteSourceSetStamp(
    const std::vector<OutputFile>& object_files) {
  // The stamp rule for source sets is generally not used, since targets that
  // depend on this will reference the object files directly. However, writing
  // this rule allows the user to type the name of the target and get a build
  // which can be convenient for development.
  UniqueVector<OutputFile> extra_object_files;
  UniqueVector<const Target*> linkable_deps;
  UniqueVector<const Target*> non_linkable_deps;
  GetDeps(&extra_object_files, &linkable_deps, &non_linkable_deps);

  // The classifier should never put extra object files in a source sets: any
  // source sets that we depend on should appear in our non-linkable deps
  // instead.
  DCHECK(extra_object_files.empty());

  std::vector<OutputFile> order_only_deps;
  for (auto* dep : non_linkable_deps)
    order_only_deps.push_back(dep->dependency_output_file());

  WriteStampForTarget(object_files, order_only_deps);
}

void NinjaBinaryTargetWriter::GetDeps(
    UniqueVector<OutputFile>* extra_object_files,
    UniqueVector<const Target*>* linkable_deps,
    UniqueVector<const Target*>* non_linkable_deps) const {
  // Normal public/private deps.
  for (const auto& pair : target_->GetDeps(Target::DEPS_LINKED)) {
    ClassifyDependency(pair.ptr, extra_object_files, linkable_deps,
                       non_linkable_deps);
  }

  // Inherited libraries.
  for (auto* inherited_target : target_->inherited_libraries().GetOrdered()) {
    ClassifyDependency(inherited_target, extra_object_files, linkable_deps,
                       non_linkable_deps);
  }

  // Data deps.
  for (const auto& data_dep_pair : target_->data_deps())
    non_linkable_deps->push_back(data_dep_pair.ptr);
}

void NinjaBinaryTargetWriter::ClassifyDependency(
    const Target* dep,
    UniqueVector<OutputFile>* extra_object_files,
    UniqueVector<const Target*>* linkable_deps,
    UniqueVector<const Target*>* non_linkable_deps) const {
  // Only the following types of outputs have libraries linked into them:
  //  EXECUTABLE
  //  SHARED_LIBRARY
  //  _complete_ STATIC_LIBRARY
  //
  // Child deps of intermediate static libraries get pushed up the
  // dependency tree until one of these is reached, and source sets
  // don't link at all.
  bool can_link_libs = target_->IsFinal();

  if (dep->output_type() == Target::SOURCE_SET ||
      // If a complete static library depends on an incomplete static library,
      // manually link in the object files of the dependent library as if it
      // were a source set. This avoids problems with braindead tools such as
      // ar which don't properly link dependent static libraries.
      (target_->complete_static_lib() &&
       (dep->output_type() == Target::STATIC_LIBRARY &&
        !dep->complete_static_lib()))) {
    // Source sets have their object files linked into final targets
    // (shared libraries, executables, loadable modules, and complete static
    // libraries). Intermediate static libraries and other source sets
    // just forward the dependency, otherwise the files in the source
    // set can easily get linked more than once which will cause
    // multiple definition errors.
    if (can_link_libs)
      AddSourceSetFiles(dep, extra_object_files);

    // Add the source set itself as a non-linkable dependency on the current
    // target. This will make sure that anything the source set's stamp file
    // depends on (like data deps) are also built before the current target
    // can be complete. Otherwise, these will be skipped since this target
    // will depend only on the source set's object files.
    non_linkable_deps->push_back(dep);
  } else if (target_->output_type() == Target::RUST_LIBRARY &&
             dep->IsLinkable()) {
    // Rust libraries aren't final, but need to have the link lines of all
    // transitive deps specified.
    linkable_deps->push_back(dep);
  } else if (target_->complete_static_lib() && dep->IsFinal()) {
    non_linkable_deps->push_back(dep);
  } else if (can_link_libs && dep->IsLinkable()) {
    linkable_deps->push_back(dep);
  } else {
    non_linkable_deps->push_back(dep);
  }
}

void NinjaBinaryTargetWriter::AddSourceSetFiles(
    const Target* source_set,
    UniqueVector<OutputFile>* obj_files) const {
  // Just add all sources to the list.
  for (const auto& source : source_set->sources()) {
    obj_files->push_back(OutputFile(settings_->build_settings(), source));
  }
}

void NinjaBinaryTargetWriter::WriteCompilerBuildLine(
    const SourceFile& source,
    const std::vector<OutputFile>& extra_deps,
    const std::vector<OutputFile>& order_only_deps,
    const char* tool_name,
    const std::vector<OutputFile>& outputs) {
  out_ << "build";
  path_output_.WriteFiles(out_, outputs);

  out_ << ": " << rule_prefix_ << tool_name;
  out_ << " ";
  path_output_.WriteFile(out_, source);

  if (!extra_deps.empty()) {
    out_ << " |";
    path_output_.WriteFiles(out_, extra_deps);
  }

  if (!order_only_deps.empty()) {
    out_ << " ||";
    path_output_.WriteFiles(out_, order_only_deps);
  }
  out_ << std::endl;
}
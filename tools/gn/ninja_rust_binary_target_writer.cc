// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_rust_binary_target_writer.h"

#include <sstream>

#include "tools/gn/deps_iterator.h"
#include "tools/gn/general_tool.h"
#include "tools/gn/ninja_target_command_util.h"
#include "tools/gn/ninja_utils.h"
#include "tools/gn/rust_substitution_type.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/target.h"

namespace {

// Returns the proper escape options for writing compiler and linker flags.
EscapeOptions GetFlagOptions() {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_COMMAND;
  return opts;
}

void WriteVar(const char* name,
              const std::string& value,
              EscapeOptions opts,
              std::ostream& out) {
  out << name << " = ";
  EscapeStringToStream(out, value, opts);
  out << std::endl;
}

void WriteCrateVars(const Target* target,
                    const Tool* tool,
                    EscapeOptions opts,
                    std::ostream& out) {
  WriteVar(kRustSubstitutionCrateName.ninja_name,
           target->rust_values().crate_name(), opts, out);

  std::string crate_type;
  switch (target->rust_values().crate_type()) {
    // Auto-select the crate type for executables, static libraries, and rlibs.
    case RustValues::CRATE_AUTO: {
      switch (target->output_type()) {
        case Target::EXECUTABLE:
          crate_type = "bin";
          break;
        case Target::STATIC_LIBRARY:
          crate_type = "staticlib";
          break;
        case Target::RUST_LIBRARY:
          crate_type = "rlib";
          break;
        default:
          NOTREACHED();
      }
      break;
    }
    case RustValues::CRATE_BIN:
      crate_type = "bin";
      break;
    case RustValues::CRATE_CDYLIB:
      crate_type = "cdylib";
      break;
    case RustValues::CRATE_DYLIB:
      crate_type = "dylib";
      break;
    case RustValues::CRATE_PROC_MACRO:
      crate_type = "proc-macro";
      break;
    case RustValues::CRATE_RLIB:
      crate_type = "rlib";
      break;
    case RustValues::CRATE_STATICLIB:
      crate_type = "staticlib";
      break;
    default:
      NOTREACHED();
  }
  WriteVar(kRustSubstitutionCrateType.ninja_name, crate_type, opts, out);

  WriteVar(SubstitutionOutputDir.ninja_name,
           SubstitutionWriter::GetLinkerSubstitution(target, tool,
                                                     &SubstitutionOutputDir),
           opts, out);
  if (!target->output_extension_set()) {
    DCHECK(tool->AsRust());
    WriteVar(kRustSubstitutionOutputExtension.ninja_name,
             tool->AsRust()->rustc_output_extension(
                 target->output_type(), target->rust_values().crate_type()),
             opts, out);
  } else if (target->output_extension().empty()) {
    WriteVar(kRustSubstitutionOutputExtension.ninja_name, "", opts, out);
  } else {
    WriteVar(kRustSubstitutionOutputExtension.ninja_name,
             std::string(".") + target->output_extension(), opts, out);
  }

  if (target->output_type() == Target::RUST_LIBRARY ||
      target->output_type() == Target::SHARED_LIBRARY)
    WriteVar(kRustSubstitutionOutputPrefix.ninja_name, "lib", opts, out);
}

}  // namespace

NinjaRustBinaryTargetWriter::NinjaRustBinaryTargetWriter(const Target* target,
                                                         std::ostream& out)
    : NinjaBinaryTargetWriter(target, out),
      tool_(target->toolchain()->GetToolForTargetFinalOutputAsRust(target)) {}

NinjaRustBinaryTargetWriter::~NinjaRustBinaryTargetWriter() = default;

// TODO(juliehockett): add inherited library support? and IsLinkable support?
// for c-cross-compat
void NinjaRustBinaryTargetWriter::Run() {
  OutputFile input_dep = WriteInputsStampAndGetDep();

  // The input dependencies will be an order-only dependency. This will cause
  // Ninja to make sure the inputs are up to date before compiling this source,
  // but changes in the inputs deps won't cause the file to be recompiled. See
  // the comment on NinjaCBinaryTargetWriter::Run for more detailed explanation.
  size_t num_stamp_uses = target_->sources().size();
  std::vector<OutputFile> order_only_deps = WriteInputDepsStampAndGetDep(
      std::vector<const Target*>(), num_stamp_uses);

  // Public rust_library deps go in a --extern rlibs, public non-rust deps go in
  // -Ldependency rustdeps, and non-public source_sets get passed in as normal
  // source files
  UniqueVector<OutputFile> deps;
  AddSourceSetFiles(target_, &deps);
  if (target_->output_type() == Target::SOURCE_SET) {
    WriteSharedVars(target_->toolchain()->substitution_bits());
    WriteSourceSetStamp(deps.vector());
  } else {
    WriteCompilerVars();
    UniqueVector<const Target*> linkable_deps;
    UniqueVector<const Target*> non_linkable_deps;
    GetDeps(&deps, &linkable_deps, &non_linkable_deps);

    if (!input_dep.value().empty())
      order_only_deps.push_back(input_dep);

    std::vector<OutputFile> rustdeps;
    std::vector<OutputFile> nonrustdeps;
    for (const auto* non_linkable_dep : non_linkable_deps) {
      order_only_deps.push_back(non_linkable_dep->dependency_output_file());
    }

    for (const auto* linkable_dep : linkable_deps) {
      if (linkable_dep->source_types_used().RustSourceUsed()) {
        rustdeps.push_back(linkable_dep->dependency_output_file());
      } else {
        nonrustdeps.push_back(linkable_dep->dependency_output_file());
      }
      deps.push_back(linkable_dep->dependency_output_file());
    }

    std::vector<OutputFile> tool_outputs;
    SubstitutionWriter::ApplyListToLinkerAsOutputFile(
        target_, tool_, tool_->outputs(), &tool_outputs);
    WriteCompilerBuildLine(target_->rust_values().crate_root(), deps.vector(),
                           order_only_deps, tool_->name(), tool_outputs);

    std::vector<const Target*> extern_deps(linkable_deps.vector());
    std::copy(non_linkable_deps.begin(), non_linkable_deps.end(),
              std::back_inserter(extern_deps));
    WriteExterns(extern_deps);

    WriteRustdeps(rustdeps, nonrustdeps);
    WriteEdition();
  }
}

void NinjaRustBinaryTargetWriter::WriteCompilerVars() {
  const SubstitutionBits& subst = target_->toolchain()->substitution_bits();

  EscapeOptions opts = GetFlagOptions();
  WriteCrateVars(target_, tool_, opts, out_);

  WriteOneFlag(target_, &kRustSubstitutionRustFlags, false,
               RustTool::kRsToolRustc, &ConfigValues::rustflags, opts,
               path_output_, out_);

  WriteOneFlag(target_, &kRustSubstitutionRustEnv, false,
               RustTool::kRsToolRustc, &ConfigValues::rustenv, opts,
               path_output_, out_);

  WriteSharedVars(subst);
}

void NinjaRustBinaryTargetWriter::WriteExterns(
    const std::vector<const Target*>& deps) {
  std::vector<const Target*> externs;
  for (const Target* target : deps) {
    if (target->output_type() == Target::RUST_LIBRARY ||
        target->rust_values().crate_type() == RustValues::CRATE_PROC_MACRO) {
      externs.push_back(target);
    }
  }
  if (externs.empty())
    return;
  out_ << "  externs =";
  for (const Target* ex : externs) {
    out_ << " --extern ";

    const auto& renamed_dep =
        target_->rust_values().aliased_deps().find(ex->label());
    if (renamed_dep != target_->rust_values().aliased_deps().end()) {
      out_ << renamed_dep->second << "=";
    } else {
      out_ << std::string(ex->rust_values().crate_name()) << "=";
    }

    path_output_.WriteFile(out_, ex->dependency_output_file());
  }
  out_ << std::endl;
}

void NinjaRustBinaryTargetWriter::WriteRustdeps(
    const std::vector<OutputFile>& rustdeps,
    const std::vector<OutputFile>& nonrustdeps) {
  if (rustdeps.empty() && nonrustdeps.empty())
    return;

  out_ << "  rustdeps =";
  for (const auto& rustdep : rustdeps) {
    out_ << " -Ldependency=";
    path_output_.WriteDir(
        out_, rustdep.AsSourceFile(settings_->build_settings()).GetDir(),
        PathOutput::DIR_NO_LAST_SLASH);
  }

  for (const auto& rustdep : nonrustdeps) {
    out_ << " -Lnative=";
    path_output_.WriteDir(
        out_, rustdep.AsSourceFile(settings_->build_settings()).GetDir(),
        PathOutput::DIR_NO_LAST_SLASH);
  }
  out_ << std::endl;
}

void NinjaRustBinaryTargetWriter::WriteEdition() {
  DCHECK(!target_->rust_values().edition().empty());
  out_ << "  edition = " << target_->rust_values().edition() << std::endl;
}

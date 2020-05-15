// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/rust_project_writer.h"

#include <fstream>
#include <sstream>
#include <tuple>

#include "base/json/string_escape.h"
#include "gn/builder.h"
#include "gn/deps_iterator.h"
#include "gn/filesystem_utils.h"
#include "gn/ninja_target_command_util.h"
#include "gn/rust_project_writer_helpers.h"
#include "gn/rust_tool.h"
#include "gn/source_file.h"
#include "gn/string_output_buffer.h"
#include "gn/tool.h"

#if defined(OS_WINDOWS)
#define NEWLINE "\r\n"
#else
#define NEWLINE "\n"
#endif

// Current structure of rust-project.json output file
//
// {
//    "roots": [] // always empty for GN. To be deprecated.
//    "crates": [
//        {
//            "deps": [
//                {
//                    "crate": 1, // index into crate array
//                    "name": "alloc" // extern name of dependency
//                },
//            ],
//            "edition": "2018", // edition of crate
//            "cfg": [
//              "unix", // "atomic" value config options
//              "rust_panic=\"abort\""", // key="value" config options
//            ]
//            "root_module": "absolute path to crate"
//        },
// }
//

bool RustProjectWriter::RunAndWriteFiles(const BuildSettings* build_settings,
                                         const Builder& builder,
                                         const std::string& file_name,
                                         bool quiet,
                                         Err* err) {
  SourceFile output_file = build_settings->build_dir().ResolveRelativeFile(
      Value(nullptr, file_name), err);
  if (output_file.is_null())
    return false;

  base::FilePath output_path = build_settings->GetFullPath(output_file);

  std::vector<const Target*> all_targets = builder.GetAllResolvedTargets();

  StringOutputBuffer out_buffer;
  std::ostream out(&out_buffer);

  RenderJSON(build_settings, all_targets, out);

  if (out_buffer.ContentsEqual(output_path)) {
    return true;
  }

  return out_buffer.WriteToFile(output_path, err);
}

// Map of Targets to their index in the crates list (for linking dependencies to
// their indexes).
using TargetIdxMap = std::unordered_map<const Target*, uint32_t>;

// A collection of Targets.
using TargetsVec = UniqueVector<const Target*>;

// Get the Rust deps for a target, recursively expanding OutputType::GROUPS
// that are present in the GN structure.  This will return a flattened list of
// deps from the groups, but will not expand a Rust lib dependency to find any
// transitive Rust dependencies.
void GetRustDeps(const Target* target, TargetsVec* rust_deps) {
  for (const auto& pair : target->GetDeps(Target::DEPS_LINKED)) {
    const Target* dep = pair.ptr;

    if (dep->source_types_used().RustSourceUsed()) {
      // Include any Rust dep.
      rust_deps->push_back(dep);
    } else if (dep->output_type() == Target::OutputType::GROUP) {
      // Inspect (recursively) any group to see if it contains Rust deps.
      GetRustDeps(dep, rust_deps);
    }
  }
}
TargetsVec GetRustDeps(const Target* target) {
  TargetsVec deps;
  GetRustDeps(target, &deps);
  return deps;
}

void WriteDeps(const Target* target,
               TargetIdxMap& lookup,
               SysrootIdxMap& sysroot_lookup,
               std::ostream& rust_project) {
  bool first_dep = true;

  rust_project << "      \"deps\": [";

  // Check if this target has had it's sysroot setup yet
  auto rust_tool =
      target->toolchain()->GetToolForSourceTypeAsRust(SourceFile::SOURCE_RS);
  auto current_sysroot = rust_tool->GetSysroot();
  if (current_sysroot != "") {
    // TODO(bwb) If this library doesn't depend on std, use core instead
    auto std_idx = sysroot_lookup[current_sysroot].find("std");
    if (std_idx != sysroot_lookup[current_sysroot].end()) {
      if (!first_dep)
        rust_project << ",";
      rust_project << NEWLINE << "        {" NEWLINE
                   << "          \"crate\": " << std::to_string(std_idx->second)
                   << "," NEWLINE << "          \"name\": \"std\"" NEWLINE
                   << "        }";
      first_dep = false;
    }
  }

  for (const auto& dep : GetRustDeps(target)) {
    auto idx = lookup[dep];
    if (!first_dep)
      rust_project << ",";
    rust_project << NEWLINE << "        {" NEWLINE
                 << "          \"crate\": " << std::to_string(idx)
                 << "," NEWLINE << "          \"name\": \""
                 << dep->rust_values().crate_name() << "\"" NEWLINE
                 << "        }";
    first_dep = false;
  }
  rust_project << NEWLINE "      ]," NEWLINE;
}

// TODO(bwb) Parse sysroot structure from toml files. This is fragile and might
// break if upstream changes the dependency structure.
const std::string_view sysroot_crates[] = {"std",
                                           "core",
                                           "alloc",
                                           "collections",
                                           "libc",
                                           "panic_unwind",
                                           "proc_macro",
                                           "rustc_unicode",
                                           "std_unicode",
                                           "test",
                                           "alloc_jemalloc",
                                           "alloc_system",
                                           "compiler_builtins",
                                           "getopts",
                                           "panic_unwind",
                                           "panic_abort",
                                           "unwind",
                                           "build_helper",
                                           "rustc_asan",
                                           "rustc_lsan",
                                           "rustc_msan",
                                           "rustc_tsan",
                                           "syntax"};

// Multiple sysroot crates have dependenices on each other.  This provides a
// mechanism for specifiying that in an extendible manner.
const std::unordered_map<std::string_view, std::vector<std::string_view>>
    sysroot_deps_map = {{"alloc", {"core"}},
                        {"std", {"alloc", "core", "panic_abort", "unwind"}}};

// Add each of the crates a sysroot has, including their dependencies.
void AddSysrootCrate(const std::string_view crate,
                     const std::string_view current_sysroot,
                     uint32_t* count,
                     SysrootCrateIdxMap& sysroot_crate_lookup,
                     std::ostream& rust_project,
                     const BuildSettings* build_settings,
                     bool first_crate) {
  if (sysroot_crate_lookup.find(crate) != sysroot_crate_lookup.end()) {
    // If this sysroot crate is already in the lookup, we don't add it again.
    return;
  }

  // Add any crates that this sysroot crate depends on.
  auto deps_lookup = sysroot_deps_map.find(crate);
  if (deps_lookup != sysroot_deps_map.end()) {
    auto deps = (*deps_lookup).second;
    for (auto dep : deps) {
      AddSysrootCrate(dep, current_sysroot, count, sysroot_crate_lookup,
                      rust_project, build_settings, first_crate);
      first_crate = false;
    }
  }

  if (!first_crate)
    rust_project << "," NEWLINE;
  first_crate = false;
  sysroot_crate_lookup.insert(std::make_pair(crate, *count));

  base::FilePath rebased_out_dir =
      build_settings->GetFullPath(build_settings->build_dir());
  auto crate_path =
      FilePathToUTF8(rebased_out_dir) + std::string(current_sysroot) +
      "/lib/rustlib/src/rust/src/lib" + std::string(crate) + "/lib.rs";
  base::FilePath crate_root = build_settings->GetFullPath(crate_path, false);

  rust_project << "    {" NEWLINE;
  rust_project << "      \"crate_id\": " << std::to_string(*count)
               << "," NEWLINE;
  rust_project << "      \"root_module\": \"" << FilePathToUTF8(crate_root)
               << "\"," NEWLINE;
  rust_project << "      \"edition\": \"2018\"," NEWLINE;
  rust_project << "      \"deps\": [";
  (*count)++;
  if (deps_lookup != sysroot_deps_map.end()) {
    auto deps = (*deps_lookup).second;
    bool first_dep = true;
    for (auto dep : deps) {
      auto idx = sysroot_crate_lookup[dep];
      if (!first_dep)
        rust_project << ",";
      first_dep = false;
      rust_project << NEWLINE "        {" NEWLINE
                   << "          \"crate\": " << std::to_string(idx)
                   << "," NEWLINE "          \"name\": \"" << dep
                   << "\"" NEWLINE "        }";
    }
  }
  rust_project << NEWLINE "      ]," NEWLINE;

  rust_project << "      \"cfg\": []" NEWLINE;

  rust_project << "    }";
}

// Add the given sysroot to the project, if it hasn't already been added.
void AddSysroot(const std::string_view sysroot,
                uint32_t* count,
                SysrootIdxMap& sysroot_lookup,
                std::ostream& rust_project,
                const BuildSettings* build_settings,
                bool first_crate) {
  // If this sysroot is already in the lookup, we don't add it again.
  if (sysroot_lookup.find(sysroot) != sysroot_lookup.end()) {
    return;
  }

  // Otherwise, add all of its crates
  for (auto crate : sysroot_crates) {
    AddSysrootCrate(crate, sysroot, count, sysroot_lookup[sysroot],
                    rust_project, build_settings, first_crate);
    first_crate = false;
  }
}

void AddTarget(const Target* target,
               uint32_t* count,
               TargetIdxMap& lookup,
               SysrootIdxMap& sysroot_lookup,
               const BuildSettings* build_settings,
               std::ostream& rust_project,
               bool first_crate) {
  if (lookup.find(target) != lookup.end()) {
    // If target is already in the lookup, we don't add it again.
    return;
  }

  // Check what sysroot this target needs.
  auto rust_tool =
      target->toolchain()->GetToolForSourceTypeAsRust(SourceFile::SOURCE_RS);
  auto current_sysroot = rust_tool->GetSysroot();
  if (current_sysroot != "" && sysroot_lookup.count(current_sysroot) == 0) {
    AddSysroot(current_sysroot, count, sysroot_lookup, rust_project,
               build_settings, first_crate);
    first_crate = false;
  }

  for (const auto& dep : GetRustDeps(target)) {
    AddTarget(dep, count, lookup, sysroot_lookup, build_settings, rust_project,
              first_crate);
    first_crate = false;
  }

  if (!first_crate)
    rust_project << "," NEWLINE;

  // Construct the crate info.
  rust_project << "    {" NEWLINE;
  rust_project << "      \"crate_id\": " << std::to_string(*count)
               << "," NEWLINE;

  // Add the target to the crate lookup.
  lookup.insert(std::make_pair(target, *count));
  (*count)++;

  base::FilePath crate_root =
      build_settings->GetFullPath(target->rust_values().crate_root());

  rust_project << "      \"root_module\": \"" << FilePathToUTF8(crate_root)
               << "\"," NEWLINE;
  rust_project << "      \"label\": \""
               << target->label().GetUserVisibleName(false) << "\"," NEWLINE;

  WriteDeps(target, lookup, sysroot_lookup, rust_project);

  std::string cfg_prefix("--cfg=");
  std::string edition_prefix("--edition=");
  std::vector<std::string> cfgs;

  bool edition_set = false;
  for (ConfigValuesIterator iter(target); !iter.done(); iter.Next()) {
    for (const auto& flag : iter.cur().rustflags()) {
      // extract the edition of this target
      if (!flag.compare(0, edition_prefix.size(), edition_prefix)) {
        auto edition = flag.substr(edition_prefix.size());
        rust_project << "      \"edition\": \"" << edition << "\"," NEWLINE;
        edition_set = true;
      }
      if (!flag.compare(0, cfg_prefix.size(), cfg_prefix)) {
        auto cfg = flag.substr(cfg_prefix.size());
        std::string escaped_config;
        base::EscapeJSONString(cfg, false, &escaped_config);
        cfgs.push_back(escaped_config);
      }
    }
  }

  if (!edition_set)
    rust_project << "      \"edition\": \"2015\"," NEWLINE;

  rust_project << "      \"cfg\": [";
  bool first_cfg = true;
  for (const auto& cfg : cfgs) {
    if (!first_cfg)
      rust_project << ",";
    first_cfg = false;
    rust_project << NEWLINE;
    rust_project << "        \"" << cfg << "\"";
  }
  rust_project << NEWLINE;
  rust_project << "      ]" NEWLINE;

  rust_project << "    }";
}

void RustProjectWriter::RenderJSON(const BuildSettings* build_settings,
                                   std::vector<const Target*>& all_targets,
                                   std::ostream& rust_project) {
  TargetIdxMap lookup;
  SysrootIdxMap sysroot_lookup;
  uint32_t count = 0;
  bool first = true;

  rust_project << "{" NEWLINE;

  rust_project << "  \"roots\": []," NEWLINE;
  rust_project << "  \"crates\": [" NEWLINE;

  // All the crates defined in the project.
  for (const auto* target : all_targets) {
    if (!target->IsBinary() || !target->source_types_used().RustSourceUsed())
      continue;

    AddTarget(target, &count, lookup, sysroot_lookup, build_settings,
              rust_project, first);
    first = false;
  }

  rust_project << NEWLINE "  ]" NEWLINE;
  rust_project << "}" NEWLINE;
}

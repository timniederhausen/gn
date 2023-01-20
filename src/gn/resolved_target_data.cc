// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/resolved_target_data.h"

#include "gn/config_values_extractors.h"

ResolvedTargetData::TargetInfo* ResolvedTargetData::GetTargetInfo(
    const Target* target) const {
  auto ret = targets_.PushBackWithIndex(target);
  if (ret.first) {
    infos_.push_back(std::make_unique<TargetInfo>(target));
  }
  return infos_[ret.second].get();
}

void ResolvedTargetData::ComputeLibInfo(TargetInfo* info) const {
  UniqueVector<SourceDir> all_lib_dirs;
  UniqueVector<LibFile> all_libs;

  for (ConfigValuesIterator iter(info->target); !iter.done(); iter.Next()) {
    const ConfigValues& cur = iter.cur();
    all_lib_dirs.Append(cur.lib_dirs());
    all_libs.Append(cur.libs());
  }
  for (const Target* dep : info->deps.linked_deps()) {
    if (!dep->IsFinal() || dep->output_type() == Target::STATIC_LIBRARY) {
      const TargetInfo* dep_info = GetTargetLibInfo(dep);
      all_lib_dirs.Append(dep_info->lib_dirs);
      all_libs.Append(dep_info->libs);
    }
  }

  info->lib_dirs = all_lib_dirs.release();
  info->libs = all_libs.release();
  info->has_lib_info = true;
}

void ResolvedTargetData::ComputeFrameworkInfo(TargetInfo* info) const {
  UniqueVector<SourceDir> all_framework_dirs;
  UniqueVector<std::string> all_frameworks;
  UniqueVector<std::string> all_weak_frameworks;

  for (ConfigValuesIterator iter(info->target); !iter.done(); iter.Next()) {
    const ConfigValues& cur = iter.cur();
    all_framework_dirs.Append(cur.framework_dirs());
    all_frameworks.Append(cur.frameworks());
    all_weak_frameworks.Append(cur.weak_frameworks());
  }
  for (const Target* dep : info->deps.linked_deps()) {
    if (!dep->IsFinal() || dep->output_type() == Target::STATIC_LIBRARY) {
      const TargetInfo* dep_info = GetTargetFrameworkInfo(dep);
      all_framework_dirs.Append(dep_info->framework_dirs);
      all_frameworks.Append(dep_info->frameworks);
      all_weak_frameworks.Append(dep_info->weak_frameworks);
    }
  }

  info->framework_dirs = all_framework_dirs.release();
  info->frameworks = all_frameworks.release();
  info->weak_frameworks = all_weak_frameworks.release();
  info->has_framework_info = true;
}

void ResolvedTargetData::ComputeHardDeps(TargetInfo* info) const {
  TargetSet all_hard_deps;
  for (const Target* dep : info->deps.linked_deps()) {
    // Direct hard dependencies
    if (info->target->hard_dep() || dep->hard_dep()) {
      all_hard_deps.insert(dep);
      continue;
    }
    // If |dep| is binary target and |dep| has no public header,
    // |this| target does not need to have |dep|'s hard_deps as its
    // hard_deps to start compiles earlier. Unless the target compiles a
    // Swift module (since they also generate a header that can be used
    // by the current target).
    if (dep->IsBinary() && !dep->all_headers_public() &&
        dep->public_headers().empty() && !dep->builds_swift_module()) {
      continue;
    }

    // Recursive hard dependencies of all dependencies.
    const TargetInfo* dep_info = GetTargetHardDeps(dep);
    all_hard_deps.insert(dep_info->hard_deps);
  }
  info->hard_deps = std::move(all_hard_deps);
  info->has_hard_deps = true;
}

void ResolvedTargetData::ComputeInheritedLibs(TargetInfo* info) const {
  TargetPublicPairListBuilder inherited_libraries;

  ComputeInheritedLibsFor(info->deps.public_deps(), true, &inherited_libraries);
  ComputeInheritedLibsFor(info->deps.private_deps(), false,
                          &inherited_libraries);

  info->inherited_libs = inherited_libraries.Build();
  info->has_inherited_libs = true;
}

void ResolvedTargetData::ComputeInheritedLibsFor(
    base::span<const Target*> deps,
    bool is_public,
    TargetPublicPairListBuilder* inherited_libraries) const {
  for (const Target* dep : deps) {
    // Direct dependent libraries.
    if (dep->output_type() == Target::STATIC_LIBRARY ||
        dep->output_type() == Target::SHARED_LIBRARY ||
        dep->output_type() == Target::RUST_LIBRARY ||
        dep->output_type() == Target::SOURCE_SET ||
        (dep->output_type() == Target::CREATE_BUNDLE &&
         dep->bundle_data().is_framework())) {
      inherited_libraries->Append(dep, is_public);
    }
    if (dep->output_type() == Target::SHARED_LIBRARY) {
      // Shared library dependendencies are inherited across public shared
      // library boundaries.
      //
      // In this case:
      //   EXE -> INTERMEDIATE_SHLIB --[public]--> FINAL_SHLIB
      // The EXE will also link to to FINAL_SHLIB. The public dependency means
      // that the EXE can use the headers in FINAL_SHLIB so the FINAL_SHLIB
      // will need to appear on EXE's link line.
      //
      // However, if the dependency is private:
      //   EXE -> INTERMEDIATE_SHLIB --[private]--> FINAL_SHLIB
      // the dependency will not be propagated because INTERMEDIATE_SHLIB is
      // not granting permission to call functions from FINAL_SHLIB. If EXE
      // wants to use functions (and link to) FINAL_SHLIB, it will need to do
      // so explicitly.
      //
      // Static libraries and source sets aren't inherited across shared
      // library boundaries because they will be linked into the shared
      // library. Rust dylib deps are handled above and transitive deps are
      // resolved by the compiler.
      const TargetInfo* dep_info = GetTargetInheritedLibs(dep);
      for (const auto& pair : dep_info->inherited_libs) {
        if (pair.target()->output_type() == Target::SHARED_LIBRARY &&
            pair.is_public()) {
          inherited_libraries->Append(pair.target(), is_public);
        }
      }
    } else if (!dep->IsFinal()) {
      // The current target isn't linked, so propagate linked deps and
      // libraries up the dependency tree.
      const TargetInfo* dep_info = GetTargetInheritedLibs(dep);
      for (const auto& pair : dep_info->inherited_libs) {
        // Proc macros are not linked into targets that depend on them, so do
        // not get inherited; they are consumed by the Rust compiler and only
        // need to be specified in --extern.
        if (pair.target()->output_type() != Target::RUST_PROC_MACRO)
          inherited_libraries->Append(pair.target(),
                                      is_public && pair.is_public());
      }
    } else if (dep->complete_static_lib()) {
      // Inherit only final targets through _complete_ static libraries.
      //
      // Inherited final libraries aren't linked into complete static
      // libraries. They are forwarded here so that targets that depend on
      // complete static libraries can link them in. Conversely, since
      // complete static libraries link in non-final targets, they shouldn't be
      // inherited.
      const TargetInfo* dep_info = GetTargetInheritedLibs(dep);
      for (const auto& pair : dep_info->inherited_libs) {
        if (pair.target()->IsFinal())
          inherited_libraries->Append(pair.target(),
                                      is_public && pair.is_public());
      }
    }
  }
}

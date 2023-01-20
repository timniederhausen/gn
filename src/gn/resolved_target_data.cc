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

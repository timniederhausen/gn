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

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RESOLVED_TARGET_DATA_H_
#define TOOLS_GN_RESOLVED_TARGET_DATA_H_

#include <memory>

#include "gn/immutable_vector.h"
#include "gn/lib_file.h"
#include "gn/source_dir.h"
#include "gn/tagged_pointer.h"
#include "gn/target.h"
#include "gn/target_public_pair.h"

class Target;

// A list of (target_ptr, is_public_flag) pairs as returned by methods
// of ResolvedTargetData.
using TargetPublicPairList = ImmutableVectorView<TargetPublicPair>;

// A class used to compute data.
class ResolvedTargetData {
 public:
  ResolvedTargetData();
  ~ResolvedTargetData();

  // Move operations
  ResolvedTargetData(ResolvedTargetData&&) noexcept;
  ResolvedTargetData& operator=(ResolvedTargetData&&);

  // Retrieve information about link-time libraries needed by this target.
  struct LibInfo {
    ImmutableVectorView<SourceDir> all_lib_dirs;
    ImmutableVectorView<LibFile> all_libs;
  };
  LibInfo GetLibInfo(const Target*) const;

  ImmutableVectorView<SourceDir> all_lib_dirs(const Target* target) const;

  ImmutableVectorView<LibFile> all_libs(const Target* target) const;

  // Retrieve information about link-time OS X frameworks needed by this target.
  struct FrameworkInfo {
    ImmutableVector<SourceDir> all_framework_dirs;
    ImmutableVector<std::string> all_frameworks;
    ImmutableVector<std::string> all_weak_frameworks;
  };
  FrameworkInfo GetFrameworkInfo(const Target* target) const;

  ImmutableVectorView<SourceDir> all_framework_dirs(const Target* target) const;

  ImmutableVectorView<std::string> all_frameworks(const Target* target) const;

  ImmutableVectorView<std::string> all_weak_frameworks(
      const Target* target) const;

  // Retrieve a set of hard dependencies for this target.
  TargetSet recursive_hard_deps(const Target* target) const;

  // Retrieve an ordered list of (target, is_public) pairs for all link-time
  // libraries inherited by this target.
  TargetPublicPairList inherited_libraries(const Target* target) const;

  // Retrieves an ordered list of (target, is_public) paris for all link-time
  // libraries for Rust-specific binary targets.
  TargetPublicPairList rust_transitive_inherited_libs(
      const Target* target) const;

 private:
  class Impl;

  Impl* GetImpl() const;

  mutable std::unique_ptr<Impl> impl_;
};

#endif  // TOOLS_GN_RESOLVED_TARGET_DATA_H_

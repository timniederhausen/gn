// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SWIFT_TARGET_VALUES_H_
#define TOOLS_GN_SWIFT_TARGET_VALUES_H_

#include <string>

#include "gn/output_file.h"
#include "gn/source_dir.h"
#include "gn/source_file.h"
#include "gn/unique_vector.h"

class Err;
class Target;

// Holds values specific to target that compile .swift files.
class SwiftValues {
 public:
  SwiftValues();
  ~SwiftValues();

  SwiftValues(const SwiftValues&) = delete;
  SwiftValues& operator=(const SwiftValues&) = delete;

  // Called when the target is resolved.
  bool OnTargetResolved(const Target* target, Err* err);

  // Path of the bridging header.
  SourceFile& bridge_header() { return bridge_header_; }
  const SourceFile& bridge_header() const { return bridge_header_; }

  // Name of the module.
  std::string& module_name() { return module_name_; }
  const std::string module_name() const { return module_name_; }

  // Returns whether the target generates a .swiftmodule.
  bool builds_module() const { return module_output_file_ != OutputFile(); }

  // Name of the generated .swiftmodule file. Computed when the target
  // is resolved.
  const OutputFile& module_output_file() const { return module_output_file_; }

  // Path of the directory containing the generated .swiftmodule file.
  // Computed when the target is resolved.
  const SourceDir& module_output_dir() const { return module_output_dir_; }

  // List of dependent target that generate a .swiftmodule. The current target
  // is assumed to depend on those modules, and will add them to the module
  // search path.
  const UniqueVector<const Target*>& modules() const { return modules_; }

  // List of dependent target that generate a .swiftmodule that are publicly
  // exported by the current target. This will include the current target if
  // it generates a .swiftmodule.
  const UniqueVector<const Target*>& public_modules() const {
    return public_modules_;
  }

 private:
  // Fill informations about .swiftmodule generated by this target.
  bool FillModuleOuputFile(const Target* target, Err* err);

  // Fill dependencies information on other target generating .swiftmodules.
  void FillModuleDependencies(const Target* target);

  // Name of the optional bridge header used to import Objective-C classes.
  // Filled from the target, may be empty even if the target include .swift
  // source files.
  SourceFile bridge_header_;

  // Name of the generate module for use by substitution.
  std::string module_name_;

  // Path to the .swiftmodule generated by this target. Will be empty if the
  // target does not include .swift sources.
  OutputFile module_output_file_;

  // Path of the directory containing the .swiftmodule generated by this
  // target. Will be null if the target does not include .swift sources.
  SourceDir module_output_dir_;

  // For modules() and public_modules() function. Will be filled when the
  // target is resolved (can be non-empty even if the target does not build
  // .swift sources due to transitive dependencies).
  UniqueVector<const Target*> modules_;
  UniqueVector<const Target*> public_modules_;
};

#endif  // TOOLS_GN_SWIFT_TARGET_VALUES_H_
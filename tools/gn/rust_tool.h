// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RUST_TOOL_H_
#define TOOLS_GN_RUST_TOOL_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "tools/gn/label.h"
#include "tools/gn/label_ptr.h"
#include "tools/gn/rust_values.h"
#include "tools/gn/source_file.h"
#include "tools/gn/substitution_list.h"
#include "tools/gn/substitution_pattern.h"
#include "tools/gn/target.h"
#include "tools/gn/tool.h"

class RustTool : public Tool {
 public:
  // Rust tools
  static const char* kRsToolRustc;

  explicit RustTool(const char* n);
  ~RustTool();

  // Manual RTTI and required functions ---------------------------------------

  bool InitTool(Scope* block_scope, Toolchain* toolchain, Err* err);
  bool ValidateName(const char* name) const override;
  void SetComplete() override;
  bool ValidateSubstitution(const Substitution* sub_type) const override;

  RustTool* AsRust() override;
  const RustTool* AsRust() const override;

  void set_exe_output_extension(std::string ext) {
    DCHECK(!complete_);
    DCHECK(ext.empty() || ext[0] == '.');
    exe_output_extension_ = std::move(ext);
  }

  void set_rlib_output_extension(std::string ext) {
    DCHECK(!complete_);
    DCHECK(ext.empty() || ext[0] == '.');
    rlib_output_extension_ = std::move(ext);
  }

  void set_dylib_output_extension(std::string ext) {
    DCHECK(!complete_);
    DCHECK(ext.empty() || ext[0] == '.');
    dylib_output_extension_ = std::move(ext);
  }

  void set_cdylib_output_extension(std::string ext) {
    DCHECK(!complete_);
    DCHECK(ext.empty() || ext[0] == '.');
    cdylib_output_extension_ = std::move(ext);
  }

  void set_staticlib_output_extension(std::string ext) {
    DCHECK(!complete_);
    DCHECK(ext.empty() || ext[0] == '.');
    staticlib_output_extension_ = std::move(ext);
  }

  void set_proc_macro_output_extension(std::string ext) {
    DCHECK(!complete_);
    DCHECK(ext.empty() || ext[0] == '.');
    proc_macro_output_extension_ = std::move(ext);
  }

  // Will include a leading "." if nonempty.
  const std::string& rustc_output_extension(
      Target::OutputType type,
      const RustValues::CrateType crate_type) const;

 private:
  bool SetOutputExtension(const Value* value, std::string* var, Err* err);
  bool ReadOutputExtensions(Scope* scope, Err* err);
  bool ReadOutputsPatternList(Scope* scope,
                              const char* var,
                              SubstitutionList* field,
                              Err* err);

  std::string exe_output_extension_;
  std::string rlib_output_extension_;
  std::string dylib_output_extension_;
  std::string cdylib_output_extension_;
  std::string staticlib_output_extension_;
  std::string proc_macro_output_extension_;

  DISALLOW_COPY_AND_ASSIGN(RustTool);
};

#endif  // TOOLS_GN_RUST_TOOL_H_

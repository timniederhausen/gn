// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_tool.h"

#include "tools/gn/rust_substitution_type.h"
#include "tools/gn/target.h"

const char* RustTool::kRsToolRustc = "rustc";

RustTool::RustTool(const char* n) : Tool(n), rlib_output_extension_(".rlib") {
  CHECK(ValidateName(n));
  // TODO: should these be settable in toolchain definition?
  set_framework_switch("-lframework=");
  set_lib_dir_switch("-Lnative=");
  set_lib_switch("-l");
  set_linker_arg("-Clink-arg=");
}

RustTool::~RustTool() = default;

RustTool* RustTool::AsRust() {
  return this;
}
const RustTool* RustTool::AsRust() const {
  return this;
}

bool RustTool::ValidateName(const char* name) const {
  return name_ == kRsToolRustc;
}

void RustTool::SetComplete() {
  SetToolComplete();
}

bool RustTool::SetOutputExtension(const Value* value,
                                  std::string* var,
                                  Err* err) {
  DCHECK(!complete_);
  if (!value)
    return true;  // Not present is fine.
  if (!value->VerifyTypeIs(Value::STRING, err))
    return false;
  if (value->string_value().empty())
    return true;

  *var = std::move(value->string_value());
  return true;
}

bool RustTool::ReadOutputExtensions(Scope* scope, Err* err) {
  if (!SetOutputExtension(scope->GetValue("exe_output_extension", true),
                          &exe_output_extension_, err))
    return false;
  if (!SetOutputExtension(scope->GetValue("rlib_output_extension", true),
                          &rlib_output_extension_, err))
    return false;
  if (!SetOutputExtension(scope->GetValue("dylib_output_extension", true),
                          &dylib_output_extension_, err))
    return false;
  if (!SetOutputExtension(scope->GetValue("cdylib_output_extension", true),
                          &cdylib_output_extension_, err))
    return false;
  if (!SetOutputExtension(scope->GetValue("staticlib_output_extension", true),
                          &staticlib_output_extension_, err))
    return false;
  if (!SetOutputExtension(scope->GetValue("proc_macro_output_extension", true),
                          &proc_macro_output_extension_, err))
    return false;
  return true;
}

bool RustTool::ReadOutputsPatternList(Scope* scope,
                                      const char* var,
                                      SubstitutionList* field,
                                      Err* err) {
  DCHECK(!complete_);
  const Value* value = scope->GetValue(var, true);
  if (!value)
    return true;  // Not present is fine.
  if (!value->VerifyTypeIs(Value::LIST, err))
    return false;

  SubstitutionList list;
  if (!list.Parse(*value, err))
    return false;

  // Validate the right kinds of patterns are used.
  if (list.list().empty()) {
    *err = Err(defined_from(), "\"outputs\" must be specified for this tool.");
    return false;
  }

  for (const auto& cur_type : list.required_types()) {
    if (!IsValidRustSubstitution(cur_type)) {
      *err = Err(*value, "Pattern not valid here.",
                 "You used the pattern " + std::string(cur_type->name) +
                     " which is not valid\nfor this variable.");
      return false;
    }
  }

  *field = std::move(list);
  return true;
}

bool RustTool::InitTool(Scope* scope, Toolchain* toolchain, Err* err) {
  // Initialize default vars.
  if (!Tool::InitTool(scope, toolchain, err)) {
    return false;
  }

  if (!ReadOutputExtensions(scope, err)) {
    return false;
  }

  // All Rust tools should have outputs.
  if (!ReadOutputsPatternList(scope, "outputs", &outputs_, err)) {
    return false;
  }
  return true;
}

bool RustTool::ValidateSubstitution(const Substitution* sub_type) const {
  if (name_ == kRsToolRustc)
    return IsValidRustSubstitution(sub_type);
  NOTREACHED();
  return false;
}

const std::string& RustTool::rustc_output_extension(
    Target::OutputType type,
    const RustValues::CrateType crate_type) const {
  switch (crate_type) {
    case RustValues::CRATE_AUTO: {
      switch (type) {
        case Target::EXECUTABLE:
          return exe_output_extension_;
        case Target::STATIC_LIBRARY:
          return staticlib_output_extension_;
        case Target::RUST_LIBRARY:
          return rlib_output_extension_;
        default:
          NOTREACHED();
          return exe_output_extension_;
      }
    }
    case RustValues::CRATE_DYLIB:
      return dylib_output_extension_;
    case RustValues::CRATE_CDYLIB:
      return cdylib_output_extension_;
    case RustValues::CRATE_PROC_MACRO:
      return proc_macro_output_extension_;
    default:
      NOTREACHED();
      return exe_output_extension_;
  }
}

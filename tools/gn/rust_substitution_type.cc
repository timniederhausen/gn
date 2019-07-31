// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_substitution_type.h"

#include <stddef.h>
#include <stdlib.h>

#include "tools/gn/err.h"
#include "tools/gn/substitution_type.h"

const SubstitutionTypes RustSubstitutions = {
    &kRustSubstitutionCrateName,       &kRustSubstitutionCrateType,
    &kRustSubstitutionEdition,         &kRustSubstitutionExterns,
    &kRustSubstitutionOutputExtension, &kRustSubstitutionOutputPrefix,
    &kRustSubstitutionRustDeps,        &kRustSubstitutionRustFlags,
    &kRustSubstitutionRustEnv,
};

// Valid for Rust tools.
const Substitution kRustSubstitutionCrateName = {"{{crate_name}}",
                                                 "crate_name"};
const Substitution kRustSubstitutionCrateType = {"{{crate_type}}",
                                                 "crate_type"};
const Substitution kRustSubstitutionEdition = {"{{edition}}", "edition"};
const Substitution kRustSubstitutionExterns = {"{{externs}}", "externs"};
const Substitution kRustSubstitutionOutputExtension = {
    "{{rustc_output_extension}}", "rustc_output_extension"};
const Substitution kRustSubstitutionOutputPrefix = {"{{rustc_output_prefix}}",
                                                    "rustc_output_prefix"};
const Substitution kRustSubstitutionRustDeps = {"{{rustdeps}}", "rustdeps"};
const Substitution kRustSubstitutionRustEnv = {"{{rustenv}}", "rustenv"};
const Substitution kRustSubstitutionRustFlags = {"{{rustflags}}", "rustflags"};

bool IsValidRustSubstitution(const Substitution* type) {
  return IsValidToolSubstitution(type) || IsValidSourceSubstitution(type) ||
         type == &SubstitutionOutputDir ||
         type == &kRustSubstitutionCrateName ||
         type == &kRustSubstitutionCrateType ||
         type == &kRustSubstitutionEdition ||
         type == &kRustSubstitutionExterns ||
         type == &kRustSubstitutionOutputExtension ||
         type == &kRustSubstitutionOutputPrefix ||
         type == &kRustSubstitutionRustDeps ||
         type == &kRustSubstitutionRustEnv ||
         type == &kRustSubstitutionRustFlags;
}

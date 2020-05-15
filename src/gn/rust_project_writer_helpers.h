// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RUST_PROJECT_WRITER_HELPERS_H_
#define TOOLS_GN_RUST_PROJECT_WRITER_HELPERS_H_

#include <fstream>
#include <sstream>
#include <unordered_map>

#include "build_settings.h"
#include "gn/target.h"

// These are internal types and helper functions for RustProjectWriter that have
// been extracted for easier testability.

// Mapping of a sysroot crate (path) to it's index in the crates list.
using SysrootCrateIdxMap = std::unordered_map<std::string_view, uint32_t>;

// Mapping of a sysroot (path) to the mapping of each of the sysroot crates to
// their index in the crates list.
using SysrootIdxMap = std::unordered_map<std::string_view, SysrootCrateIdxMap>;

// Add all of the crates for a sysroot (path) to the rust_project ostream.
void AddSysroot(const std::string_view sysroot,
                uint32_t* count,
                SysrootIdxMap& sysroot_lookup,
                std::ostream& rust_project,
                const BuildSettings* build_settings,
                bool first_crate);

// Add a sysroot crate to the rust_project ostream, first recursively adding its
// sysroot crate depedencies.
void AddSysrootCrate(const std::string_view crate,
                     const std::string_view current_sysroot,
                     uint32_t* count,
                     SysrootCrateIdxMap& sysroot_crate_lookup,
                     std::ostream& rust_project,
                     const BuildSettings* build_settings,
                     bool first_crate);

#endif  // TOOLS_GN_RUST_PROJECT_WRITER_HELPERS_H_
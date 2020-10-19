// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_TOOLS_H_
#define TOOLS_GN_NINJA_TOOLS_H_

#include <vector>

#include "base/files/file_path.h"
#include "gn/err.h"

// Invokes the ninja restat tool (ie, ninja -C build_dir -t restat). This tool
// tells ninja that it should check the mtime of the provided files and update
// the .ninja_log accordingly. This is useful when GN knows that an output file
// in the ninja graph has been updated without invoking ninja.
//
// The best example of this is after gn gen runs, we know that build.ninja has
// been potentially updated, but ninja will still use the mtime from the
// .ninja_log and could trigger another re-gen. By telling ninja to restat
// build.ninja, we can eliminate the extra re-gen.
//
// If files_to_restat is empty, ninja will restat all files that have an entry
// in the .ninja_log.
bool InvokeNinjaRestatTool(const base::FilePath& ninja_executable,
                           const base::FilePath& build_dir,
                           const std::vector<base::FilePath>& files_to_restat,
                           Err* err);

#endif // TOOLS_GN_NINJA_TOOLS_H_

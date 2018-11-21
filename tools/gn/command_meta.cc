// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <set>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "tools/gn/commands.h"
#include "tools/gn/metadata_walk.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/switches.h"
#include "tools/gn/target.h"

namespace commands {

const char kMeta[] = "meta";
const char kMeta_HelpShort[] = "meta: List target metadata collection results.";
const char kMeta_Help[] =
    R"(gn meta <out_dir> <target>* --data=<key>[,<key>*]* [--walk=<key>[,<key>*]*]
       [--rebase]

  Lists collected metaresults of all given targets for the given data key(s),
  collecting metadata dependencies as specified by the given walk key(s).

Examples

  gn meta out/Debug "//base/foo" --data=files
      Lists collected metaresults for the `files` key in the //base/foo:foo
      target and all of its dependency tree.

  gn meta out/Debug "//base/foo" --data=files --data=other
      Lists collected metaresults for the `files` and `other` keys in the
      //base/foo:foo target and all of its dependency tree.

  gn meta out/Debug "//base/foo" --data=files --walk=stop
      Lists collected metaresults for the `files` key in the //base/foo:foo
      target and all of the dependencies listed in the `stop` key (and so on).

  gn meta out/Debug "//base/foo" --data=files --rebase-files
      Lists collected metaresults for the `files` key in the //base/foo:foo
      target and all of its dependency tree, rebasing the strings in the `files`
      key onto the source directory of the target's declaration.
)";

int RunMeta(const std::vector<std::string>& args) {
  if (args.size() == 0) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn meta <out_dir> <target>* --data=<key>[,<key>*] "
        "[--walk=<key>[,<key>*]*] [--rebase-files]\"")
        .PrintToStdout();
    return 1;
  }

  Setup* setup = new Setup;
  if (!setup->DoSetup(args[0], false) || !setup->Run())
    return 1;

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  bool rebase_files = cmdline->HasSwitch(switches::kMetaRebaseFiles);
  std::string data_keys_str =
      cmdline->GetSwitchValueASCII(switches::kMetaDataKeys);
  std::string walk_keys_str =
      cmdline->GetSwitchValueASCII(switches::kMetaWalkKeys);

  std::vector<std::string> inputs(args.begin() + 1, args.end());

  UniqueVector<const Target*> targets;
  for (const auto& input : inputs) {
    const Target* target = ResolveTargetFromCommandLineString(setup, input);
    if (!target) {
      Err(Location(), "Unknown target " + input).PrintToStdout();
      return 1;
    }
    targets.push_back(target);
  }

  std::vector<std::string> data_keys = base::SplitString(
      data_keys_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (data_keys.empty()) {
    return 1;
  }
  std::vector<std::string> walk_keys = base::SplitString(
      walk_keys_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  Err err;
  std::set<const Target*> targets_walked;
  std::vector<Value> result = WalkMetadata(targets, data_keys, walk_keys,
                                           rebase_files, &targets_walked, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return 1;
  }

  OutputString("Metadata values\n", DECORATION_DIM);
  for (const auto& value : result)
    OutputString("\n" + value.ToString(false) + "\n");

  // TODO(juliehockett): We should have better dep tracing and error support for
  // this. Also possibly data about where different values came from.
  OutputString("\nExtracted from:\n", DECORATION_DIM);
  bool first = true;
  for (const auto* target : targets_walked) {
    if (!first) {
      first = false;
      OutputString(", ", DECORATION_DIM);
    }
    OutputString(target->label().GetUserVisibleName(true) + "\n");
  }
  OutputString("\nusing data keys:\n", DECORATION_DIM);
  first = true;
  for (const auto& key : data_keys) {
    if (!first) {
      first = false;
      OutputString(", ");
    }
    OutputString(key + "\n");
  }
  if (!walk_keys.empty()) {
    OutputString("\nand using walk keys:\n", DECORATION_DIM);
    first = true;
    for (const auto& key : walk_keys) {
      if (!first) {
        first = false;
        OutputString(", ");
      }
      OutputString(key + "\n");
    }
  }
  return 0;
}

}  // namespace commands

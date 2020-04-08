// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/rust_project_writer.h"
#include "base/strings/string_util.h"
#include "gn/substitution_list.h"
#include "gn/target.h"
#include "gn/test_with_scheduler.h"
#include "gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

using RustProjectJSONWriter = TestWithScheduler;

TEST_F(RustProjectJSONWriter, OneRustTarget) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::RUST_LIBRARY);
  target.visibility().SetPublic();
  SourceFile lib("//foo/lib.rs");
  target.sources().push_back(lib);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(lib);
  target.rust_values().crate_name() = "foo";
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream stream;
  std::vector<const Target*> targets;
  targets.push_back(&target);
  RustProjectWriter::RenderJSON(setup.build_settings(), targets, stream);
  std::string out = stream.str();
#if defined(OS_WIN)
  base::ReplaceSubstringsAfterOffset(&out, 0, "\r\n", "\n");
#endif
  const char expected_json[] =
      "{\n"
      "  \"roots\": [],\n"
      "  \"crates\": [\n"
      "    {\n"
      "      \"crate_id\": 0,\n"
      "      \"root_module\": \"foo/lib.rs\",\n"
      "      \"deps\": [\n"
      "      ],\n"
      "      \"edition\": \"2015\",\n"
      "      \"atom_cfgs\": [\n"
      "      ],\n"
      "      \"key_value_cfgs\": {\n"
      "      }\n"
      "    }\n"
      "  ]\n"
      "}\n";

  EXPECT_EQ(expected_json, out);
}

TEST_F(RustProjectJSONWriter, RustTargetDep) {
  Err err;
  TestWithScope setup;

  Target dep(setup.settings(), Label(SourceDir("//tortoise/"), "bar"));
  dep.set_output_type(Target::RUST_LIBRARY);
  dep.visibility().SetPublic();
  SourceFile tlib("//tortoise/lib.rs");
  dep.sources().push_back(tlib);
  dep.source_types_used().Set(SourceFile::SOURCE_RS);
  dep.rust_values().set_crate_root(tlib);
  dep.rust_values().crate_name() = "tortoise";
  dep.SetToolchain(setup.toolchain());

  Target target(setup.settings(), Label(SourceDir("//hare/"), "bar"));
  target.set_output_type(Target::RUST_LIBRARY);
  target.visibility().SetPublic();
  SourceFile harelib("//hare/lib.rs");
  target.sources().push_back(harelib);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(harelib);
  target.rust_values().crate_name() = "hare";
  target.public_deps().push_back(LabelTargetPair(&dep));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream stream;
  std::vector<const Target*> targets;
  targets.push_back(&target);
  RustProjectWriter::RenderJSON(setup.build_settings(), targets, stream);
  std::string out = stream.str();
#if defined(OS_WIN)
  base::ReplaceSubstringsAfterOffset(&out, 0, "\r\n", "\n");
#endif
  const char expected_json[] =
      "{\n"
      "  \"roots\": [],\n"
      "  \"crates\": [\n"
      "    {\n"
      "      \"crate_id\": 0,\n"
      "      \"root_module\": \"tortoise/lib.rs\",\n"
      "      \"deps\": [\n"
      "      ],\n"
      "      \"edition\": \"2015\",\n"
      "      \"atom_cfgs\": [\n"
      "      ],\n"
      "      \"key_value_cfgs\": {\n"
      "      }\n"
      "    },\n"
      "    {\n"
      "      \"crate_id\": 1,\n"
      "      \"root_module\": \"hare/lib.rs\",\n"
      "      \"deps\": [\n"
      "        {\n"
      "          \"crate\": 0,\n"
      "          \"name\": \"tortoise\"\n"
      "        }\n"
      "      ],\n"
      "      \"edition\": \"2015\",\n"
      "      \"atom_cfgs\": [\n"
      "      ],\n"
      "      \"key_value_cfgs\": {\n"
      "      }\n"
      "    }\n"
      "  ]\n"
      "}\n";

  EXPECT_EQ(expected_json, out);
}

TEST_F(RustProjectJSONWriter, RustTargetDepTwo) {
  Err err;
  TestWithScope setup;

  Target dep(setup.settings(), Label(SourceDir("//tortoise/"), "bar"));
  dep.set_output_type(Target::RUST_LIBRARY);
  dep.visibility().SetPublic();
  SourceFile tlib("//tortoise/lib.rs");
  dep.sources().push_back(tlib);
  dep.source_types_used().Set(SourceFile::SOURCE_RS);
  dep.rust_values().set_crate_root(tlib);
  dep.rust_values().crate_name() = "tortoise";
  dep.SetToolchain(setup.toolchain());

  Target dep2(setup.settings(), Label(SourceDir("//achilles/"), "bar"));
  dep2.set_output_type(Target::RUST_LIBRARY);
  dep2.visibility().SetPublic();
  SourceFile alib("//achilles/lib.rs");
  dep2.sources().push_back(alib);
  dep2.source_types_used().Set(SourceFile::SOURCE_RS);
  dep2.rust_values().set_crate_root(alib);
  dep2.rust_values().crate_name() = "achilles";
  dep2.SetToolchain(setup.toolchain());

  Target target(setup.settings(), Label(SourceDir("//hare/"), "bar"));
  target.set_output_type(Target::RUST_LIBRARY);
  target.visibility().SetPublic();
  SourceFile harelib("//hare/lib.rs");
  target.sources().push_back(harelib);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(harelib);
  target.rust_values().crate_name() = "hare";
  target.public_deps().push_back(LabelTargetPair(&dep));
  target.public_deps().push_back(LabelTargetPair(&dep2));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream stream;
  std::vector<const Target*> targets;
  targets.push_back(&target);
  RustProjectWriter::RenderJSON(setup.build_settings(), targets, stream);
  std::string out = stream.str();
#if defined(OS_WIN)
  base::ReplaceSubstringsAfterOffset(&out, 0, "\r\n", "\n");
#endif
  const char expected_json[] =
      "{\n"
      "  \"roots\": [],\n"
      "  \"crates\": [\n"
      "    {\n"
      "      \"crate_id\": 0,\n"
      "      \"root_module\": \"tortoise/lib.rs\",\n"
      "      \"deps\": [\n"
      "      ],\n"
      "      \"edition\": \"2015\",\n"
      "      \"atom_cfgs\": [\n"
      "      ],\n"
      "      \"key_value_cfgs\": {\n"
      "      }\n"
      "    },\n"
      "    {\n"
      "      \"crate_id\": 1,\n"
      "      \"root_module\": \"achilles/lib.rs\",\n"
      "      \"deps\": [\n"
      "      ],\n"
      "      \"edition\": \"2015\",\n"
      "      \"atom_cfgs\": [\n"
      "      ],\n"
      "      \"key_value_cfgs\": {\n"
      "      }\n"
      "    },\n"
      "    {\n"
      "      \"crate_id\": 2,\n"
      "      \"root_module\": \"hare/lib.rs\",\n"
      "      \"deps\": [\n"
      "        {\n"
      "          \"crate\": 0,\n"
      "          \"name\": \"tortoise\"\n"
      "        },\n"
      "        {\n"
      "          \"crate\": 1,\n"
      "          \"name\": \"achilles\"\n"
      "        }\n"
      "      ],\n"
      "      \"edition\": \"2015\",\n"
      "      \"atom_cfgs\": [\n"
      "      ],\n"
      "      \"key_value_cfgs\": {\n"
      "      }\n"
      "    }\n"
      "  ]\n"
      "}\n";
  EXPECT_EQ(expected_json, out);
}

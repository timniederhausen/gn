// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/functions.h"
#include "gn/rust_tool.h"
#include "gn/scheduler.h"
#include "gn/test_with_scheduler.h"
#include "gn/test_with_scope.h"
#include "util/test/test.h"

using FunctionToolchain = TestWithScheduler;

TEST_F(FunctionToolchain, NoArguments) {
  TestWithScope setup;

  // Check that creating a toolchain with no name reports an error.
  {
    TestParseInput input(R"(toolchain() {})");
    ASSERT_FALSE(input.has_error());

    Err err;
    input.parsed()->Execute(setup.scope(), &err);
    ASSERT_TRUE(err.has_error()) << err.message();
  }

  // Check that creating a toolchain with too many arguments is an error.
  {
    TestParseInput input(R"(toolchain("too", "many", "arguments") {})");
    ASSERT_FALSE(input.has_error());

    Err err;
    input.parsed()->Execute(setup.scope(), &err);
    ASSERT_TRUE(err.has_error()) << err.message();
  }
}

TEST_F(FunctionToolchain, RuntimeOutputs) {
  TestWithScope setup;

  // These runtime outputs are a subset of the outputs so are OK.
  {
    TestParseInput input(
        R"(toolchain("good") {
          tool("link") {
            command = "link"
            outputs = [ "foo" ]
            runtime_outputs = [ "foo" ]
          }
        })");
    ASSERT_FALSE(input.has_error());

    Err err;
    input.parsed()->Execute(setup.scope(), &err);
    ASSERT_FALSE(err.has_error()) << err.message();

    // It should have generated a toolchain.
    ASSERT_EQ(1u, setup.items().size());
    const Toolchain* toolchain = setup.items()[0]->AsToolchain();
    ASSERT_TRUE(toolchain);

    // The toolchain should have a link tool with the two outputs.
    const Tool* link = toolchain->GetTool(CTool::kCToolLink);
    ASSERT_TRUE(link);
    ASSERT_EQ(1u, link->outputs().list().size());
    EXPECT_EQ("foo", link->outputs().list()[0].AsString());
    ASSERT_EQ(1u, link->runtime_outputs().list().size());
    EXPECT_EQ("foo", link->runtime_outputs().list()[0].AsString());
  }

  // This one is not a subset so should throw an error.
  {
    TestParseInput input(
        R"(toolchain("bad") {
          tool("link") {
            outputs = [ "foo" ]
            runtime_outputs = [ "bar" ]
          }
        })");
    ASSERT_FALSE(input.has_error());

    Err err;
    input.parsed()->Execute(setup.scope(), &err);
    ASSERT_TRUE(err.has_error()) << err.message();
  }
}

TEST_F(FunctionToolchain, Rust) {
  TestWithScope setup;

  // These runtime outputs are a subset of the outputs so are OK.
  {
    TestParseInput input(
        R"(toolchain("rust") {
          tool("rust_bin") {
            command = "{{rustenv}} rustc --crate-name {{crate_name}} --crate-type bin {{rustflags}} -o {{output}} {{externs}} {{source}}"
            description = "RUST {{output}}"
          }
        })");
    ASSERT_FALSE(input.has_error());

    Err err;
    input.parsed()->Execute(setup.scope(), &err);
    ASSERT_FALSE(err.has_error()) << err.message();

    // It should have generated a toolchain.
    ASSERT_EQ(1u, setup.items().size());
    const Toolchain* toolchain = setup.items()[0]->AsToolchain();
    ASSERT_TRUE(toolchain);

    const Tool* rust = toolchain->GetTool(RustTool::kRsToolBin);
    ASSERT_TRUE(rust);
    ASSERT_EQ(rust->command().AsString(),
              "{{rustenv}} rustc --crate-name {{crate_name}} --crate-type bin "
              "{{rustflags}} -o {{output}} {{externs}} {{source}}");
    ASSERT_EQ(rust->description().AsString(), "RUST {{output}}");
  }
}

TEST_F(FunctionToolchain, RustRuntimeOutputs) {
  TestWithScope setup;

  // These runtime outputs are a subset of the outputs so are OK.
  {
    TestParseInput input(
        R"(toolchain("good") {
          tool("rust_dylib") {
            command = "rust_dylib"
            outputs = [ "foo" ]
            runtime_outputs = [ "foo" ]
          }
        })");
    ASSERT_FALSE(input.has_error());

    Err err;
    input.parsed()->Execute(setup.scope(), &err);
    ASSERT_FALSE(err.has_error()) << err.message();

    // It should have generated a toolchain.
    ASSERT_EQ(1u, setup.items().size());
    const Toolchain* toolchain = setup.items()[0]->AsToolchain();
    ASSERT_TRUE(toolchain);

    // The toolchain should have a link tool with the two outputs.
    const Tool* link = toolchain->GetTool(RustTool::kRsToolDylib);
    ASSERT_TRUE(link);
    ASSERT_EQ(1u, link->outputs().list().size());
    EXPECT_EQ("foo", link->outputs().list()[0].AsString());
    ASSERT_EQ(1u, link->runtime_outputs().list().size());
    EXPECT_EQ("foo", link->runtime_outputs().list()[0].AsString());
  }

  // This one is not a subset so should throw an error.
  {
    TestParseInput input(
        R"(toolchain("bad") {
          tool("rust_dylib") {
            outputs = [ "foo" ]
            runtime_outputs = [ "bar" ]
          }
        })");
    ASSERT_FALSE(input.has_error());

    Err err;
    input.parsed()->Execute(setup.scope(), &err);
    ASSERT_TRUE(err.has_error()) << err.message();
  }
}

TEST_F(FunctionToolchain, RustLinkDependAndRuntimeOutputs) {
  TestWithScope setup;

  // These runtime outputs are a subset of the outputs so are OK.
  {
    TestParseInput input(
        R"(toolchain("good") {
          tool("rust_dylib") {
            command = "rust_dylib"
            outputs = [ "interface", "lib", "unstripped", "stripped" ]
            depend_output = "interface"
            link_output = "lib"
            runtime_outputs = [ "stripped" ]
          }
        })");
    ASSERT_FALSE(input.has_error());

    Err err;
    input.parsed()->Execute(setup.scope(), &err);
    ASSERT_FALSE(err.has_error()) << err.message();

    // It should have generated a toolchain.
    ASSERT_EQ(1u, setup.items().size());
    const Toolchain* toolchain = setup.items()[0]->AsToolchain();
    ASSERT_TRUE(toolchain);

    // The toolchain should have a link tool with the two outputs.
    const Tool* link = toolchain->GetTool(RustTool::kRsToolDylib);
    ASSERT_TRUE(link);
    ASSERT_EQ(4u, link->outputs().list().size());
    EXPECT_EQ("interface", link->outputs().list()[0].AsString());
    EXPECT_EQ("lib", link->outputs().list()[1].AsString());
    EXPECT_EQ("unstripped", link->outputs().list()[2].AsString());
    EXPECT_EQ("stripped", link->outputs().list()[3].AsString());
    ASSERT_EQ(1u, link->runtime_outputs().list().size());
    EXPECT_EQ("stripped", link->runtime_outputs().list()[0].AsString());

    const RustTool* rust_tool = link->AsRust();
    ASSERT_TRUE(rust_tool);
    EXPECT_EQ("interface", rust_tool->depend_output().AsString());
    EXPECT_EQ("lib", rust_tool->link_output().AsString());
  }
}

TEST_F(FunctionToolchain, Command) {
  TestWithScope setup;

  TestParseInput input(
      R"(toolchain("missing_command") {
        tool("cxx") {}
      })");
  ASSERT_FALSE(input.has_error());

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  ASSERT_TRUE(err.has_error()) << err.message();
}

TEST_F(FunctionToolchain, CommandLauncher) {
  TestWithScope setup;

  TestParseInput input(
      R"(toolchain("good") {
        tool("cxx") {
          command = "cxx"
          command_launcher = "/usr/goma/gomacc"
        }
      })");
  ASSERT_FALSE(input.has_error());

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();

  // It should have generated a toolchain.
  ASSERT_EQ(1u, setup.items().size());
  const Toolchain* toolchain = setup.items()[0]->AsToolchain();
  ASSERT_TRUE(toolchain);

  // The toolchain should have a link tool with the two outputs.
  const Tool* link = toolchain->GetTool(CTool::kCToolCxx);
  ASSERT_TRUE(link);
  EXPECT_EQ("/usr/goma/gomacc", link->command_launcher());
}

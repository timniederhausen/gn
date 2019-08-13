// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_rust_binary_target_writer.h"

#include "tools/gn/config.h"
#include "tools/gn/rust_values.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scheduler.h"
#include "tools/gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

using NinjaRustBinaryTargetWriterTest = TestWithScheduler;

TEST_F(NinjaRustBinaryTargetWriterTest, RustSourceSet) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::SOURCE_SET);
  target.visibility().SetPublic();
  target.sources().push_back(SourceFile("//foo/input1.rs"));
  target.sources().push_back(SourceFile("//foo/main.rs"));
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  // Source set itself.
  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/bar.stamp: stamp ../../foo/input1.rs "
        "../../foo/main.rs\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RustExecutable) {
  Err err;
  TestWithScope setup;

  Target source_set(setup.settings(), Label(SourceDir("//foo/"), "sources"));
  source_set.set_output_type(Target::SOURCE_SET);
  source_set.visibility().SetPublic();
  source_set.sources().push_back(SourceFile("//foo/input1.rs"));
  source_set.sources().push_back(SourceFile("//foo/input2.rs"));
  source_set.source_types_used().Set(SourceFile::SOURCE_RS);
  source_set.SetToolchain(setup.toolchain());
  ASSERT_TRUE(source_set.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/input3.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.rust_values().edition() = "2018";
  target.private_deps().push_back(LabelTargetPair(&source_set));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_dir = \n"
        "rustc_output_extension = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/foo_bar: rustc ../../foo/main.rs | ../../foo/input3.rs "
        "../../foo/main.rs ../../foo/input1.rs ../../foo/input2.rs || "
        "obj/foo/sources.stamp\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RlibDeps) {
  Err err;
  TestWithScope setup;

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.rust_values().edition() = "2018";
  rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(rlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&rlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = mylib\n"
        "crate_type = rlib\n"
        "output_dir = \n"
        "rustc_output_extension = .rlib\n"
        "rustc_output_prefix = lib\n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = mylib\n"
        "\n"
        "build obj/bar/libmylib.rlib: rustc ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << out_str;
  }

  Target another_rlib(setup.settings(), Label(SourceDir("//foo/"), "direct"));
  another_rlib.set_output_type(Target::RUST_LIBRARY);
  another_rlib.visibility().SetPublic();
  SourceFile lib("//foo/main.rs");
  another_rlib.sources().push_back(SourceFile("//foo/direct.rs"));
  another_rlib.sources().push_back(lib);
  another_rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  another_rlib.rust_values().set_crate_root(lib);
  another_rlib.rust_values().crate_name() = "direct";
  another_rlib.rust_values().edition() = "2018";
  another_rlib.SetToolchain(setup.toolchain());
  another_rlib.public_deps().push_back(LabelTargetPair(&rlib));
  ASSERT_TRUE(another_rlib.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.rust_values().edition() = "2018";
  target.private_deps().push_back(LabelTargetPair(&another_rlib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_dir = \n"
        "rustc_output_extension = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/foo_bar: rustc ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/foo/libdirect.rlib obj/bar/libmylib.rlib\n"
        "  externs = --extern direct=obj/foo/libdirect.rlib --extern "
        "mylib=obj/bar/libmylib.rlib\n"
        "  rustdeps = -Ldependency=obj/foo -Ldependency=obj/bar\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RenamedDeps) {
  Err err;
  TestWithScope setup;

  Target another_rlib(setup.settings(), Label(SourceDir("//foo/"), "direct"));
  another_rlib.set_output_type(Target::RUST_LIBRARY);
  another_rlib.visibility().SetPublic();
  SourceFile lib("//foo/lib.rs");
  another_rlib.sources().push_back(SourceFile("//foo/direct.rs"));
  another_rlib.sources().push_back(lib);
  another_rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  another_rlib.rust_values().set_crate_root(lib);
  another_rlib.rust_values().crate_name() = "direct";
  another_rlib.rust_values().edition() = "2018";
  another_rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(another_rlib.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.rust_values().aliased_deps()[another_rlib.label()] = "direct_renamed";
  target.rust_values().edition() = "2018";
  target.private_deps().push_back(LabelTargetPair(&another_rlib));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_dir = \n"
        "rustc_output_extension = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/foo_bar: rustc ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/foo/libdirect.rlib\n"
        "  externs = --extern direct_renamed=obj/foo/libdirect.rlib\n"
        "  rustdeps = -Ldependency=obj/foo\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, NonRustDeps) {
  Err err;
  TestWithScope setup;

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.rust_values().edition() = "2018";
  rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(rlib.OnResolved(&err));

  Target staticlib(setup.settings(), Label(SourceDir("//foo/"), "static"));
  staticlib.set_output_type(Target::STATIC_LIBRARY);
  staticlib.visibility().SetPublic();
  staticlib.sources().push_back(SourceFile("//foo/static.cpp"));
  staticlib.source_types_used().Set(SourceFile::SOURCE_CPP);
  staticlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(staticlib.OnResolved(&err));

  Target nonrust(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  nonrust.set_output_type(Target::EXECUTABLE);
  nonrust.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  nonrust.sources().push_back(SourceFile("//foo/source.rs"));
  nonrust.sources().push_back(main);
  nonrust.source_types_used().Set(SourceFile::SOURCE_RS);
  nonrust.rust_values().set_crate_root(main);
  nonrust.rust_values().crate_name() = "foo_bar";
  nonrust.rust_values().edition() = "2018";
  nonrust.private_deps().push_back(LabelTargetPair(&rlib));
  nonrust.private_deps().push_back(LabelTargetPair(&staticlib));
  nonrust.SetToolchain(setup.toolchain());
  ASSERT_TRUE(nonrust.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&nonrust, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_dir = \n"
        "rustc_output_extension = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/foo_bar: rustc ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/bar/libmylib.rlib obj/foo/libstatic.a\n"
        "  externs = --extern mylib=obj/bar/libmylib.rlib\n"
        "  rustdeps = -Ldependency=obj/bar -Lnative=obj/foo\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }

  Target nonrust_only(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  nonrust_only.set_output_type(Target::EXECUTABLE);
  nonrust_only.visibility().SetPublic();
  nonrust_only.sources().push_back(SourceFile("//foo/source.rs"));
  nonrust_only.sources().push_back(main);
  nonrust_only.source_types_used().Set(SourceFile::SOURCE_RS);
  nonrust_only.rust_values().set_crate_root(main);
  nonrust_only.rust_values().crate_name() = "foo_bar";
  nonrust_only.rust_values().edition() = "2018";
  nonrust_only.private_deps().push_back(LabelTargetPair(&staticlib));
  nonrust_only.SetToolchain(setup.toolchain());
  ASSERT_TRUE(nonrust_only.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&nonrust_only, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_dir = \n"
        "rustc_output_extension = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/foo_bar: rustc ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/foo/libstatic.a\n"
        "  rustdeps = -Lnative=obj/foo\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, RustOutputExtensionAndDir) {
  Err err;
  TestWithScope setup;

  Target source_set(setup.settings(), Label(SourceDir("//foo/"), "sources"));
  source_set.set_output_type(Target::SOURCE_SET);
  source_set.visibility().SetPublic();
  source_set.sources().push_back(SourceFile("//foo/input1.rs"));
  source_set.sources().push_back(SourceFile("//foo/input2.rs"));
  source_set.source_types_used().Set(SourceFile::SOURCE_RS);
  source_set.SetToolchain(setup.toolchain());
  ASSERT_TRUE(source_set.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/input3.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.set_output_extension(std::string("exe"));
  target.set_output_dir(SourceDir("//out/Debug/foo/"));
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.rust_values().edition() = "2018";
  target.private_deps().push_back(LabelTargetPair(&source_set));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_dir = foo\n"
        "rustc_output_extension = .exe\n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/foo_bar.exe: rustc ../../foo/main.rs | ../../foo/input3.rs "
        "../../foo/main.rs ../../foo/input1.rs ../../foo/input2.rs || "
        "obj/foo/sources.stamp\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, ProcMacro) {
  Err err;
  TestWithScope setup;

  Target procmacro(setup.settings(), Label(SourceDir("//bar/"), "mymacro"));
  procmacro.set_output_type(Target::LOADABLE_MODULE);
  procmacro.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  procmacro.sources().push_back(SourceFile("//bar/mylib.rs"));
  procmacro.sources().push_back(barlib);
  procmacro.source_types_used().Set(SourceFile::SOURCE_RS);
  procmacro.rust_values().set_crate_root(barlib);
  procmacro.rust_values().crate_name() = "mymacro";
  procmacro.rust_values().edition() = "2018";
  procmacro.rust_values().set_crate_type(RustValues::CRATE_PROC_MACRO);
  procmacro.SetToolchain(setup.toolchain());
  ASSERT_TRUE(procmacro.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&procmacro, out);
    writer.Run();

    const char expected[] =
        "crate_name = mymacro\n"
        "crate_type = proc-macro\n"
        "output_dir = \n"
        "rustc_output_extension = .so\n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = mymacro\n"
        "\n"
        "build obj/bar/libmymacro.so: rustc ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << out_str;
  }

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.rust_values().edition() = "2018";
  target.private_deps().push_back(LabelTargetPair(&procmacro));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_dir = \n"
        "rustc_output_extension = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/foo_bar: rustc ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs || obj/bar/libmymacro.so\n"
        "  externs = --extern mymacro=obj/bar/libmymacro.so\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

TEST_F(NinjaRustBinaryTargetWriterTest, GroupDeps) {
  Err err;
  TestWithScope setup;

  Target rlib(setup.settings(), Label(SourceDir("//bar/"), "mylib"));
  rlib.set_output_type(Target::RUST_LIBRARY);
  rlib.visibility().SetPublic();
  SourceFile barlib("//bar/lib.rs");
  rlib.sources().push_back(SourceFile("//bar/mylib.rs"));
  rlib.sources().push_back(barlib);
  rlib.source_types_used().Set(SourceFile::SOURCE_RS);
  rlib.rust_values().set_crate_root(barlib);
  rlib.rust_values().crate_name() = "mylib";
  rlib.rust_values().edition() = "2018";
  rlib.SetToolchain(setup.toolchain());
  ASSERT_TRUE(rlib.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&rlib, out);
    writer.Run();

    const char expected[] =
        "crate_name = mylib\n"
        "crate_type = rlib\n"
        "output_dir = \n"
        "rustc_output_extension = .rlib\n"
        "rustc_output_prefix = lib\n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/bar\n"
        "target_output_name = mylib\n"
        "\n"
        "build obj/bar/libmylib.rlib: rustc ../../bar/lib.rs | "
        "../../bar/mylib.rs ../../bar/lib.rs\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << out_str;
  }

  Target group(setup.settings(), Label(SourceDir("//baz/"), "group"));
  group.set_output_type(Target::GROUP);
  group.visibility().SetPublic();
  group.public_deps().push_back(LabelTargetPair(&rlib));
  group.SetToolchain(setup.toolchain());
  ASSERT_TRUE(group.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::EXECUTABLE);
  target.visibility().SetPublic();
  SourceFile main("//foo/main.rs");
  target.sources().push_back(SourceFile("//foo/source.rs"));
  target.sources().push_back(main);
  target.source_types_used().Set(SourceFile::SOURCE_RS);
  target.rust_values().set_crate_root(main);
  target.rust_values().crate_name() = "foo_bar";
  target.rust_values().edition() = "2018";
  target.private_deps().push_back(LabelTargetPair(&group));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaRustBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "crate_name = foo_bar\n"
        "crate_type = bin\n"
        "output_dir = \n"
        "rustc_output_extension = \n"
        "rustflags =\n"
        "rustenv =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/foo_bar: rustc ../../foo/main.rs | ../../foo/source.rs "
        "../../foo/main.rs obj/bar/libmylib.rlib || obj/baz/group.stamp\n"
        "  externs = --extern mylib=obj/bar/libmylib.rlib\n"
        "  rustdeps = -Ldependency=obj/bar\n"
        "  edition = 2018\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str) << expected << "\n" << out_str;
  }
}

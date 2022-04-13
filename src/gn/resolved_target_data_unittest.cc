// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/resolved_target_data.h"

#include "gn/test_with_scope.h"
#include "util/test/test.h"

// Tests that lib[_dir]s are inherited across deps boundaries for static
// libraries but not executables.
TEST(ResolvedTargetDataTest, LibInheritance) {
  TestWithScope setup;
  Err err;

  ResolvedTargetData resolved;

  const LibFile lib("foo");
  const SourceDir libdir("/foo_dir/");

  // Leaf target with ldflags set.
  TestTarget z(setup, "//foo:z", Target::STATIC_LIBRARY);
  z.config_values().libs().push_back(lib);
  z.config_values().lib_dirs().push_back(libdir);
  ASSERT_TRUE(z.OnResolved(&err));

  // All lib[_dir]s should be set when target is resolved.
  auto z_info = resolved.GetLibInfo(&z);
  ASSERT_EQ(1u, z_info.all_libs.size());
  EXPECT_EQ(lib, z_info.all_libs[0]);
  ASSERT_EQ(1u, z_info.all_lib_dirs.size());
  EXPECT_EQ(libdir, z_info.all_lib_dirs[0]);

  // Shared library target should inherit the libs from the static library
  // and its own. Its own flag should be before the inherited one.
  const LibFile second_lib("bar");
  const SourceDir second_libdir("/bar_dir/");
  TestTarget shared(setup, "//foo:shared", Target::SHARED_LIBRARY);
  shared.config_values().libs().push_back(second_lib);
  shared.config_values().lib_dirs().push_back(second_libdir);
  shared.private_deps().push_back(LabelTargetPair(&z));
  ASSERT_TRUE(shared.OnResolved(&err));

  const auto libinfo = resolved.GetLibInfo(&shared);
  ASSERT_EQ(2u, libinfo.all_libs.size());
  EXPECT_EQ(second_lib, libinfo.all_libs[0]);
  EXPECT_EQ(lib, libinfo.all_libs[1]);
  ASSERT_EQ(2u, libinfo.all_lib_dirs.size());
  EXPECT_EQ(second_libdir, libinfo.all_lib_dirs[0]);
  EXPECT_EQ(libdir, libinfo.all_lib_dirs[1]);

  // Executable target shouldn't get either by depending on shared.
  TestTarget exec(setup, "//foo:exec", Target::EXECUTABLE);
  exec.private_deps().push_back(LabelTargetPair(&shared));
  ASSERT_TRUE(exec.OnResolved(&err));
  auto exec_libinfo = resolved.GetLibInfo(&exec);
  EXPECT_EQ(0u, exec_libinfo.all_libs.size());
  EXPECT_EQ(0u, exec_libinfo.all_lib_dirs.size());
}

// Tests that framework[_dir]s are inherited across deps boundaries for static
// libraries but not executables.
TEST(ResolvedTargetDataTest, FrameworkInheritance) {
  TestWithScope setup;
  Err err;

  const std::string framework("Foo.framework");
  const SourceDir frameworkdir("//out/foo/");

  // Leaf target with ldflags set.
  TestTarget z(setup, "//foo:z", Target::STATIC_LIBRARY);
  z.config_values().frameworks().push_back(framework);
  z.config_values().framework_dirs().push_back(frameworkdir);
  ASSERT_TRUE(z.OnResolved(&err));

  ResolvedTargetData resolved;

  // All framework[_dir]s should be set when target is resolved.
  auto info = resolved.GetFrameworkInfo(&z);
  ASSERT_EQ(1u, info.all_frameworks.size());
  EXPECT_EQ(framework, info.all_frameworks[0]);
  ASSERT_EQ(1u, info.all_framework_dirs.size());
  EXPECT_EQ(frameworkdir, info.all_framework_dirs[0]);

  // Shared library target should inherit the libs from the static library
  // and its own. Its own flag should be before the inherited one.
  const std::string second_framework("Bar.framework");
  const SourceDir second_frameworkdir("//out/bar/");
  TestTarget shared(setup, "//foo:shared", Target::SHARED_LIBRARY);
  shared.config_values().frameworks().push_back(second_framework);
  shared.config_values().framework_dirs().push_back(second_frameworkdir);
  shared.private_deps().push_back(LabelTargetPair(&z));
  ASSERT_TRUE(shared.OnResolved(&err));

  auto shared_info = resolved.GetFrameworkInfo(&shared);
  ASSERT_EQ(2u, shared_info.all_frameworks.size());
  EXPECT_EQ(second_framework, shared_info.all_frameworks[0]);
  EXPECT_EQ(framework, shared_info.all_frameworks[1]);
  ASSERT_EQ(2u, shared_info.all_framework_dirs.size());
  EXPECT_EQ(second_frameworkdir, shared_info.all_framework_dirs[0]);
  EXPECT_EQ(frameworkdir, shared_info.all_framework_dirs[1]);

  // Executable target shouldn't get either by depending on shared.
  TestTarget exec(setup, "//foo:exec", Target::EXECUTABLE);
  exec.private_deps().push_back(LabelTargetPair(&shared));
  ASSERT_TRUE(exec.OnResolved(&err));
  auto exec_info = resolved.GetFrameworkInfo(&exec);
  EXPECT_EQ(0u, exec_info.all_frameworks.size());
  EXPECT_EQ(0u, exec_info.all_framework_dirs.size());
}

TEST(ResolvedTargetDataTest, InheritLibs) {
  TestWithScope setup;
  Err err;

  // Create a dependency chain:
  //   A (executable) -> B (shared lib) -> C (static lib) -> D (source set)
  TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
  TestTarget b(setup, "//foo:b", Target::SHARED_LIBRARY);
  TestTarget c(setup, "//foo:c", Target::STATIC_LIBRARY);
  TestTarget d(setup, "//foo:d", Target::SOURCE_SET);
  a.private_deps().push_back(LabelTargetPair(&b));
  b.private_deps().push_back(LabelTargetPair(&c));
  c.private_deps().push_back(LabelTargetPair(&d));

  ASSERT_TRUE(d.OnResolved(&err));
  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  ResolvedTargetData resolved;

  // C should have D in its inherited libs.
  auto c_inherited_libs = resolved.inherited_libraries(&c);
  ASSERT_EQ(1u, c_inherited_libs.size());
  EXPECT_EQ(&d, c_inherited_libs[0].target());

  // B should have C and D in its inherited libs.
  auto b_inherited = resolved.inherited_libraries(&b);
  ASSERT_EQ(2u, b_inherited.size());
  EXPECT_EQ(&c, b_inherited[0].target());
  EXPECT_EQ(&d, b_inherited[1].target());

  // A should have B in its inherited libs, but not any others (the shared
  // library will include the static library and source set).
  auto a_inherited = resolved.inherited_libraries(&a);
  ASSERT_EQ(1u, a_inherited.size());
  EXPECT_EQ(&b, a_inherited[0].target());
}

TEST(ResolvedTargetData, NoActionDepPropgation) {
  TestWithScope setup;
  Err err;
  ResolvedTargetData resolved;
  // Create a dependency chain:
  //   A (exe) -> B (action) -> C (source_set)
  {
    TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
    TestTarget b(setup, "//foo:b", Target::ACTION);
    TestTarget c(setup, "//foo:c", Target::SOURCE_SET);

    a.private_deps().push_back(LabelTargetPair(&b));
    b.private_deps().push_back(LabelTargetPair(&c));

    ASSERT_TRUE(c.OnResolved(&err));
    ASSERT_TRUE(b.OnResolved(&err));
    ASSERT_TRUE(a.OnResolved(&err));

    // The executable should not have inherited the source set across the
    // action.
    ASSERT_TRUE(resolved.inherited_libraries(&a).empty());
  }
}

TEST(ResolvedTargetDataTest, InheritCompleteStaticLib) {
  TestWithScope setup;
  Err err;

  ResolvedTargetData resolved;

  // Create a dependency chain:
  //   A (executable) -> B (complete static lib) -> C (source set)
  TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
  TestTarget b(setup, "//foo:b", Target::STATIC_LIBRARY);
  b.set_complete_static_lib(true);

  const LibFile lib("foo");
  const SourceDir lib_dir("/foo_dir/");
  TestTarget c(setup, "//foo:c", Target::SOURCE_SET);
  c.config_values().libs().push_back(lib);
  c.config_values().lib_dirs().push_back(lib_dir);

  a.public_deps().push_back(LabelTargetPair(&b));
  b.public_deps().push_back(LabelTargetPair(&c));

  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  // B should have C in its inherited libs.
  auto b_inherited = resolved.inherited_libraries(&b);
  ASSERT_EQ(1u, b_inherited.size());
  EXPECT_EQ(&c, b_inherited[0].target());

  // A should have B in its inherited libs, but not any others (the complete
  // static library will include the source set).
  auto a_inherited = resolved.inherited_libraries(&a);
  ASSERT_EQ(1u, a_inherited.size());
  EXPECT_EQ(&b, a_inherited[0].target());

  // A should inherit the libs and lib_dirs from the C.
  auto a_info = resolved.GetLibInfo(&a);
  ASSERT_EQ(1u, a_info.all_libs.size());
  EXPECT_EQ(lib, a_info.all_libs[0]);
  ASSERT_EQ(1u, a_info.all_lib_dirs.size());
  EXPECT_EQ(lib_dir, a_info.all_lib_dirs[0]);
}

TEST(ResolvedTargetDataTest, InheritCompleteStaticLibStaticLibDeps) {
  TestWithScope setup;
  Err err;

  // Create a dependency chain:
  //   A (executable) -> B (complete static lib) -> C (static lib)
  TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
  TestTarget b(setup, "//foo:b", Target::STATIC_LIBRARY);
  b.set_complete_static_lib(true);
  TestTarget c(setup, "//foo:c", Target::STATIC_LIBRARY);
  a.public_deps().push_back(LabelTargetPair(&b));
  b.public_deps().push_back(LabelTargetPair(&c));

  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  ResolvedTargetData resolved;

  // B should have C in its inherited libs.
  auto b_inherited = resolved.inherited_libraries(&b);
  ASSERT_EQ(1u, b_inherited.size());
  EXPECT_EQ(&c, b_inherited[0].target());

  // A should have B in its inherited libs, but not any others (the complete
  // static library will include the static library).
  auto a_inherited = resolved.inherited_libraries(&a);
  ASSERT_EQ(1u, a_inherited.size());
  EXPECT_EQ(&b, a_inherited[0].target());
}

TEST(ResolvedTargetDataTest,
     InheritCompleteStaticLibInheritedCompleteStaticLibDeps) {
  TestWithScope setup;
  Err err;

  // Create a dependency chain:
  //   A (executable) -> B (complete static lib) -> C (complete static lib)
  TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
  TestTarget b(setup, "//foo:b", Target::STATIC_LIBRARY);
  b.set_complete_static_lib(true);
  TestTarget c(setup, "//foo:c", Target::STATIC_LIBRARY);
  c.set_complete_static_lib(true);

  a.private_deps().push_back(LabelTargetPair(&b));
  b.private_deps().push_back(LabelTargetPair(&c));

  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  ResolvedTargetData resolved;

  // B should have C in its inherited libs.
  auto b_inherited = resolved.inherited_libraries(&b);
  ASSERT_EQ(1u, b_inherited.size());
  EXPECT_EQ(&c, b_inherited[0].target());

  // A should have B and C in its inherited libs.
  auto a_inherited = resolved.inherited_libraries(&a);
  ASSERT_EQ(2u, a_inherited.size());
  EXPECT_EQ(&b, a_inherited[0].target());
  EXPECT_EQ(&c, a_inherited[1].target());
}

TEST(ResolvedTargetDataTest, NoActionDepPropgation) {
  TestWithScope setup;
  Err err;
  ResolvedTargetData resolved;

  // Create a dependency chain:
  //   A (exe) -> B (action) -> C (source_set)
  {
    TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
    TestTarget b(setup, "//foo:b", Target::ACTION);
    TestTarget c(setup, "//foo:c", Target::SOURCE_SET);

    a.private_deps().push_back(LabelTargetPair(&b));
    b.private_deps().push_back(LabelTargetPair(&c));

    ASSERT_TRUE(c.OnResolved(&err));
    ASSERT_TRUE(b.OnResolved(&err));
    ASSERT_TRUE(a.OnResolved(&err));

    // The executable should not have inherited the source set across the
    // action.
    auto libs = resolved.inherited_libraries(&a);
    ASSERT_TRUE(libs.empty());
  }
}

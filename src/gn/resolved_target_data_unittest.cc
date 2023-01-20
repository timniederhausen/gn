// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/resolved_target_data.h"

#include "gn/test_with_scope.h"
#include "util/test/test.h"

// Tests that lib[_dir]s are inherited across deps boundaries for static
// libraries but not executables.
TEST(ResolvedTargetDataTest, GetTargetDeps) {
  TestWithScope setup;
  Err err;

  TestTarget a(setup, "//foo:a", Target::GROUP);
  TestTarget b(setup, "//foo:b", Target::GROUP);
  TestTarget c(setup, "//foo:c", Target::GROUP);
  TestTarget d(setup, "//foo:d", Target::GROUP);
  TestTarget e(setup, "//foo:e", Target::GROUP);

  a.private_deps().push_back(LabelTargetPair(&b));
  a.private_deps().push_back(LabelTargetPair(&c));
  a.public_deps().push_back(LabelTargetPair(&d));
  a.data_deps().push_back(LabelTargetPair(&e));

  b.private_deps().push_back(LabelTargetPair(&e));

  ASSERT_TRUE(e.OnResolved(&err));
  ASSERT_TRUE(d.OnResolved(&err));
  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  ResolvedTargetData resolved;

  const auto& a_deps = resolved.GetTargetDeps(&a);
  EXPECT_EQ(a_deps.size(), 4u);
  EXPECT_EQ(a_deps.private_deps().size(), 2u);
  EXPECT_EQ(a_deps.private_deps()[0], &b);
  EXPECT_EQ(a_deps.private_deps()[1], &c);
  EXPECT_EQ(a_deps.public_deps().size(), 1u);
  EXPECT_EQ(a_deps.public_deps()[0], &d);
  EXPECT_EQ(a_deps.data_deps().size(), 1u);
  EXPECT_EQ(a_deps.data_deps()[0], &e);

  const auto& b_deps = resolved.GetTargetDeps(&b);
  EXPECT_EQ(b_deps.size(), 1u);
  EXPECT_EQ(b_deps.private_deps().size(), 1u);
  EXPECT_EQ(b_deps.private_deps()[0], &e);
  EXPECT_EQ(b_deps.public_deps().size(), 0u);
  EXPECT_EQ(b_deps.data_deps().size(), 0u);
}

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
  const auto& all_libs = resolved.GetLinkedLibraries(&z);
  ASSERT_EQ(1u, all_libs.size());
  EXPECT_EQ(lib, all_libs[0]);

  const auto& all_lib_dirs = resolved.GetLinkedLibraryDirs(&z);
  ASSERT_EQ(1u, all_lib_dirs.size());
  EXPECT_EQ(libdir, all_lib_dirs[0]);

  // Shared library target should inherit the libs from the static library
  // and its own. Its own flag should be before the inherited one.
  const LibFile second_lib("bar");
  const SourceDir second_libdir("/bar_dir/");
  TestTarget shared(setup, "//foo:shared", Target::SHARED_LIBRARY);
  shared.config_values().libs().push_back(second_lib);
  shared.config_values().lib_dirs().push_back(second_libdir);
  shared.private_deps().push_back(LabelTargetPair(&z));
  ASSERT_TRUE(shared.OnResolved(&err));

  const auto& all_libs2 = resolved.GetLinkedLibraries(&shared);
  ASSERT_EQ(2u, all_libs2.size());
  EXPECT_EQ(second_lib, all_libs2[0]);
  EXPECT_EQ(lib, all_libs2[1]);

  const auto& all_lib_dirs2 = resolved.GetLinkedLibraryDirs(&shared);
  ASSERT_EQ(2u, all_lib_dirs2.size());
  EXPECT_EQ(second_libdir, all_lib_dirs2[0]);
  EXPECT_EQ(libdir, all_lib_dirs2[1]);

  // Executable target shouldn't get either by depending on shared.
  TestTarget exec(setup, "//foo:exec", Target::EXECUTABLE);
  exec.private_deps().push_back(LabelTargetPair(&shared));
  ASSERT_TRUE(exec.OnResolved(&err));

  const auto& all_libs3 = resolved.GetLinkedLibraries(&exec);
  EXPECT_EQ(0u, all_libs3.size());

  const auto& all_lib_dirs3 = resolved.GetLinkedLibraryDirs(&exec);
  EXPECT_EQ(0u, all_lib_dirs3.size());
}

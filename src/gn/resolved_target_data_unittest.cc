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

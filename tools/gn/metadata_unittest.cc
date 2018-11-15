// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/metadata.h"
#include "tools/gn/test_with_scope.h"
#include "util/test/test.h"

TEST(MetadataTest, SetContents) {
  Metadata metadata;

  ASSERT_TRUE(metadata.contents().empty());

  Value a_expected(nullptr, Value::LIST);
  a_expected.list_value().push_back(Value(nullptr, "foo"));
  Value b_expected(nullptr, Value::LIST);
  b_expected.list_value().push_back(Value(nullptr, true));

  Metadata::Contents contents;
  contents.insert(std::pair<base::StringPiece, Value>("a", a_expected));
  contents.insert(std::pair<base::StringPiece, Value>("b", b_expected));

  metadata.set_contents(std::move(contents));

  ASSERT_EQ(metadata.contents().size(), 2);
  auto a_actual = metadata.contents().find("a");
  auto b_actual = metadata.contents().find("b");
  ASSERT_FALSE(a_actual == metadata.contents().end());
  ASSERT_FALSE(b_actual == metadata.contents().end());
  ASSERT_EQ(a_actual->second, a_expected);
  ASSERT_EQ(b_actual->second, b_expected);
}

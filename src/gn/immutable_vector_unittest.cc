// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/immutable_vector.h"
#include "util/test/test.h"

#include <set>
#include <vector>

TEST(ImmutableVector, CreationDestruction) {
  ImmutableVector<int> empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(0u, empty.size());

  ImmutableVector<int> vec1 = {100, 42};
  EXPECT_FALSE(vec1.empty());
  EXPECT_EQ(2u, vec1.size());
  EXPECT_EQ(100, vec1.front());
  EXPECT_EQ(42, vec1.back());
  EXPECT_EQ(100, vec1[0]);
  EXPECT_EQ(42, vec1[1]);
  EXPECT_TRUE(vec1.begin());
  EXPECT_TRUE(vec1.end());
  EXPECT_NE(vec1.begin(), vec1.end());
  EXPECT_EQ(vec1.begin() + 2, vec1.end());

  std::vector<int> input;
  input.push_back(100);
  input.push_back(42);
  input.push_back(-12);
  ImmutableVector<int> vec2(input);
  EXPECT_FALSE(vec2.empty());
  EXPECT_EQ(3u, vec2.size());
  EXPECT_EQ(100, vec2.front());
  EXPECT_EQ(100, vec2[0]);
  EXPECT_EQ(42, vec2[1]);
  EXPECT_EQ(-12, vec2[2]);
  EXPECT_NE(vec2.begin(), &input[0]);
  EXPECT_NE(vec2.end(), &input[0] + 3);
}

TEST(ImmutableVetor, InPlaceConstruction) {
  size_t count = 0;
  auto count_producer = [&count]() { return count++; };
  ImmutableVector<int> vec(count_producer, 5u);
  EXPECT_EQ(5u, vec.size());
  EXPECT_EQ(0, vec[0]);
  EXPECT_EQ(1, vec[1]);
  EXPECT_EQ(2, vec[2]);
  EXPECT_EQ(3, vec[3]);
  EXPECT_EQ(4, vec[4]);
}

TEST(ImmutableVector, CopyAndMoveOperations) {
  ImmutableVector<int> vec1 = {1, 2, 3, 4};
  ImmutableVector<int> vec2 = vec1;
  ImmutableVector<int> vec3 = std::move(vec1);

  EXPECT_TRUE(vec1.empty());
  EXPECT_EQ(4u, vec2.size());
  EXPECT_EQ(4u, vec3.size());
  EXPECT_NE(vec2.begin(), vec3.begin());
  EXPECT_NE(vec2.end(), vec3.end());
  EXPECT_TRUE(std::equal(vec2.begin(), vec2.end(), vec3.begin(), vec3.end()));
}

TEST(ImmutableVectorView, Creation) {
  ImmutableVector<int> vec1 = {1, 3, 5, 7};
  ImmutableVectorView<int> view1 = vec1;
  ImmutableVectorView<int> view2(view1);

  EXPECT_EQ(vec1.size(), view1.size());
  EXPECT_EQ(vec1.size(), view2.size());

  EXPECT_EQ(vec1.begin(), view1.begin());
  EXPECT_EQ(vec1.end(), view1.end());

  EXPECT_EQ(vec1.begin(), view2.begin());
  EXPECT_EQ(vec1.end(), view2.end());
}

#include "gn/tagged_pointer.h"
#include "util/test/test.h"

struct Point {
  double x;
  double y;
};

TEST(TaggedPointer, Creation) {
  TaggedPointer<Point, 2> ptr;

  EXPECT_FALSE(ptr.ptr());
  EXPECT_EQ(0u, ptr.tag());

  Point point1 = {1., 2.};
  TaggedPointer<Point, 2> ptr2(&point1, 2);
  EXPECT_EQ(&point1, ptr2.ptr());
  EXPECT_EQ(2u, ptr2.tag());
}

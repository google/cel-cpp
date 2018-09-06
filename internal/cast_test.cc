#include "internal/cast.h"

#include <limits>

#include "gtest/gtest.h"

namespace google {
namespace api {
namespace expr {
namespace internal {
namespace {

struct A {};
struct B : A {};
struct C {};

TEST(CastTest, RepAs_Numeric) {
  EXPECT_TRUE(representable_as<int64_t>(1));
  EXPECT_TRUE(representable_as<int64_t>(1u));
  EXPECT_TRUE(representable_as<int64_t>(1.5));
  EXPECT_FALSE(representable_as<int64_t>(std::numeric_limits<uint64_t>::max()));
  EXPECT_FALSE(representable_as<int64_t>(std::numeric_limits<double>::max()));
  EXPECT_FALSE(
      representable_as<int64_t>(std::numeric_limits<double>::infinity()));

  EXPECT_TRUE(representable_as<uint64_t>(1));
  EXPECT_TRUE(representable_as<uint64_t>(1u));
  EXPECT_TRUE(representable_as<uint64_t>(1.5));
  EXPECT_FALSE(representable_as<uint64_t>(-1));
  EXPECT_FALSE(representable_as<uint64_t>(-1.0));
  EXPECT_FALSE(
      representable_as<uint64_t>(std::numeric_limits<double>::infinity()));

  EXPECT_TRUE(representable_as<double>(1));
  EXPECT_TRUE(representable_as<double>(1u));
  EXPECT_TRUE(representable_as<double>(1.5));
  EXPECT_FALSE(representable_as<float>(std::numeric_limits<double>::max()));
  EXPECT_TRUE(representable_as<float>(std::numeric_limits<double>::infinity()));
  EXPECT_TRUE(
      representable_as<float>(std::numeric_limits<double>::quiet_NaN()));
}

TEST(CastTest, RepAs_Value) {
  A a;
  B b;
  C c;

  // Representable as self.
  EXPECT_TRUE(representable_as<A>(a));
  EXPECT_TRUE(representable_as<A&>(a));
  EXPECT_TRUE(representable_as<A*>(&a));

  // Representable as ref or pointer to base class.
  EXPECT_FALSE(representable_as<A>(b));
  EXPECT_TRUE(representable_as<A&>(b));
  EXPECT_TRUE(representable_as<A*>(&b));

  // Defaults to false when conversion would be required.
  EXPECT_FALSE(representable_as<A>(b));
  EXPECT_FALSE(representable_as<A>(c));

  // Bad down casting.
  EXPECT_FALSE(representable_as<B&>(a));
  EXPECT_FALSE(representable_as<B*>(&a));

  // Down casting not currently supported
  A& b_as_a = b;
  EXPECT_FALSE(representable_as<B&>(b_as_a));
  EXPECT_FALSE(representable_as<B&>(&b_as_a));
}

TEST(CastTest, CopyIf) {
  A* null_ptr = nullptr;
  std::unique_ptr<A> null_uptr;
  A a;
  EXPECT_EQ(absl::nullopt, copy_if(null_ptr));
  EXPECT_TRUE(copy_if(&a).has_value());
  EXPECT_EQ(absl::nullopt, copy_if<A>(null_uptr));
}

}  // namespace
}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

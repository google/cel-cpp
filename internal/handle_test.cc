#include "internal/handle.h"

#include "gtest/gtest.h"
#include "absl/container/node_hash_set.h"

namespace google {
namespace api {
namespace expr {
namespace internal {
namespace {

class IntHandle : public Handle<int, IntHandle> {
 public:
  constexpr explicit IntHandle(int value) : Handle(value) {}
};

class OtherIntHandle : public Handle<int, OtherIntHandle> {
 public:
  constexpr explicit OtherIntHandle(int value) : Handle(value) {}
};

// Should be usable in a constexpr.
constexpr IntHandle kOne = IntHandle(1);
constexpr bool kOneVsTwo = kOne == IntHandle(2) && kOne != IntHandle(2) &&
                           kOne < IntHandle(2) && kOne <= IntHandle(2) &&
                           kOne > IntHandle(2) && kOne >= IntHandle(2);

TEST(Handle, TypeSafty) {
  auto convertible = std::is_convertible<IntHandle, OtherIntHandle>::value;
  EXPECT_FALSE(convertible);
  convertible = std::is_convertible<IntHandle, int>::value;
  EXPECT_FALSE(convertible);
}

TEST(Handle, Operators) {
  EXPECT_TRUE(IntHandle(1) == IntHandle(1));
  EXPECT_TRUE(IntHandle(1) != IntHandle(2));
  EXPECT_TRUE(IntHandle(1) < IntHandle(2));
  EXPECT_TRUE(IntHandle(1) <= IntHandle(2));
  EXPECT_TRUE(IntHandle(2) > IntHandle(1));
  EXPECT_TRUE(IntHandle(2) >= IntHandle(1));
}

TEST(Handle, Hash) {
  absl::node_hash_set<IntHandle, IntHandle::Hasher> handles;
  handles.emplace(1);
  handles.insert(IntHandle(1));
  handles.emplace(2);

  EXPECT_EQ(handles.size(), 2);
}

TEST(Handle, Order) {
  std::set<IntHandle> handles;
  handles.emplace(1);
  handles.insert(IntHandle(1));
  handles.emplace(2);

  EXPECT_EQ(handles.size(), 2);
}

}  // namespace
}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

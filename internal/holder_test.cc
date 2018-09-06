#include "internal/holder.h"

#include "gtest/gtest.h"
#include "absl/strings/str_cat.h"
#include "internal/types.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace internal {
namespace {

TEST(Holder, Access) {
  Holder<std::string, Copy> holder;

  // Can be accessed like a smart pointer (similar to std::optional)
  EXPECT_EQ("", *holder);
  EXPECT_TRUE(holder->empty());
  *holder = "hi";
  EXPECT_EQ("hi", holder.value());
  EXPECT_FALSE(holder->empty());
  holder.value() = "bye";
  EXPECT_EQ("bye", holder.value());
  EXPECT_FALSE(holder->empty());
}

TEST(Holder, Copy) {
  // Copy supports all modes
  using HolderType = Holder<int, Copy>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);

  // Value can be mutated.
  HolderType holder(1);
  testutil::ExpectSameType<int&, decltype(holder.value())>();
  EXPECT_EQ(1, holder.value());
  holder.value() = 2;
  EXPECT_EQ(2, holder.value());

  // Const holder cannot be assigned or have its value changed.
  EXPECT_TRUE(std::is_copy_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<const HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_move_assignable<const HolderType>::value);
  const HolderType const_holder(2);
  testutil::ExpectSameType<const int&, decltype(const_holder.value())>();
  EXPECT_EQ(2, const_holder.value());
}

TEST(Holder, Copy_const) {
  // Const Copy supports all modes.
  using HolderType = Holder<const int, Copy>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);

  // Value cannot be changed.
  HolderType holder(1);
  testutil::ExpectSameType<const int&, decltype(holder.value())>();
  EXPECT_EQ(1, holder.value());

  // Const holder has the same properties.
  EXPECT_TRUE(std::is_copy_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<const HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_move_assignable<const HolderType>::value);
  const HolderType const_holder(2);
  testutil::ExpectSameType<const int&, decltype(const_holder.value())>();
  EXPECT_EQ(2, const_holder.value());
}

TEST(Holder, OwnedPtr) {
  // OwnedPtr can only be moved.
  using HolderType = Holder<int, OwnedPtr>;
  EXPECT_FALSE(std::is_copy_constructible<HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);

  // Null cannot be accessed.
  HolderType holder;
  testutil::ExpectSameType<int&, decltype(holder.value())>();
#ifndef NDEBUG  // Assert only throws when debugging.
  EXPECT_DEATH(holder.value(), "null");
  holder = HolderType(nullptr);
  EXPECT_DEATH(holder.value(), "null");
#endif

  // Value can be mutated.
  holder = HolderType(absl::make_unique<int>(1));
  EXPECT_EQ(1, holder.value());
  holder.value() = 2;
  EXPECT_EQ(2, holder.value());

  // Const holder is not assignable, and cannot have its value changed.
  EXPECT_FALSE(std::is_copy_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<const HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_move_assignable<const HolderType>::value);
  const HolderType const_holder(absl::make_unique<int>(2));
  testutil::ExpectSameType<const int&, decltype(const_holder.value())>();
  EXPECT_EQ(2, const_holder.value());
}

TEST(Holder, OwnedPtr_const) {
  // OwnedPtr of a const value cannot be copied, but can be assigned.
  using HolderType = Holder<const int, OwnedPtr>;
  EXPECT_FALSE(std::is_copy_constructible<HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);

  // Null cannot be accessed.
  HolderType holder;
#ifndef NDEBUG  // Assert only throws when debugging.
  EXPECT_DEATH(holder.value(), "null");
  holder = HolderType(nullptr);
  EXPECT_DEATH(holder.value(), "null");
#endif

  // Value cannot be changed.
  testutil::ExpectSameType<const int&, decltype(holder.value())>();
  // Holder can be assigned.
  holder = HolderType(absl::make_unique<int>(1));
  EXPECT_EQ(1, holder.value());
  holder = HolderType(absl::make_unique<const int>(2));
  EXPECT_EQ(2, holder.value());

  // Const version const only be moved.
  EXPECT_FALSE(std::is_copy_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<const HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_move_assignable<const HolderType>::value);
  const HolderType const_holder(absl::make_unique<int>(3));
  testutil::ExpectSameType<const int&, decltype(const_holder.value())>();
  EXPECT_EQ(3, const_holder.value());
}

TEST(Holder, UnownedPtr) {
  // UnownedPtr supports all modes.
  using HolderType = Holder<int, UnownedPtr>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);

  // Null cannot be accessed.
  HolderType holder(static_cast<int*>(0));
#ifndef NDEBUG  // Assert only throws when debugging.
  EXPECT_DEATH(holder.value(), "null");
  holder = HolderType(nullptr);
  EXPECT_DEATH(holder.value(), "null");
#endif

  // Value can be mutated
  testutil::ExpectSameType<int&, decltype(holder.value())>();
  int i = 1;
  holder = HolderType(&i);
  EXPECT_EQ(1, holder.value());
  holder.value() = 2;
  EXPECT_EQ(2, holder.value());
  EXPECT_EQ(2, i);

  // Const holder cannot be assigned, and value cannot be changed.
  EXPECT_TRUE(std::is_copy_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<const HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_move_assignable<const HolderType>::value);
  const HolderType const_holder(&i);
  testutil::ExpectSameType<const int&, decltype(const_holder.value())>();
  EXPECT_EQ(2, const_holder.value());
  EXPECT_EQ(&holder.value(), &const_holder.value());
}

TEST(Holder, UnownedPtr_const) {
  // UnownedPtr to a const value supports all modes.
  using HolderType = Holder<const int, UnownedPtr>;
  EXPECT_TRUE(std::is_copy_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_copy_assignable<HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<HolderType>::value);
  EXPECT_TRUE(std::is_move_assignable<HolderType>::value);

  // Null cannot be accessed.
  HolderType holder(static_cast<int*>(0));
#ifndef NDEBUG  // Assert only throws when debugging.
  EXPECT_DEATH(holder.value(), "null");
  holder = HolderType(nullptr);
  EXPECT_DEATH(holder.value(), "null");
#endif

  // Value cannot be changed, but holder can be assigned.
  testutil::ExpectSameType<const int&, decltype(holder.value())>();
  int i = 1;
  holder = HolderType(&i);
  EXPECT_EQ(1, holder.value());

  // Const holder cannot be assigned, and value cannot be changed.
  EXPECT_TRUE(std::is_copy_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_copy_assignable<const HolderType>::value);
  EXPECT_TRUE(std::is_move_constructible<const HolderType>::value);
  EXPECT_FALSE(std::is_move_assignable<const HolderType>::value);
  const HolderType const_holder(&i);
  testutil::ExpectSameType<const int&, decltype(const_holder.value())>();
  EXPECT_EQ(1, const_holder.value());
  EXPECT_EQ(&holder.value(), &const_holder.value());
}

// Crazy policy that inits everything with the string "cat#", where # is the
// number of args.
struct CatHolderPolicy : Copy {
  template <typename V, typename T, typename... Args>
  static V Create(Args&&... args) {
    return absl::StrCat("cat", args_size<Args...>::value);
  }
};

TEST(Holder, Create) {
  using HolderType = Holder<std::string, CatHolderPolicy>;
  HolderType holder;
  EXPECT_EQ("cat0", *holder);
  holder = HolderType(1, "foo");
  EXPECT_EQ("cat2", *holder);
}

}  // namespace
}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#include "internal/value_internal.h"

#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

using testutil::ExpectSameType;

struct RefOnlyType : public RefCountable {
 public:
  RefOnlyType() = default;
  RefOnlyType(const RefOnlyType&) = delete;
  RefOnlyType(RefOnlyType&&) = delete;
  RefOnlyType& operator=(const RefOnlyType&) = delete;
  RefOnlyType& operator=(RefOnlyType&&) = delete;

  bool operator==(const RefOnlyType& rhs) const { return this == &rhs; }
};

class ValueAdapterTest : public ::testing::Test {
 public:
  using ValueData = BaseValue::ValueData;
  using ValueAdapter = BaseValue::ValueAdapter;
  template <typename... Types>
  using BaseTypeHelper = BaseValue::BaseTypeHelper<Types...>;
  template <typename T>
  using TypeHelper = BaseValue::TypeHelper<T>;
  using UnownedStr = BaseValue::UnownedStr;
  using OwnedStr = BaseValue::OwnedStr;

  template <typename ExpectedType, typename T, typename E>
  void TestAdapter(T&& value, E&& expected) {
    // Verify that the adapter produces the expected type.
    ExpectSameType<ExpectedType,
                   decltype(
                       MaybeAdapt(ValueAdapter(), std::forward<T>(value)))>();

    EXPECT_EQ(expected, MaybeAdapt(ValueAdapter(), std::forward<T>(value)));
  }

  template <typename ExpectedType, typename T, typename E>
  void TestValueAdapter(T&& value, E&& expected) {
    ExpectSameType<ExpectedType,
                   decltype(ValueAdapter()(std::forward<T>(value)))>();
    EXPECT_EQ(expected, ValueAdapter()(value));
    TestAdapter<ExpectedType>(std::forward<T>(value),
                              std::forward<E>(expected));
  }
};

TEST_F(ValueAdapterTest, Bool) {
  ExpectSameType<BaseTypeHelper<bool>, BaseTypeHelper<bool, bool>>();
  ExpectSameType<const bool*, decltype(BaseTypeHelper<bool>::get_if(
                                  inst_of<const ValueData*>()))>();
  ExpectSameType<const bool*, decltype(TypeHelper<bool>::get_if(
                                  inst_of<const ValueData*>()))>();
  static_assert(!is_numeric<bool>::value, "blah");
}

TEST_F(ValueAdapterTest, NullPtr) {
  // nullptr passes through by value, unchanged
  TestValueAdapter<std::nullptr_t&&>(nullptr, nullptr);
}

TEST_F(ValueAdapterTest, RefPtr) {
  // Smart pointer is dereferenced.
  auto ptr = RefPtrHolder<RefOnlyType>();
  TestAdapter<RefOnlyType&>(ptr, *ptr);
  auto cptr = RefPtrHolder<const RefOnlyType>();
  TestAdapter<const RefOnlyType&>(cptr, *cptr);
}

TEST_F(ValueAdapterTest, RefCopy) {
  auto i = RefCopyHolder<int>(1);
  TestAdapter<int&>(i, 1);

  const auto& const_ptr_int = i;
  TestAdapter<const int&>(const_ptr_int, 1);

  auto ptr_const_int = RefCopyHolder<const int>(2);
  TestAdapter<const int&>(ptr_const_int, 2);
}

TEST_F(ValueAdapterTest, String) {
  // Strings are normalized to string_view.
  absl::string_view view("hi");
  TestAdapter<absl::string_view>(view, "hi");
  std::string value = "hi";
  TestAdapter<absl::string_view>(value, "hi");

  const std::string& cvalue = value;
  TestAdapter<absl::string_view>(cvalue, "hi");

  UnownedStr unowned(cvalue);
  TestAdapter<absl::string_view>(unowned, "hi");

  using ParentOwnedStrPolicy = Ref<ParentOwned<ReffedPtr<RefOnlyType>, Copy>>;
  using ParentOwnedStr = Holder<absl::string_view, ParentOwnedStrPolicy>;
  auto parent = MakeReffed<RefOnlyType>();
  ParentOwnedStr parent_owned(parent, cvalue);
  TestValueAdapter<absl::string_view>(parent_owned, "hi");

  OwnedStr owned("hi");
  TestAdapter<absl::string_view>(owned, "hi");
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#include "internal/visitor_util.h"
#include "internal/adapter_util.h"

#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace internal {
namespace {

using testutil::ExpectSameType;

struct NoCopyType {
  NoCopyType() = default;
  NoCopyType(const NoCopyType&) = delete;
  NoCopyType(NoCopyType&&) = default;
};

struct MyStruct {};

struct TestAdapter {
  int operator()(int value) { return value + 1; }

  absl::string_view operator()(const MyStruct& value, general) {
    return "general";
  }
  absl::string_view operator()(const MyStruct& value, specialize) {
    return "specialize";
  }

  template <class T>
  MaybeAdaptResultType<TestAdapter, T&> operator()(T* ptr) {
    return MaybeAdapt(*this, *ptr);
  }
};

TEST(MaybeVisitTest, MaybeAdaptResultType) {
  // Finds the right function for all convertible types.
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, int>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, int&>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, const int&>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, int&&>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, double>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, double&>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, const double&>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, double&&>>();
  ExpectSameType<absl::string_view,
                 MaybeAdaptResultType<TestAdapter, MyStruct>>();
  ExpectSameType<absl::string_view,
                 MaybeAdaptResultType<TestAdapter, MyStruct&>>();
  ExpectSameType<absl::string_view,
                 MaybeAdaptResultType<TestAdapter, const MyStruct&>>();
  ExpectSameType<absl::string_view,
                 MaybeAdaptResultType<TestAdapter, MyStruct&&>>();

  // Even recursively.
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, int*>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, const int*>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, int*&>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, const int*&>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, double*>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, const double*>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, double*&>>();
  ExpectSameType<int, MaybeAdaptResultType<TestAdapter, const double*&>>();
  ExpectSameType<absl::string_view,
                 MaybeAdaptResultType<TestAdapter, MyStruct*>>();
  ExpectSameType<absl::string_view,
                 MaybeAdaptResultType<TestAdapter, MyStruct*&>>();

  // Reference is preserved.
  ExpectSameType<NoCopyType&, MaybeAdaptResultType<TestAdapter, NoCopyType&>>();
  // Move is preserved.
  ExpectSameType<NoCopyType&&,
                 MaybeAdaptResultType<TestAdapter, NoCopyType&&>>();
  // Even recursively.
  ExpectSameType<NoCopyType&, MaybeAdaptResultType<TestAdapter, NoCopyType*>>();
}

TEST(MaybeVisitTest, ExactMatch) {
  // Visits when exactly matches.
  EXPECT_EQ(MaybeAdapt(TestAdapter(), 1), 2);
  // Event recursively.
  int i = 9;
  EXPECT_EQ(MaybeAdapt(TestAdapter(), &i), 10);
}

TEST(MaybeVisitTest, Convertible) {
  // Visits when convertible.
  EXPECT_EQ(MaybeAdapt(TestAdapter(), 2.5), 3);
  // Even recursively.
  double d = 9.5;
  EXPECT_EQ(MaybeAdapt(TestAdapter(), &d), 10);
}

TEST(MaybeVisitTest, NoOverload) {
  // Does not visit mismatched.
  EXPECT_EQ(MaybeAdapt(TestAdapter(), std::string("hi")), "hi");

  // Works with move only.
  NoCopyType no_copy;
  EXPECT_EQ(&MaybeAdapt(TestAdapter(), no_copy), &no_copy);

  // Even recursively.
  EXPECT_EQ(&MaybeAdapt(TestAdapter(), &no_copy), &no_copy);
}

TEST(MaybeVisitTest, Specialize) {
  // Automatically specializes.
  EXPECT_EQ(MaybeAdapt(TestAdapter(), MyStruct()), "specialize");
}

struct StringVisitor {
  absl::string_view operator()(absl::string_view any_string) {
    return "string";
  }
};

struct IntVisitor {
  absl::string_view operator()(int any_int) { return "int"; }
};

struct DoubleVisitor {
  absl::string_view operator()(double any_double) { return "double"; }
};

struct NoCopyVisitor {
  absl::string_view operator()(const NoCopyType& any_int) {
    return "NoCopyType";
  }
};

TEST(OrderedVisitorTest, OrderedVisitor) {
  auto vis = MakeOrderedVisitor(StringVisitor(), IntVisitor(), DoubleVisitor(),
                                NoCopyVisitor(), DefaultVisitor<std::string>());
  EXPECT_EQ("string", vis("hi"));
  EXPECT_EQ("int", vis(1));
  // Double is converted to an int, as IntVisitor is higher priority.
  EXPECT_EQ("int", vis(2.5));
  EXPECT_EQ("NoCopyType", vis(NoCopyType()));
  EXPECT_EQ("", vis(MyStruct()));
}

}  // namespace
}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

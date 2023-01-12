// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/function_adapter.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/function.h"
#include "base/function_interface.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/memory_manager.h"
#include "base/type_factory.h"
#include "base/type_provider.h"
#include "base/value_factory.h"
#include "base/values/double_value.h"
#include "base/values/int_value.h"
#include "base/values/uint_value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::ElementsAre;
using testing::HasSubstr;
using cel::internal::StatusIs;

class FunctionAdapterTest : public ::testing::Test {
 public:
  FunctionAdapterTest()
      : type_factory_(cel::MemoryManager::Global()),
        type_manager_(type_factory_, TypeProvider::Builtin()),
        value_factory_(type_manager_),
        test_context_(value_factory_) {}

  ValueFactory& value_factory() { return value_factory_; }

  const FunctionEvaluationContext& test_context() { return test_context_; }

 private:
  TypeFactory type_factory_;
  TypeManager type_manager_;
  ValueFactory value_factory_;
  FunctionEvaluationContext test_context_;
};

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionInt) {
  using FunctionAdapter = UnaryFunctionAdapter<int64_t, int64_t>;

  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, int64_t x) -> int64_t { return x + 2; });

  std::vector<Handle<Value>> args{value_factory().CreateIntValue(40)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result.Is<IntValue>());
  EXPECT_EQ(result.As<IntValue>()->value(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionDouble) {
  using FunctionAdapter = UnaryFunctionAdapter<double, double>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, double x) -> double { return x * 2; });

  std::vector<Handle<Value>> args{value_factory().CreateDoubleValue(40.0)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result.Is<DoubleValue>());
  EXPECT_EQ(result.As<DoubleValue>()->value(), 80.0);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionUint) {
  using FunctionAdapter = UnaryFunctionAdapter<uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, uint64_t x) -> uint64_t { return x - 2; });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result.Is<UintValue>());
  EXPECT_EQ(result.As<UintValue>()->value(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionAny) {
  using FunctionAdapter = UnaryFunctionAdapter<uint64_t, Handle<Value>>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, const Handle<Value>& x) -> uint64_t {
        return x.As<UintValue>()->value() - 2;
      });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result.Is<UintValue>());
  EXPECT_EQ(result.As<UintValue>()->value(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionReturnError) {
  using FunctionAdapter = UnaryFunctionAdapter<Handle<Value>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory, uint64_t x) -> Handle<Value> {
        return value_factory.CreateErrorValue(
            absl::InvalidArgumentError("test_error"));
      });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result.Is<ErrorValue>());
  EXPECT_THAT(result.As<ErrorValue>()->value(),
              StatusIs(absl::StatusCode::kInvalidArgument, "test_error"));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionPropagateStatus) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory, uint64_t x) -> absl::StatusOr<uint64_t> {
        // Returning a status directly stops CEL evaluation and
        // immediately returns.
        return absl::InternalError("test_error");
      });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInternal, "test_error"));
}

TEST_F(FunctionAdapterTest,
       UnaryFunctionAdapterWrapFunctionReturnStatusOrValue) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory, uint64_t x) -> absl::StatusOr<uint64_t> {
        return x;
      });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(Handle<Value> result,
                       wrapped->Invoke(test_context(), args));
  EXPECT_EQ(result.As<UintValue>()->value(), 44);
}

TEST_F(FunctionAdapterTest,
       UnaryFunctionAdapterWrapFunctionWrongArgCountError) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory, uint64_t x) -> absl::StatusOr<uint64_t> {
        return 42;
      });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44),
                                  value_factory().CreateUintValue(43)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "unexpected number of arguments for unary function"));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionWrongArgTypeError) {
  using FunctionAdapter =
      UnaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory, uint64_t x) -> absl::StatusOr<uint64_t> {
        return 42;
      });

  std::vector<Handle<Value>> args{value_factory().CreateDoubleValue(44)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected uint value")));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorInt) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                           int64_t>::CreateDescriptor("Increment", false);

  EXPECT_EQ(desc.name(), "Increment");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kInt64));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorDouble) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                           double>::CreateDescriptor("Mult2", true);

  EXPECT_EQ(desc.name(), "Mult2");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_TRUE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kDouble));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorUint) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                           uint64_t>::CreateDescriptor("Increment", false);

  EXPECT_EQ(desc.name(), "Increment");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kUint64));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorAny) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                           Handle<Value>>::CreateDescriptor("Increment", false);

  EXPECT_EQ(desc.name(), "Increment");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kAny));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionInt) {
  using FunctionAdapter = BinaryFunctionAdapter<int64_t, int64_t, int64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, int64_t x, int64_t y) -> int64_t { return x + y; });

  std::vector<Handle<Value>> args{value_factory().CreateIntValue(21),
                                  value_factory().CreateIntValue(21)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result.Is<IntValue>());
  EXPECT_EQ(result.As<IntValue>()->value(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionDouble) {
  using FunctionAdapter = BinaryFunctionAdapter<double, double, double>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, double x, double y) -> double { return x * y; });

  std::vector<Handle<Value>> args{value_factory().CreateDoubleValue(40.0),
                                  value_factory().CreateDoubleValue(2.0)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result.Is<DoubleValue>());
  EXPECT_EQ(result.As<DoubleValue>()->value(), 80.0);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionUint) {
  using FunctionAdapter = BinaryFunctionAdapter<uint64_t, uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, uint64_t x, uint64_t y) -> uint64_t { return x - y; });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44),
                                  value_factory().CreateUintValue(2)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result.Is<UintValue>());
  EXPECT_EQ(result.As<UintValue>()->value(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionAny) {
  using FunctionAdapter =
      BinaryFunctionAdapter<uint64_t, Handle<Value>, Handle<Value>>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](ValueFactory&, const Handle<Value>& x,
                                       const Handle<Value>& y) -> uint64_t {
        return x.As<UintValue>()->value() -
               static_cast<int64_t>(y.As<DoubleValue>()->value());
      });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44),
                                  value_factory().CreateDoubleValue(2)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result.Is<UintValue>());
  EXPECT_EQ(result.As<UintValue>()->value(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionReturnError) {
  using FunctionAdapter =
      BinaryFunctionAdapter<Handle<Value>, int64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory, int64_t x, uint64_t y) -> Handle<Value> {
        return value_factory.CreateErrorValue(
            absl::InvalidArgumentError("test_error"));
      });

  std::vector<Handle<Value>> args{value_factory().CreateIntValue(44),
                                  value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result.Is<ErrorValue>());
  EXPECT_THAT(result.As<ErrorValue>()->value(),
              StatusIs(absl::StatusCode::kInvalidArgument, "test_error"));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionPropagateStatus) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<uint64_t>, int64_t, uint64_t>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](ValueFactory& value_factory, int64_t,
                                       uint64_t x) -> absl::StatusOr<uint64_t> {
        // Returning a status directly stops CEL evaluation and
        // immediately returns.
        return absl::InternalError("test_error");
      });

  std::vector<Handle<Value>> args{value_factory().CreateIntValue(43),
                                  value_factory().CreateUintValue(44)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInternal, "test_error"));
}

TEST_F(FunctionAdapterTest,
       BinaryFunctionAdapterWrapFunctionWrongArgCountError) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t, double>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory, uint64_t x,
         double y) -> absl::StatusOr<uint64_t> { return 42; });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "unexpected number of arguments for binary function"));
}

TEST_F(FunctionAdapterTest,
       BinaryFunctionAdapterWrapFunctionWrongArgTypeError) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<uint64_t>, uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory, int64_t x,
         int64_t y) -> absl::StatusOr<uint64_t> { return 42; });

  std::vector<Handle<Value>> args{value_factory().CreateDoubleValue(44),
                                  value_factory().CreateDoubleValue(44)};
  EXPECT_THAT(wrapped->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected uint value")));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorInt) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, int64_t,
                            int64_t>::CreateDescriptor("Add", false);

  EXPECT_EQ(desc.name(), "Add");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kInt64, Kind::kInt64));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorDouble) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, double,
                            double>::CreateDescriptor("Mult", true);

  EXPECT_EQ(desc.name(), "Mult");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_TRUE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kDouble, Kind::kDouble));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorUint) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, uint64_t,
                            uint64_t>::CreateDescriptor("Add", false);

  EXPECT_EQ(desc.name(), "Add");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kUint64, Kind::kUint64));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorAny) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, Handle<Value>,
                            Handle<Value>>::CreateDescriptor("Add", false);
  EXPECT_EQ(desc.name(), "Add");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kAny, Kind::kAny));
}

}  // namespace
}  // namespace cel

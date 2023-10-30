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
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "base/function.h"
#include "base/function_descriptor.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/bytes_value.h"
#include "base/values/double_value.h"
#include "base/values/duration_value.h"
#include "base/values/int_value.h"
#include "base/values/timestamp_value.h"
#include "base/values/uint_value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::ElementsAre;
using testing::HasSubstr;
using testing::IsEmpty;
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

  ASSERT_TRUE(result->Is<IntValue>());
  EXPECT_EQ(result.As<IntValue>()->NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionDouble) {
  using FunctionAdapter = UnaryFunctionAdapter<double, double>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, double x) -> double { return x * 2; });

  std::vector<Handle<Value>> args{value_factory().CreateDoubleValue(40.0)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<DoubleValue>());
  EXPECT_EQ(result.As<DoubleValue>()->NativeValue(), 80.0);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionUint) {
  using FunctionAdapter = UnaryFunctionAdapter<uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, uint64_t x) -> uint64_t { return x - 2; });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.As<UintValue>()->NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionBool) {
  using FunctionAdapter = UnaryFunctionAdapter<bool, bool>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, bool x) -> bool { return !x; });

  std::vector<Handle<Value>> args{value_factory().CreateBoolValue(true)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<BoolValue>());
  EXPECT_EQ(result.As<BoolValue>()->NativeValue(), false);
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionTimestamp) {
  using FunctionAdapter = UnaryFunctionAdapter<absl::Time, absl::Time>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, absl::Time x) -> absl::Time {
        return x + absl::Minutes(1);
      });

  std::vector<Handle<Value>> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateTimestampValue(absl::UnixEpoch()));
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<TimestampValue>());
  EXPECT_EQ(result.As<TimestampValue>()->NativeValue(),
            absl::UnixEpoch() + absl::Minutes(1));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionDuration) {
  using FunctionAdapter = UnaryFunctionAdapter<absl::Duration, absl::Duration>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, absl::Duration x) -> absl::Duration {
        return x + absl::Seconds(2);
      });

  std::vector<Handle<Value>> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateDurationValue(absl::Seconds(6)));
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<DurationValue>());
  EXPECT_EQ(result.As<DurationValue>()->NativeValue(), absl::Seconds(8));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionString) {
  using FunctionAdapter =
      UnaryFunctionAdapter<Handle<StringValue>, Handle<StringValue>>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory,
         const Handle<StringValue>& x) -> Handle<StringValue> {
        return value_factory.CreateStringValue("pre_" + x->ToString()).value();
      });

  std::vector<Handle<Value>> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateStringValue("string"));
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.As<StringValue>()->ToString(), "pre_string");
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionBytes) {
  using FunctionAdapter =
      UnaryFunctionAdapter<Handle<BytesValue>, Handle<BytesValue>>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory,
         const Handle<BytesValue>& x) -> Handle<BytesValue> {
        return value_factory.CreateBytesValue("pre_" + x->ToString()).value();
      });

  std::vector<Handle<Value>> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateBytesValue("bytes"));
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<BytesValue>());
  EXPECT_EQ(result.As<BytesValue>()->ToString(), "pre_bytes");
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterWrapFunctionAny) {
  using FunctionAdapter = UnaryFunctionAdapter<uint64_t, Handle<Value>>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, const Handle<Value>& x) -> uint64_t {
        return x.As<UintValue>()->NativeValue() - 2;
      });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.As<UintValue>()->NativeValue(), 42);
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

  ASSERT_TRUE(result->Is<ErrorValue>());
  EXPECT_THAT(result.As<ErrorValue>()->NativeValue(),
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
  EXPECT_EQ(result.As<UintValue>()->NativeValue(), 44);
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

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorBool) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                           bool>::CreateDescriptor("Not", false);

  EXPECT_EQ(desc.name(), "Not");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kBool));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorTimestamp) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                           absl::Time>::CreateDescriptor("AddMinute", false);

  EXPECT_EQ(desc.name(), "AddMinute");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kTimestamp));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorDuration) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                           absl::Duration>::CreateDescriptor("AddFiveSeconds",
                                                             false);

  EXPECT_EQ(desc.name(), "AddFiveSeconds");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kDuration));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorString) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                           Handle<StringValue>>::CreateDescriptor("Prepend",
                                                                  false);

  EXPECT_EQ(desc.name(), "Prepend");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kString));
}

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorBytes) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                           Handle<BytesValue>>::CreateDescriptor("Prepend",
                                                                 false);

  EXPECT_EQ(desc.name(), "Prepend");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kBytes));
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

TEST_F(FunctionAdapterTest, UnaryFunctionAdapterCreateDescriptorNonStrict) {
  FunctionDescriptor desc =
      UnaryFunctionAdapter<absl::StatusOr<Handle<Value>>, Handle<Value>>::
          CreateDescriptor("Increment", false,
                           /*is_strict=*/false);

  EXPECT_EQ(desc.name(), "Increment");
  EXPECT_FALSE(desc.is_strict());
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

  ASSERT_TRUE(result->Is<IntValue>());
  EXPECT_EQ(result.As<IntValue>()->NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionDouble) {
  using FunctionAdapter = BinaryFunctionAdapter<double, double, double>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, double x, double y) -> double { return x * y; });

  std::vector<Handle<Value>> args{value_factory().CreateDoubleValue(40.0),
                                  value_factory().CreateDoubleValue(2.0)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<DoubleValue>());
  EXPECT_EQ(result.As<DoubleValue>()->NativeValue(), 80.0);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionUint) {
  using FunctionAdapter = BinaryFunctionAdapter<uint64_t, uint64_t, uint64_t>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, uint64_t x, uint64_t y) -> uint64_t { return x - y; });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44),
                                  value_factory().CreateUintValue(2)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.As<UintValue>()->NativeValue(), 42);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionBool) {
  using FunctionAdapter = BinaryFunctionAdapter<bool, bool, bool>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, bool x, bool y) -> bool { return x != y; });

  std::vector<Handle<Value>> args{value_factory().CreateBoolValue(false),
                                  value_factory().CreateBoolValue(true)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<BoolValue>());
  EXPECT_EQ(result.As<BoolValue>()->NativeValue(), true);
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionTimestamp) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::Time, absl::Time, absl::Time>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, absl::Time x, absl::Time y) -> absl::Time {
        return x > y ? x : y;
      });

  std::vector<Handle<Value>> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateTimestampValue(absl::UnixEpoch() +
                                                            absl::Seconds(1)));
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateTimestampValue(absl::UnixEpoch() +
                                                            absl::Seconds(2)));

  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<TimestampValue>());
  EXPECT_EQ(result.As<TimestampValue>()->NativeValue(),
            absl::UnixEpoch() + absl::Seconds(2));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionDuration) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::Duration, absl::Duration, absl::Duration>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory&, absl::Duration x, absl::Duration y) -> absl::Duration {
        return x > y ? x : y;
      });

  std::vector<Handle<Value>> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateDurationValue(absl::Seconds(5)));
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateDurationValue(absl::Seconds(2)));

  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<DurationValue>());
  EXPECT_EQ(result.As<DurationValue>()->NativeValue(), absl::Seconds(5));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionString) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<Handle<StringValue>>,
                            const Handle<StringValue>&,
                            const Handle<StringValue>&>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory, const Handle<StringValue>& x,
         const Handle<StringValue>& y) -> absl::StatusOr<Handle<StringValue>> {
        return value_factory.CreateStringValue(x->ToString() + y->ToString());
      });

  std::vector<Handle<Value>> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateStringValue("abc"));
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateStringValue("def"));

  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.As<StringValue>()->ToString(), "abcdef");
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionBytes) {
  using FunctionAdapter =
      BinaryFunctionAdapter<absl::StatusOr<Handle<BytesValue>>,
                            const Handle<BytesValue>&,
                            const Handle<BytesValue>&>;
  std::unique_ptr<Function> wrapped = FunctionAdapter::WrapFunction(
      [](ValueFactory& value_factory, const Handle<BytesValue>& x,
         const Handle<BytesValue>& y) -> absl::StatusOr<Handle<BytesValue>> {
        return value_factory.CreateBytesValue(x->ToString() + y->ToString());
      });

  std::vector<Handle<Value>> args;
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateBytesValue("abc"));
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateBytesValue("def"));

  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<BytesValue>());
  EXPECT_EQ(result.As<BytesValue>()->ToString(), "abcdef");
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterWrapFunctionAny) {
  using FunctionAdapter =
      BinaryFunctionAdapter<uint64_t, Handle<Value>, Handle<Value>>;
  std::unique_ptr<Function> wrapped =
      FunctionAdapter::WrapFunction([](ValueFactory&, const Handle<Value>& x,
                                       const Handle<Value>& y) -> uint64_t {
        return x.As<UintValue>()->NativeValue() -
               static_cast<int64_t>(y.As<DoubleValue>()->NativeValue());
      });

  std::vector<Handle<Value>> args{value_factory().CreateUintValue(44),
                                  value_factory().CreateDoubleValue(2)};
  ASSERT_OK_AND_ASSIGN(auto result, wrapped->Invoke(test_context(), args));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.As<UintValue>()->NativeValue(), 42);
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

  ASSERT_TRUE(result->Is<ErrorValue>());
  EXPECT_THAT(result.As<ErrorValue>()->NativeValue(),
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

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorBool) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, bool,
                            bool>::CreateDescriptor("Xor", false);

  EXPECT_EQ(desc.name(), "Xor");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kBool, Kind::kBool));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorTimestamp) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Time,
                            absl::Time>::CreateDescriptor("Max", false);

  EXPECT_EQ(desc.name(), "Max");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kTimestamp, Kind::kTimestamp));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorDuration) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Duration,
                            absl::Duration>::CreateDescriptor("Max", false);

  EXPECT_EQ(desc.name(), "Max");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kDuration, Kind::kDuration));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorString) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, Handle<StringValue>,
                            Handle<StringValue>>::CreateDescriptor("Concat",
                                                                   false);

  EXPECT_EQ(desc.name(), "Concat");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kString, Kind::kString));
}

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorBytes) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, Handle<BytesValue>,
                            Handle<BytesValue>>::CreateDescriptor("Concat",
                                                                  false);

  EXPECT_EQ(desc.name(), "Concat");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kBytes, Kind::kBytes));
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

TEST_F(FunctionAdapterTest, BinaryFunctionAdapterCreateDescriptorNonStrict) {
  FunctionDescriptor desc =
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, Handle<Value>,
                            Handle<Value>>::CreateDescriptor("Add", false,
                                                             false);
  EXPECT_EQ(desc.name(), "Add");
  EXPECT_FALSE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), ElementsAre(Kind::kAny, Kind::kAny));
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterCreateDescriptor0Args) {
  FunctionDescriptor desc =
      VariadicFunctionAdapter<absl::StatusOr<Handle<Value>>>::CreateDescriptor(
          "ZeroArgs", false);

  EXPECT_EQ(desc.name(), "ZeroArgs");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(), IsEmpty());
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterWrapFunction0Args) {
  std::unique_ptr<Function> fn =
      VariadicFunctionAdapter<absl::StatusOr<Handle<Value>>>::WrapFunction(
          [](ValueFactory& value_factory) {
            return value_factory.CreateStringValue("abc");
          });

  ASSERT_OK_AND_ASSIGN(auto result, fn->Invoke(test_context(), {}));
  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.As<StringValue>()->ToString(), "abc");
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterCreateDescriptor3Args) {
  FunctionDescriptor desc = VariadicFunctionAdapter<
      absl::StatusOr<Handle<Value>>, int64_t, bool,
      const StringValue&>::CreateDescriptor("MyFormatter", false);

  EXPECT_EQ(desc.name(), "MyFormatter");
  EXPECT_TRUE(desc.is_strict());
  EXPECT_FALSE(desc.receiver_style());
  EXPECT_THAT(desc.types(),
              ElementsAre(Kind::kInt64, Kind::kBool, Kind::kString));
}

TEST_F(FunctionAdapterTest, VariadicFunctionAdapterWrapFunction3Args) {
  std::unique_ptr<Function> fn = VariadicFunctionAdapter<
      absl::StatusOr<Handle<Value>>, int64_t, bool,
      const StringValue&>::WrapFunction([](ValueFactory& value_factory,
                                           int64_t int_val, bool bool_val,
                                           const StringValue& string_val)
                                            -> absl::StatusOr<Handle<Value>> {
    return value_factory.CreateStringValue(
        absl::StrCat(int_val, "_", (bool_val ? "true" : "false"), "_",
                     string_val.ToString()));
  });

  std::vector<Handle<Value>> args{value_factory().CreateIntValue(42),
                                  value_factory().CreateBoolValue(false)};
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateStringValue("abcd"));
  ASSERT_OK_AND_ASSIGN(auto result, fn->Invoke(test_context(), args));
  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.As<StringValue>()->ToString(), "42_false_abcd");
}

TEST_F(FunctionAdapterTest,
       VariadicFunctionAdapterWrapFunction3ArgsBadArgType) {
  std::unique_ptr<Function> fn = VariadicFunctionAdapter<
      absl::StatusOr<Handle<Value>>, int64_t, bool,
      const StringValue&>::WrapFunction([](ValueFactory& value_factory,
                                           int64_t int_val, bool bool_val,
                                           const StringValue& string_val)
                                            -> absl::StatusOr<Handle<Value>> {
    return value_factory.CreateStringValue(
        absl::StrCat(int_val, "_", (bool_val ? "true" : "false"), "_",
                     string_val.ToString()));
  });

  std::vector<Handle<Value>> args{value_factory().CreateIntValue(42),
                                  value_factory().CreateBoolValue(false)};
  ASSERT_OK_AND_ASSIGN(args.emplace_back(),
                       value_factory().CreateTimestampValue(absl::UnixEpoch()));
  EXPECT_THAT(fn->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("expected string value")));
}

TEST_F(FunctionAdapterTest,
       VariadicFunctionAdapterWrapFunction3ArgsBadArgCount) {
  std::unique_ptr<Function> fn = VariadicFunctionAdapter<
      absl::StatusOr<Handle<Value>>, int64_t, bool,
      const StringValue&>::WrapFunction([](ValueFactory& value_factory,
                                           int64_t int_val, bool bool_val,
                                           const StringValue& string_val)
                                            -> absl::StatusOr<Handle<Value>> {
    return value_factory.CreateStringValue(
        absl::StrCat(int_val, "_", (bool_val ? "true" : "false"), "_",
                     string_val.ToString()));
  });

  std::vector<Handle<Value>> args{value_factory().CreateIntValue(42),
                                  value_factory().CreateBoolValue(false)};
  EXPECT_THAT(fn->Invoke(test_context(), args),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("unexpected number of arguments")));
}

}  // namespace
}  // namespace cel

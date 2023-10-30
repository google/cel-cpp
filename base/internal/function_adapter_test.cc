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

#include "base/internal/function_adapter.h"

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
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
#include "base/values/error_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "base/values/null_value.h"
#include "base/values/string_value.h"
#include "base/values/struct_value.h"
#include "base/values/timestamp_value.h"
#include "base/values/uint_value.h"
#include "internal/testing.h"

namespace cel::internal {
namespace {

using cel::internal::StatusIs;

static_assert(AdaptedKind<int64_t>() == Kind::kInt, "int adapts to int64_t");
static_assert(AdaptedKind<uint64_t>() == Kind::kUint,
              "uint adapts to uint64_t");
static_assert(AdaptedKind<double>() == Kind::kDouble,
              "double adapts to double");
static_assert(AdaptedKind<bool>() == Kind::kBool, "bool adapts to bool");
static_assert(AdaptedKind<absl::Time>() == Kind::kTimestamp,
              "timestamp adapts to absl::Time");
static_assert(AdaptedKind<absl::Duration>() == Kind::kDuration,
              "duration adapts to absl::Duration");
// Handle types.
static_assert(AdaptedKind<Handle<Value>>() == Kind::kAny,
              "any adapts to Handle<Value>");
static_assert(AdaptedKind<Handle<StringValue>>() == Kind::kString,
              "string adapts to Handle<String>");
static_assert(AdaptedKind<Handle<BytesValue>>() == Kind::kBytes,
              "bytes adapts to Handle<Bytes>");
static_assert(AdaptedKind<Handle<StructValue>>() == Kind::kStruct,
              "struct adapts to Handle<StructValue>");
static_assert(AdaptedKind<Handle<ListValue>>() == Kind::kList,
              "list adapts to Handle<ListValue>");
static_assert(AdaptedKind<Handle<MapValue>>() == Kind::kMap,
              "map adapts to Handle<MapValue>");
static_assert(AdaptedKind<Handle<NullValue>>() == Kind::kNullType,
              "null adapts to Handle<NullValue>");
static_assert(AdaptedKind<const Value&>() == Kind::kAny,
              "any adapts to const Value&");
static_assert(AdaptedKind<const StringValue&>() == Kind::kString,
              "string adapts to const String&");
static_assert(AdaptedKind<const BytesValue&>() == Kind::kBytes,
              "bytes adapts to const Bytes&");
static_assert(AdaptedKind<const StructValue&>() == Kind::kStruct,
              "struct adapts to const StructValue&");
static_assert(AdaptedKind<const ListValue&>() == Kind::kList,
              "list adapts to const ListValue&");
static_assert(AdaptedKind<const MapValue&>() == Kind::kMap,
              "map adapts to const MapValue&");
static_assert(AdaptedKind<const NullValue&>() == Kind::kNullType,
              "null adapts to const NullValue&");

class ValueFactoryTestBase : public testing::Test {
 public:
  ValueFactoryTestBase()
      : type_factory_(MemoryManager::Global()),
        type_manager_(type_factory_, TypeProvider::Builtin()),
        value_factory_(type_manager_) {}

  ValueFactory& value_factory() { return value_factory_; }

 private:
  TypeFactory type_factory_;
  TypeManager type_manager_;
  ValueFactory value_factory_;
};

class HandleToAdaptedVisitorTest : public ValueFactoryTestBase {};

TEST_F(HandleToAdaptedVisitorTest, Int) {
  Handle<Value> v = value_factory().CreateIntValue(10);

  int64_t out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ(out, 10);
}

TEST_F(HandleToAdaptedVisitorTest, IntWrongKind) {
  Handle<Value> v = value_factory().CreateUintValue(10);

  int64_t out;
  EXPECT_THAT(
      HandleToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected int value"));
}

TEST_F(HandleToAdaptedVisitorTest, Uint) {
  Handle<Value> v = value_factory().CreateUintValue(11);

  uint64_t out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ(out, 11);
}

TEST_F(HandleToAdaptedVisitorTest, UintWrongKind) {
  Handle<Value> v = value_factory().CreateIntValue(11);

  uint64_t out;
  EXPECT_THAT(
      HandleToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected uint value"));
}

TEST_F(HandleToAdaptedVisitorTest, Double) {
  Handle<Value> v = value_factory().CreateDoubleValue(12.0);

  double out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ(out, 12.0);
}

TEST_F(HandleToAdaptedVisitorTest, DoubleWrongKind) {
  Handle<Value> v = value_factory().CreateUintValue(10);

  double out;
  EXPECT_THAT(
      HandleToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected double value"));
}

TEST_F(HandleToAdaptedVisitorTest, Bool) {
  Handle<Value> v = value_factory().CreateBoolValue(false);

  bool out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ(out, false);
}

TEST_F(HandleToAdaptedVisitorTest, BoolWrongKind) {
  Handle<Value> v = value_factory().CreateUintValue(10);

  bool out;
  EXPECT_THAT(
      HandleToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected bool value"));
}

TEST_F(HandleToAdaptedVisitorTest, Timestamp) {
  ASSERT_OK_AND_ASSIGN(Handle<Value> v,
                       value_factory().CreateTimestampValue(absl::UnixEpoch() +
                                                            absl::Seconds(1)));

  absl::Time out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ(out, absl::UnixEpoch() + absl::Seconds(1));
}

TEST_F(HandleToAdaptedVisitorTest, TimestampWrongKind) {
  Handle<Value> v = value_factory().CreateUintValue(10);

  absl::Time out;
  EXPECT_THAT(
      HandleToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected timestamp value"));
}

TEST_F(HandleToAdaptedVisitorTest, Duration) {
  ASSERT_OK_AND_ASSIGN(Handle<Value> v,
                       value_factory().CreateDurationValue(absl::Seconds(5)));

  absl::Duration out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ(out, absl::Seconds(5));
}

TEST_F(HandleToAdaptedVisitorTest, DurationWrongKind) {
  Handle<Value> v = value_factory().CreateUintValue(10);

  absl::Duration out;
  EXPECT_THAT(
      HandleToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected duration value"));
}

TEST_F(HandleToAdaptedVisitorTest, String) {
  ASSERT_OK_AND_ASSIGN(Handle<Value> v,
                       value_factory().CreateStringValue("string"));

  Handle<StringValue> out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ(out->ToString(), "string");
}

TEST_F(HandleToAdaptedVisitorTest, StringHandlePtr) {
  ASSERT_OK_AND_ASSIGN(Handle<Value> v,
                       value_factory().CreateStringValue("string"));

  const Handle<StringValue>* out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ((*out)->ToString(), "string");
}

TEST_F(HandleToAdaptedVisitorTest, StringPtr) {
  ASSERT_OK_AND_ASSIGN(Handle<Value> v,
                       value_factory().CreateStringValue("string"));

  const StringValue* out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ(out->ToString(), "string");
}

TEST_F(HandleToAdaptedVisitorTest, StringWrongKind) {
  Handle<Value> v = value_factory().CreateUintValue(10);

  Handle<StringValue> out;
  EXPECT_THAT(
      HandleToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected string value"));
}

TEST_F(HandleToAdaptedVisitorTest, Bytes) {
  ASSERT_OK_AND_ASSIGN(Handle<Value> v,
                       value_factory().CreateBytesValue("bytes"));

  Handle<BytesValue> out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ(out->ToString(), "bytes");
}

TEST_F(HandleToAdaptedVisitorTest, BytesHandlePtr) {
  ASSERT_OK_AND_ASSIGN(Handle<Value> v,
                       value_factory().CreateBytesValue("bytes"));

  const Handle<BytesValue>* out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ((*out)->ToString(), "bytes");
}

TEST_F(HandleToAdaptedVisitorTest, BytesPtr) {
  ASSERT_OK_AND_ASSIGN(Handle<Value> v,
                       value_factory().CreateBytesValue("bytes"));

  const BytesValue* out;
  ASSERT_OK(HandleToAdaptedVisitor{v}(&out));

  EXPECT_EQ(out->ToString(), "bytes");
}

TEST_F(HandleToAdaptedVisitorTest, BytesWrongKind) {
  Handle<Value> v = value_factory().CreateUintValue(10);

  Handle<BytesValue> out;
  EXPECT_THAT(
      HandleToAdaptedVisitor{v}(&out),
      StatusIs(absl::StatusCode::kInvalidArgument, "expected bytes value"));
}

class AdaptedToHandleVisitorTest : public ValueFactoryTestBase {};

TEST_F(AdaptedToHandleVisitorTest, Int) {
  int64_t value = 10;

  ASSERT_OK_AND_ASSIGN(auto result,
                       AdaptedToHandleVisitor{value_factory()}(value));

  ASSERT_TRUE(result->Is<IntValue>());
  EXPECT_EQ(result.As<IntValue>()->NativeValue(), 10);
}

TEST_F(AdaptedToHandleVisitorTest, Double) {
  double value = 10;

  ASSERT_OK_AND_ASSIGN(auto result,
                       AdaptedToHandleVisitor{value_factory()}(value));

  ASSERT_TRUE(result->Is<DoubleValue>());
  EXPECT_EQ(result.As<DoubleValue>()->NativeValue(), 10.0);
}

TEST_F(AdaptedToHandleVisitorTest, Uint) {
  uint64_t value = 10;

  ASSERT_OK_AND_ASSIGN(auto result,
                       AdaptedToHandleVisitor{value_factory()}(value));

  ASSERT_TRUE(result->Is<UintValue>());
  EXPECT_EQ(result.As<UintValue>()->NativeValue(), 10);
}

TEST_F(AdaptedToHandleVisitorTest, Bool) {
  bool value = true;

  ASSERT_OK_AND_ASSIGN(auto result,
                       AdaptedToHandleVisitor{value_factory()}(value));

  ASSERT_TRUE(result->Is<BoolValue>());
  EXPECT_EQ(result.As<BoolValue>()->NativeValue(), true);
}

TEST_F(AdaptedToHandleVisitorTest, Timestamp) {
  absl::Time value = absl::UnixEpoch() + absl::Seconds(10);

  ASSERT_OK_AND_ASSIGN(auto result,
                       AdaptedToHandleVisitor{value_factory()}(value));

  ASSERT_TRUE(result->Is<TimestampValue>());
  EXPECT_EQ(result.As<TimestampValue>()->NativeValue(),
            absl::UnixEpoch() + absl::Seconds(10));
}

TEST_F(AdaptedToHandleVisitorTest, Duration) {
  absl::Duration value = absl::Seconds(5);

  ASSERT_OK_AND_ASSIGN(auto result,
                       AdaptedToHandleVisitor{value_factory()}(value));

  ASSERT_TRUE(result->Is<DurationValue>());
  EXPECT_EQ(result.As<DurationValue>()->NativeValue(), absl::Seconds(5));
}

TEST_F(AdaptedToHandleVisitorTest, String) {
  ASSERT_OK_AND_ASSIGN(Handle<StringValue> value,
                       value_factory().CreateStringValue("str"));

  ASSERT_OK_AND_ASSIGN(auto result,
                       AdaptedToHandleVisitor{value_factory()}(value));

  ASSERT_TRUE(result->Is<StringValue>());
  EXPECT_EQ(result.As<StringValue>()->ToString(), "str");
}

TEST_F(AdaptedToHandleVisitorTest, Bytes) {
  ASSERT_OK_AND_ASSIGN(Handle<BytesValue> value,
                       value_factory().CreateBytesValue("bytes"));

  ASSERT_OK_AND_ASSIGN(auto result,
                       AdaptedToHandleVisitor{value_factory()}(value));

  ASSERT_TRUE(result->Is<BytesValue>());
  EXPECT_EQ(result.As<BytesValue>()->ToString(), "bytes");
}

TEST_F(AdaptedToHandleVisitorTest, StatusOrValue) {
  absl::StatusOr<int64_t> value = 10;

  ASSERT_OK_AND_ASSIGN(auto result,
                       AdaptedToHandleVisitor{value_factory()}(value));

  ASSERT_TRUE(result->Is<IntValue>());
  EXPECT_EQ(result.As<IntValue>()->NativeValue(), 10);
}

TEST_F(AdaptedToHandleVisitorTest, StatusOrError) {
  absl::StatusOr<int64_t> value = absl::InternalError("test_error");

  EXPECT_THAT(AdaptedToHandleVisitor{value_factory()}(value).status(),
              StatusIs(absl::StatusCode::kInternal, "test_error"));
}

TEST_F(AdaptedToHandleVisitorTest, Any) {
  auto handle =
      value_factory().CreateErrorValue(absl::InternalError("test_error"));

  ASSERT_OK_AND_ASSIGN(auto result,
                       AdaptedToHandleVisitor{value_factory()}(handle));

  ASSERT_TRUE(result->Is<ErrorValue>());
  EXPECT_THAT(result.As<ErrorValue>()->value(),
              StatusIs(absl::StatusCode::kInternal, "test_error"));
}

}  // namespace
}  // namespace cel::internal

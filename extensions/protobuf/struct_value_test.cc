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

#include "extensions/protobuf/struct_value.h"

#include <set>
#include <utility>
#include <vector>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/functional/function_ref.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "base/internal/memory_manager_testing.h"
#include "base/testing/value_matchers.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/types/struct_type.h"
#include "base/value_factory.h"
#include "extensions/protobuf/internal/testing.h"
#include "extensions/protobuf/type_provider.h"
#include "extensions/protobuf/value.h"
#include "internal/testing.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"

namespace cel::extensions {
namespace {

using FieldId = ::cel::extensions::ProtoStructType::FieldId;
using ::cel_testing::ValueOf;
using testing::Eq;
using testing::EqualsProto;
using testing::Optional;
using testing::status::CanonicalStatusIs;
using cel::internal::IsOkAndHolds;

using TestAllTypes = ::google::api::expr::test::v1::proto3::TestAllTypes;
using NullValueProto = ::google::protobuf::NullValue;

constexpr NullValueProto NULL_VALUE = NullValueProto::NULL_VALUE;

using ProtoStructValueTest = ProtoTest<>;

TestAllTypes CreateTestMessage() {
  TestAllTypes message;
  return message;
}

template <typename Func>
TestAllTypes CreateTestMessage(Func&& func) {
  TestAllTypes message;
  std::forward<Func>(func)(message);
  return message;
}

TestAllTypes::NestedMessage CreateTestNestedMessage(int bb) {
  TestAllTypes::NestedMessage nested_message;
  nested_message.set_bb(bb);
  return nested_message;
}

template <typename T>
Handle<T> Must(Handle<T> handle) {
  return handle;
}

template <typename T>
Handle<T> Must(absl::optional<T> optional) {
  return std::move(optional).value();
}

template <typename T>
T Must(absl::StatusOr<T> status_or) {
  return Must(std::move(status_or).value());
}

int TestMessageFieldNameToNumber(absl::string_view name) {
  const auto* descriptor = TestAllTypes::descriptor();
  return ABSL_DIE_IF_NULL(descriptor->FindFieldByName(name))->number();
}

void TestHasFieldImpl(
    MemoryManager& memory_manager,
    absl::FunctionRef<absl::StatusOr<bool>(const Handle<StructValue>&,
                                           const StructValue::HasFieldContext&)>
        has_field,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker, bool found) {
  TypeFactory type_factory(memory_manager);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value_without,
                       ProtoValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(
      has_field(value_without, StructValue::HasFieldContext(type_manager)),
      IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoValue::Create(value_factory, CreateTestMessage(test_message_maker)));
  EXPECT_THAT(has_field(value_with, StructValue::HasFieldContext(type_manager)),
              IsOkAndHolds(Eq(found)));
}

void TestHasFieldByName(
    MemoryManager& memory_manager, absl::string_view name,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker, bool found) {
  TestHasFieldImpl(
      memory_manager,
      [&](const Handle<StructValue>& value,
          const StructValue::HasFieldContext& context) {
        return value->HasFieldByName(context, name);
      },
      test_message_maker, found);
}

void TestHasFieldByNumber(
    MemoryManager& memory_manager, int64_t number,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker, bool found) {
  TestHasFieldImpl(
      memory_manager,
      [&](const Handle<StructValue>& value,
          const StructValue::HasFieldContext& context) {
        return value->HasFieldByNumber(context, number);
      },
      test_message_maker, found);
}

void TestHasField(MemoryManager& memory_manager, absl::string_view name,
                  absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
                  bool found = true) {
  TestHasFieldByName(memory_manager, name, test_message_maker, found);
  TestHasFieldByNumber(memory_manager, TestMessageFieldNameToNumber(name),
                       test_message_maker, found);
}

#define TEST_HAS_FIELD(...) ASSERT_NO_FATAL_FAILURE(TestHasField(__VA_ARGS__))

TEST_P(ProtoStructValueTest, NullValueHasField) {
  // In proto3, this can never be present as it will always be the default
  // value. We would need to add `optional` for it to work.
  TEST_HAS_FIELD(
      memory_manager(), "null_value",
      [](TestAllTypes& message) { message.set_null_value(NULL_VALUE); }, false);
}

TEST_P(ProtoStructValueTest, OptionalNullValueHasField) {
  TEST_HAS_FIELD(memory_manager(), "optional_null_value",
                 [](TestAllTypes& message) {
                   message.set_optional_null_value(NULL_VALUE);
                 });
}

TEST_P(ProtoStructValueTest, BoolHasField) {
  TEST_HAS_FIELD(memory_manager(), "single_bool",
                 [](TestAllTypes& message) { message.set_single_bool(true); });
}

TEST_P(ProtoStructValueTest, Int32HasField) {
  TEST_HAS_FIELD(memory_manager(), "single_int32",
                 [](TestAllTypes& message) { message.set_single_int32(1); });
}

TEST_P(ProtoStructValueTest, Int64HasField) {
  TEST_HAS_FIELD(memory_manager(), "single_int64",
                 [](TestAllTypes& message) { message.set_single_int64(1); });
}

TEST_P(ProtoStructValueTest, Uint32HasField) {
  TEST_HAS_FIELD(memory_manager(), "single_uint32",
                 [](TestAllTypes& message) { message.set_single_uint32(1); });
}

TEST_P(ProtoStructValueTest, Uint64HasField) {
  TEST_HAS_FIELD(memory_manager(), "single_uint64",
                 [](TestAllTypes& message) { message.set_single_uint64(1); });
}

TEST_P(ProtoStructValueTest, FloatHasField) {
  TEST_HAS_FIELD(memory_manager(), "single_float",
                 [](TestAllTypes& message) { message.set_single_float(1.0); });
}

TEST_P(ProtoStructValueTest, DoubleHasField) {
  TEST_HAS_FIELD(memory_manager(), "single_double",
                 [](TestAllTypes& message) { message.set_single_double(1.0); });
}

TEST_P(ProtoStructValueTest, BytesHasField) {
  TEST_HAS_FIELD(memory_manager(), "single_bytes", [](TestAllTypes& message) {
    message.set_single_bytes("foo");
  });
}

TEST_P(ProtoStructValueTest, StringHasField) {
  TEST_HAS_FIELD(memory_manager(), "single_string", [](TestAllTypes& message) {
    message.set_single_string("foo");
  });
}

TEST_P(ProtoStructValueTest, DurationHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_duration",
      [](TestAllTypes& message) { message.mutable_single_duration(); });
}

TEST_P(ProtoStructValueTest, TimestampHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_timestamp",
      [](TestAllTypes& message) { message.mutable_single_timestamp(); });
}

TEST_P(ProtoStructValueTest, EnumHasField) {
  TEST_HAS_FIELD(memory_manager(), "standalone_enum",
                 [](TestAllTypes& message) {
                   message.set_standalone_enum(TestAllTypes::BAR);
                 });
}

TEST_P(ProtoStructValueTest, MessageHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "standalone_message",
      [](TestAllTypes& message) { message.mutable_standalone_message(); });
}

TEST_P(ProtoStructValueTest, BoolWrapperHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_bool_wrapper",
      [](TestAllTypes& message) { message.mutable_single_bool_wrapper(); });
}

TEST_P(ProtoStructValueTest, Int32WrapperHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_int32_wrapper",
      [](TestAllTypes& message) { message.mutable_single_int32_wrapper(); });
}

TEST_P(ProtoStructValueTest, Int64WrapperHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_int64_wrapper",
      [](TestAllTypes& message) { message.mutable_single_int64_wrapper(); });
}

TEST_P(ProtoStructValueTest, UInt32WrapperHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_uint32_wrapper",
      [](TestAllTypes& message) { message.mutable_single_uint32_wrapper(); });
}

TEST_P(ProtoStructValueTest, UInt64WrapperHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_uint64_wrapper",
      [](TestAllTypes& message) { message.mutable_single_uint64_wrapper(); });
}

TEST_P(ProtoStructValueTest, FloatWrapperHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_float_wrapper",
      [](TestAllTypes& message) { message.mutable_single_float_wrapper(); });
}

TEST_P(ProtoStructValueTest, DoubleWrapperHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_double_wrapper",
      [](TestAllTypes& message) { message.mutable_single_double_wrapper(); });
}

TEST_P(ProtoStructValueTest, BytesWrapperHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_bytes_wrapper",
      [](TestAllTypes& message) { message.mutable_single_bytes_wrapper(); });
}

TEST_P(ProtoStructValueTest, StringWrapperHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "single_string_wrapper",
      [](TestAllTypes& message) { message.mutable_single_string_wrapper(); });
}

TEST_P(ProtoStructValueTest, ListValueHasField) {
  TEST_HAS_FIELD(memory_manager(), "list_value",
                 [](TestAllTypes& message) { message.mutable_list_value(); });
}

TEST_P(ProtoStructValueTest, StructHasField) {
  TEST_HAS_FIELD(memory_manager(), "single_struct", [](TestAllTypes& message) {
    message.mutable_single_struct();
  });
}

TEST_P(ProtoStructValueTest, ValueHasField) {
  TEST_HAS_FIELD(memory_manager(), "single_value",
                 [](TestAllTypes& message) { message.mutable_single_value(); });
}

TEST_P(ProtoStructValueTest, NullValueListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_null_value",
                 [](TestAllTypes& message) {
                   message.add_repeated_null_value(NULL_VALUE);
                 });
}

TEST_P(ProtoStructValueTest, BoolListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_bool", [](TestAllTypes& message) {
    message.add_repeated_bool(true);
  });
}

TEST_P(ProtoStructValueTest, Int32ListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_int32", [](TestAllTypes& message) {
    message.add_repeated_int32(true);
  });
}

TEST_P(ProtoStructValueTest, Int64ListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_int64",
                 [](TestAllTypes& message) { message.add_repeated_int64(1); });
}

TEST_P(ProtoStructValueTest, Uint32ListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_uint32",
                 [](TestAllTypes& message) { message.add_repeated_uint32(1); });
}

TEST_P(ProtoStructValueTest, Uint64ListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_uint64",
                 [](TestAllTypes& message) { message.add_repeated_uint64(1); });
}

TEST_P(ProtoStructValueTest, FloatListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_float", [](TestAllTypes& message) {
    message.add_repeated_float(1.0);
  });
}

TEST_P(ProtoStructValueTest, DoubleListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_double",
      [](TestAllTypes& message) { message.add_repeated_double(1.0); });
}

TEST_P(ProtoStructValueTest, BytesListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_bytes", [](TestAllTypes& message) {
    message.add_repeated_bytes("foo");
  });
}

TEST_P(ProtoStructValueTest, StringListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_string",
      [](TestAllTypes& message) { message.add_repeated_string("foo"); });
}

TEST_P(ProtoStructValueTest, DurationListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_duration",
                 [](TestAllTypes& message) {
                   message.add_repeated_duration()->set_seconds(1);
                 });
}

TEST_P(ProtoStructValueTest, TimestampListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_timestamp",
                 [](TestAllTypes& message) {
                   message.add_repeated_timestamp()->set_seconds(1);
                 });
}

TEST_P(ProtoStructValueTest, EnumListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_nested_enum",
                 [](TestAllTypes& message) {
                   message.add_repeated_nested_enum(TestAllTypes::BAR);
                 });
}

TEST_P(ProtoStructValueTest, MessageListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_nested_message",
      [](TestAllTypes& message) { message.add_repeated_nested_message(); });
}

TEST_P(ProtoStructValueTest, BoolWrapperListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_bool_wrapper",
      [](TestAllTypes& message) { message.add_repeated_bool_wrapper(); });
}

TEST_P(ProtoStructValueTest, Int32WrapperListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_int32_wrapper",
      [](TestAllTypes& message) { message.add_repeated_int32_wrapper(); });
}

TEST_P(ProtoStructValueTest, Int64WrapperListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_int64_wrapper",
      [](TestAllTypes& message) { message.add_repeated_int64_wrapper(); });
}

TEST_P(ProtoStructValueTest, Uint32WrapperListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_uint32_wrapper",
      [](TestAllTypes& message) { message.add_repeated_uint32_wrapper(); });
}

TEST_P(ProtoStructValueTest, Uint64WrapperListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_uint64_wrapper",
      [](TestAllTypes& message) { message.add_repeated_uint64_wrapper(); });
}

TEST_P(ProtoStructValueTest, FloatWrapperListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_float_wrapper",
      [](TestAllTypes& message) { message.add_repeated_float_wrapper(); });
}

TEST_P(ProtoStructValueTest, DoubleWrapperListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_double_wrapper",
      [](TestAllTypes& message) { message.add_repeated_double_wrapper(); });
}

TEST_P(ProtoStructValueTest, BytesWrapperListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_bytes_wrapper",
      [](TestAllTypes& message) { message.add_repeated_bytes_wrapper(); });
}

TEST_P(ProtoStructValueTest, StringWrapperListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_string_wrapper",
      [](TestAllTypes& message) { message.add_repeated_string_wrapper(); });
}

TEST_P(ProtoStructValueTest, ListValueListHasField) {
  TEST_HAS_FIELD(
      memory_manager(), "repeated_list_value",
      [](TestAllTypes& message) { message.add_repeated_list_value(); });
}

TEST_P(ProtoStructValueTest, StructListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_struct",
                 [](TestAllTypes& message) { message.add_repeated_struct(); });
}

TEST_P(ProtoStructValueTest, ValueListHasField) {
  TEST_HAS_FIELD(memory_manager(), "repeated_value",
                 [](TestAllTypes& message) { message.add_repeated_value(); });
}

void TestGetFieldImpl(
    MemoryManager& memory_manager,
    absl::FunctionRef<absl::StatusOr<Handle<Value>>(
        const Handle<StructValue>&, const StructValue::GetFieldContext&)>
        get_field,
    absl::FunctionRef<void(const Handle<Value>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<Value>&)>
        set_field_tester) {
  TypeFactory type_factory(memory_manager);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value_without,
                       ProtoValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      get_field(value_without, StructValue::GetFieldContext(value_factory)));
  ASSERT_NO_FATAL_FAILURE(unset_field_tester(field));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoValue::Create(value_factory, CreateTestMessage(test_message_maker)));
  ASSERT_OK_AND_ASSIGN(
      field,
      get_field(value_with, StructValue::GetFieldContext(value_factory)));
  ASSERT_NO_FATAL_FAILURE(set_field_tester(value_factory, field));
}

void TestGetFieldByName(
    MemoryManager& memory_manager, absl::string_view name,
    absl::FunctionRef<void(const Handle<Value>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<Value>&)>
        set_field_tester) {
  TestGetFieldImpl(
      memory_manager,
      [&](const Handle<StructValue>& value,
          const StructValue::GetFieldContext& context) {
        return value->GetFieldByName(context, name);
      },
      unset_field_tester, test_message_maker, set_field_tester);
}

void TestGetFieldByNumber(
    MemoryManager& memory_manager, int64_t number,
    absl::FunctionRef<void(const Handle<Value>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<Value>&)>
        set_field_tester) {
  TestGetFieldImpl(
      memory_manager,
      [&](const Handle<StructValue>& value,
          const StructValue::GetFieldContext& context) {
        return value->GetFieldByNumber(context, number);
      },
      unset_field_tester, test_message_maker, set_field_tester);
}

void TestGetField(
    MemoryManager& memory_manager, absl::string_view name,
    absl::FunctionRef<void(const Handle<Value>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<Value>&)>
        set_field_tester) {
  TestGetFieldByName(memory_manager, name, unset_field_tester,
                     test_message_maker, set_field_tester);
  TestGetFieldByNumber(memory_manager, TestMessageFieldNameToNumber(name),
                       unset_field_tester, test_message_maker,
                       set_field_tester);
}

void TestGetField(
    MemoryManager& memory_manager, absl::string_view name,
    absl::FunctionRef<void(const Handle<Value>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(const Handle<Value>&)> set_field_tester) {
  TestGetField(memory_manager, name, unset_field_tester, test_message_maker,
               [&](ValueFactory& value_factory, const Handle<Value>& field) {
                 set_field_tester(field);
               });
}

#define TEST_GET_FIELD(...) ASSERT_NO_FATAL_FAILURE(TestGetField(__VA_ARGS__))

TEST_P(ProtoStructValueTest, NullValueGetField) {
  TEST_GET_FIELD(
      memory_manager(), "null_value",
      [](const Handle<Value>& field) { EXPECT_TRUE(field->Is<NullValue>()); },
      [](TestAllTypes& message) { message.set_null_value(NULL_VALUE); },
      [](const Handle<Value>& field) { EXPECT_TRUE(field->Is<NullValue>()); });
}

TEST_P(ProtoStructValueTest, OptionalNullValueGetField) {
  TEST_GET_FIELD(
      memory_manager(), "optional_null_value",
      [](const Handle<Value>& field) { EXPECT_TRUE(field->Is<NullValue>()); },
      [](TestAllTypes& message) {
        message.set_optional_null_value(NULL_VALUE);
      },
      [](const Handle<Value>& field) { EXPECT_TRUE(field->Is<NullValue>()); });
}

TEST_P(ProtoStructValueTest, BoolGetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_bool",
      [](const Handle<Value>& field) {
        EXPECT_FALSE(field.As<BoolValue>()->value());
      },
      [](TestAllTypes& message) { message.set_single_bool(true); },
      [](const Handle<Value>& field) {
        EXPECT_TRUE(field.As<BoolValue>()->value());
      });
}

TEST_P(ProtoStructValueTest, Int32GetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_int32",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<IntValue>()->value(), 0);
      },
      [](TestAllTypes& message) { message.set_single_int32(1); },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<IntValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, Int64GetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_int64",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<IntValue>()->value(), 0);
      },
      [](TestAllTypes& message) { message.set_single_int64(1); },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<IntValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, Uint32GetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_uint32",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<UintValue>()->value(), 0);
      },
      [](TestAllTypes& message) { message.set_single_uint32(1); },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<UintValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, Uint64GetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_uint64",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<UintValue>()->value(), 0);
      },
      [](TestAllTypes& message) { message.set_single_uint64(1); },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<UintValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, FloatGetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_float",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<DoubleValue>()->value(), 0);
      },
      [](TestAllTypes& message) { message.set_single_float(1.0); },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<DoubleValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, DoubleGetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_double",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<DoubleValue>()->value(), 0);
      },
      [](TestAllTypes& message) { message.set_single_double(1.0); },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<DoubleValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, BytesGetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_bytes",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<BytesValue>()->ToString(), "");
      },
      [](TestAllTypes& message) { message.set_single_bytes("foo"); },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<BytesValue>()->ToString(), "foo");
      });
}

TEST_P(ProtoStructValueTest, StringGetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_string",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<StringValue>()->ToString(), "");
      },
      [](TestAllTypes& message) { message.set_single_string("foo"); },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<StringValue>()->ToString(), "foo");
      });
}

TEST_P(ProtoStructValueTest, DurationGetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_duration",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<DurationValue>()->value(), absl::ZeroDuration());
      },
      [](TestAllTypes& message) {
        message.mutable_single_duration()->set_seconds(1);
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<DurationValue>()->value(), absl::Seconds(1));
      });
}

TEST_P(ProtoStructValueTest, TimestampGetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_timestamp",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<TimestampValue>()->value(), absl::UnixEpoch());
      },
      [](TestAllTypes& message) {
        message.mutable_single_timestamp()->set_seconds(1);
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<TimestampValue>()->value(),
                  absl::UnixEpoch() + absl::Seconds(1));
      });
}

TEST_P(ProtoStructValueTest, EnumGetField) {
  TEST_GET_FIELD(
      memory_manager(), "standalone_enum",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<EnumValue>()->number(), 0);
      },
      [](TestAllTypes& message) {
        message.set_standalone_enum(TestAllTypes::BAR);
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<EnumValue>()->number(), 1);
      });
}

TEST_P(ProtoStructValueTest, MessageGetField) {
  TEST_GET_FIELD(
      memory_manager(), "standalone_message",
      [](const Handle<Value>& field) {
        EXPECT_THAT(*field.As<ProtoStructValue>()->value(),
                    EqualsProto(CreateTestMessage().standalone_message()));
      },
      [](TestAllTypes& message) {
        message.mutable_standalone_message()->set_bb(1);
      },
      [](const Handle<Value>& field) {
        TestAllTypes::NestedMessage expected =
            CreateTestMessage([](TestAllTypes& message) {
              message.mutable_standalone_message()->set_bb(1);
            }).standalone_message();
        TestAllTypes::NestedMessage scratch;
        EXPECT_THAT(*field.As<ProtoStructValue>()->value(),
                    EqualsProto(expected));
        EXPECT_THAT(*field.As<ProtoStructValue>()->value(scratch),
                    EqualsProto(expected));
        google::protobuf::Arena arena;
        EXPECT_THAT(*field.As<ProtoStructValue>()->value(arena),
                    EqualsProto(expected));
      });
}

void TestGetWrapperFieldImpl(
    MemoryManager& memory_manager,
    absl::FunctionRef<absl::StatusOr<Handle<Value>>(
        const Handle<StructValue>&, const StructValue::GetFieldContext&)>
        get_field,
    absl::FunctionRef<void(const Handle<Value>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<Value>&)>
        set_field_tester) {
  TypeFactory type_factory(memory_manager);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value_without,
                       ProtoValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      get_field(value_without, StructValue::GetFieldContext(value_factory)
                                   .set_unbox_null_wrapper_types(true)));
  EXPECT_TRUE(field->Is<NullValue>());
  ASSERT_OK_AND_ASSIGN(
      field,
      get_field(value_without, StructValue::GetFieldContext(value_factory)
                                   .set_unbox_null_wrapper_types(false)));
  ASSERT_NO_FATAL_FAILURE(unset_field_tester(field));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoValue::Create(value_factory, CreateTestMessage(test_message_maker)));
  ASSERT_OK_AND_ASSIGN(
      field,
      get_field(value_with, StructValue::GetFieldContext(value_factory)));
  ASSERT_NO_FATAL_FAILURE(set_field_tester(value_factory, field));
}

void TestGetWrapperFieldByName(
    MemoryManager& memory_manager, absl::string_view name,
    absl::FunctionRef<void(const Handle<Value>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<Value>&)>
        set_field_tester) {
  TestGetWrapperFieldImpl(
      memory_manager,
      [&](const Handle<StructValue>& value,
          const StructValue::GetFieldContext& context) {
        return value->GetFieldByName(context, name);
      },
      unset_field_tester, test_message_maker, set_field_tester);
}

void TestGetWrapperFieldByNumber(
    MemoryManager& memory_manager, int64_t number,
    absl::FunctionRef<void(const Handle<Value>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<Value>&)>
        set_field_tester) {
  TestGetWrapperFieldImpl(
      memory_manager,
      [&](const Handle<StructValue>& value,
          const StructValue::GetFieldContext& context) {
        return value->GetFieldByNumber(context, number);
      },
      unset_field_tester, test_message_maker, set_field_tester);
}

void TestGetWrapperField(
    MemoryManager& memory_manager, absl::string_view name,
    absl::FunctionRef<void(const Handle<Value>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<Value>&)>
        set_field_tester) {
  TestGetWrapperFieldByName(memory_manager, name, unset_field_tester,
                            test_message_maker, set_field_tester);
  TestGetWrapperFieldByNumber(
      memory_manager, TestMessageFieldNameToNumber(name), unset_field_tester,
      test_message_maker, set_field_tester);
}

void TestGetWrapperField(
    MemoryManager& memory_manager, absl::string_view name,
    absl::FunctionRef<void(const Handle<Value>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(const Handle<Value>&)> set_field_tester) {
  TestGetWrapperField(
      memory_manager, name, unset_field_tester, test_message_maker,
      [&](ValueFactory& value_factory, const Handle<Value>& field) {
        set_field_tester(field);
      });
}

#define TEST_GET_WRAPPER_FIELD(...) \
  ASSERT_NO_FATAL_FAILURE(TestGetWrapperField(__VA_ARGS__))

TEST_P(ProtoStructValueTest, BoolWrapperGetField) {
  TEST_GET_WRAPPER_FIELD(
      memory_manager(), "single_bool_wrapper",
      [](const Handle<Value>& field) {
        EXPECT_FALSE(field.As<BoolValue>()->value());
      },
      [](TestAllTypes& message) {
        message.mutable_single_bool_wrapper()->set_value(true);
      },
      [](const Handle<Value>& field) {
        EXPECT_TRUE(field.As<BoolValue>()->value());
      });
}

TEST_P(ProtoStructValueTest, Int32WrapperGetField) {
  TEST_GET_WRAPPER_FIELD(
      memory_manager(), "single_int32_wrapper",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<IntValue>()->value(), 0);
      },
      [](TestAllTypes& message) {
        message.mutable_single_int32_wrapper()->set_value(1);
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<IntValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, Int64WrapperGetField) {
  TEST_GET_WRAPPER_FIELD(
      memory_manager(), "single_int64_wrapper",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<IntValue>()->value(), 0);
      },
      [](TestAllTypes& message) {
        message.mutable_single_int64_wrapper()->set_value(1);
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<IntValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, Uint32WrapperGetField) {
  TEST_GET_WRAPPER_FIELD(
      memory_manager(), "single_uint32_wrapper",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<UintValue>()->value(), 0);
      },
      [](TestAllTypes& message) {
        message.mutable_single_uint32_wrapper()->set_value(1);
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<UintValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, Uint64WrapperGetField) {
  TEST_GET_WRAPPER_FIELD(
      memory_manager(), "single_uint64_wrapper",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<UintValue>()->value(), 0);
      },
      [](TestAllTypes& message) {
        message.mutable_single_uint64_wrapper()->set_value(1);
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<UintValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, FloatWrapperGetField) {
  TEST_GET_WRAPPER_FIELD(
      memory_manager(), "single_float_wrapper",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<DoubleValue>()->value(), 0);
      },
      [](TestAllTypes& message) {
        message.mutable_single_float_wrapper()->set_value(1.0);
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<DoubleValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, DoubleWrapperGetField) {
  TEST_GET_WRAPPER_FIELD(
      memory_manager(), "single_double_wrapper",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<DoubleValue>()->value(), 0);
      },
      [](TestAllTypes& message) {
        message.mutable_single_double_wrapper()->set_value(1.0);
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<DoubleValue>()->value(), 1);
      });
}

TEST_P(ProtoStructValueTest, BytesWrapperGetField) {
  TEST_GET_WRAPPER_FIELD(
      memory_manager(), "single_bytes_wrapper",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<BytesValue>()->ToString(), "");
      },
      [](TestAllTypes& message) {
        message.mutable_single_bytes_wrapper()->set_value("foo");
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<BytesValue>()->ToString(), "foo");
      });
}

TEST_P(ProtoStructValueTest, StringWrapperGetField) {
  TEST_GET_WRAPPER_FIELD(
      memory_manager(), "single_string_wrapper",
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<StringValue>()->ToString(), "");
      },
      [](TestAllTypes& message) {
        message.mutable_single_string_wrapper()->set_value("foo");
      },
      [](const Handle<Value>& field) {
        EXPECT_EQ(field.As<StringValue>()->ToString(), "foo");
      });
}

TEST_P(ProtoStructValueTest, StructGetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_struct",
      [](const Handle<Value>& field) {
        ASSERT_TRUE(field->Is<MapValue>());
        EXPECT_TRUE(field->As<MapValue>().empty());
      },
      [](TestAllTypes& message) {
        google::protobuf::Value value_proto;
        value_proto.set_bool_value(true);
        message.mutable_single_struct()->mutable_fields()->insert(
            {"foo", std::move(value_proto)});
      },
      [](ValueFactory& value_factory, const Handle<Value>& field) {
        ASSERT_TRUE(field->Is<MapValue>());
        EXPECT_EQ(field->As<MapValue>().size(), 1);
        ASSERT_OK_AND_ASSIGN(auto key, value_factory.CreateStringValue("foo"));
        EXPECT_THAT(
            field->As<MapValue>().Get(MapValue::GetContext(value_factory), key),
            IsOkAndHolds(Optional(ValueOf<BoolValue>(value_factory, true))));
      });
}

TEST_P(ProtoStructValueTest, ListValueGetField) {
  TEST_GET_FIELD(
      memory_manager(), "list_value",
      [](const Handle<Value>& field) {
        ASSERT_TRUE(field->Is<ListValue>());
        EXPECT_TRUE(field->As<ListValue>().empty());
      },
      [](TestAllTypes& message) {
        message.mutable_list_value()->add_values()->set_bool_value(true);
      },
      [](ValueFactory& value_factory, const Handle<Value>& field) {
        ASSERT_TRUE(field->Is<ListValue>());
        EXPECT_EQ(field->As<ListValue>().size(), 1);
        EXPECT_THAT(
            field->As<ListValue>().Get(ListValue::GetContext(value_factory), 0),
            IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
      });
}

TEST_P(ProtoStructValueTest, ValueGetField) {
  TEST_GET_FIELD(
      memory_manager(), "single_value",
      [](const Handle<Value>& field) { EXPECT_TRUE(field->Is<NullValue>()); },
      [](TestAllTypes& message) {
        message.mutable_single_value()->set_bool_value(true);
      },
      [](const Handle<Value>& field) {
        EXPECT_TRUE(field->As<BoolValue>().value());
      });
}

void TestGetListFieldImpl(
    MemoryManager& memory_manager,
    absl::FunctionRef<absl::StatusOr<Handle<Value>>(
        const Handle<StructValue>&, const StructValue::GetFieldContext&)>
        get_field,
    absl::FunctionRef<void(const Handle<ListValue>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<ListValue>&)>
        set_field_tester) {
  TypeFactory type_factory(memory_manager);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value_without,
                       ProtoValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      get_field(value_without, StructValue::GetFieldContext(value_factory)));
  ASSERT_TRUE(field->Is<ListValue>());
  ASSERT_NO_FATAL_FAILURE(unset_field_tester(field.As<ListValue>()));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoValue::Create(value_factory, CreateTestMessage(test_message_maker)));
  ASSERT_OK_AND_ASSIGN(
      field,
      get_field(value_with, StructValue::GetFieldContext(value_factory)));
  ASSERT_TRUE(field->Is<ListValue>());
  ASSERT_NO_FATAL_FAILURE(
      set_field_tester(value_factory, field.As<ListValue>()));
}

void TestGetListFieldByName(
    MemoryManager& memory_manager, absl::string_view name,
    absl::FunctionRef<void(const Handle<ListValue>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<ListValue>&)>
        set_field_tester) {
  TestGetListFieldImpl(
      memory_manager,
      [&](const Handle<StructValue>& value,
          const StructValue::GetFieldContext& context) {
        return value->GetFieldByName(context, name);
      },
      unset_field_tester, test_message_maker, set_field_tester);
}

void TestGetListFieldByNumber(
    MemoryManager& memory_manager, int64_t number,
    absl::FunctionRef<void(const Handle<ListValue>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<ListValue>&)>
        set_field_tester) {
  TestGetListFieldImpl(
      memory_manager,
      [&](const Handle<StructValue>& value,
          const StructValue::GetFieldContext& context) {
        return value->GetFieldByNumber(context, number);
      },
      unset_field_tester, test_message_maker, set_field_tester);
}

void TestGetListField(
    MemoryManager& memory_manager, absl::string_view name,
    absl::FunctionRef<void(const Handle<ListValue>&)> unset_field_tester,
    absl::FunctionRef<void(TestAllTypes&)> test_message_maker,
    absl::FunctionRef<void(ValueFactory&, const Handle<ListValue>&)>
        set_field_tester) {
  TestGetListFieldByName(memory_manager, name, unset_field_tester,
                         test_message_maker, set_field_tester);
  TestGetListFieldByNumber(memory_manager, TestMessageFieldNameToNumber(name),
                           unset_field_tester, test_message_maker,
                           set_field_tester);
}

#define TEST_GET_LIST_FIELD(...) \
  ASSERT_NO_FATAL_FAILURE(TestGetListField(__VA_ARGS__))

void EmptyListFieldTester(const Handle<ListValue>& field) {
  EXPECT_EQ(field->size(), 0);
  EXPECT_TRUE(field->empty());
  EXPECT_EQ(field->DebugString(), "[]");
}

TEST_P(ProtoStructValueTest, NullValueListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_null_value", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_null_value(NULL_VALUE);
        message.add_repeated_null_value(NULL_VALUE);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[null, null]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_TRUE(field_value->Is<NullValue>());
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_TRUE(field_value->Is<NullValue>());
      });
}

TEST_P(ProtoStructValueTest, BoolListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_bool", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_bool(true);
        message.add_repeated_bool(false);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[true, false]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_TRUE(field_value.As<BoolValue>()->value());
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_FALSE(field_value.As<BoolValue>()->value());
      });
}

TEST_P(ProtoStructValueTest, Int32ListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_int32", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_int32(1);
        message.add_repeated_int32(0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1, 0]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<IntValue>()->value(), 1);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<IntValue>()->value(), 0);
      });
}

TEST_P(ProtoStructValueTest, Int64ListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_int64", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_int64(1);
        message.add_repeated_int64(0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1, 0]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<IntValue>()->value(), 1);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<IntValue>()->value(), 0);
      });
}

TEST_P(ProtoStructValueTest, Uint32ListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_uint32", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_uint32(1);
        message.add_repeated_uint32(0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1u, 0u]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<UintValue>()->value(), 1);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<UintValue>()->value(), 0);
      });
}

TEST_P(ProtoStructValueTest, Uint64ListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_uint64", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_uint64(1);
        message.add_repeated_uint64(0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1u, 0u]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<UintValue>()->value(), 1);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<UintValue>()->value(), 0);
      });
}

TEST_P(ProtoStructValueTest, FloatListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_float", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_float(1.0);
        message.add_repeated_float(0.0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1.0, 0.0]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<DoubleValue>()->value(), 1.0);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<DoubleValue>()->value(), 0.0);
      });
}

TEST_P(ProtoStructValueTest, DoubleListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_double", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_double(1.0);
        message.add_repeated_double(0.0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1.0, 0.0]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<DoubleValue>()->value(), 1.0);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<DoubleValue>()->value(), 0.0);
      });
}

TEST_P(ProtoStructValueTest, BytesListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_bytes", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_bytes("foo");
        message.add_repeated_bytes("bar");
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[b\"foo\", b\"bar\"]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<BytesValue>()->ToString(), "foo");
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<BytesValue>()->ToString(), "bar");
      });
}

TEST_P(ProtoStructValueTest, StringListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_string", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_string("foo");
        message.add_repeated_string("bar");
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[\"foo\", \"bar\"]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<StringValue>()->ToString(), "foo");
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<StringValue>()->ToString(), "bar");
      });
}

TEST_P(ProtoStructValueTest, DurationListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_duration", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_duration()->set_seconds(1);
        message.add_repeated_duration()->set_seconds(2);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1s, 2s]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<DurationValue>()->value(), absl::Seconds(1));
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<DurationValue>()->value(), absl::Seconds(2));
      });
}

TEST_P(ProtoStructValueTest, TimestampListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_timestamp", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_timestamp()->set_seconds(1);
        message.add_repeated_timestamp()->set_seconds(2);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(),
                  "[1970-01-01T00:00:01Z, 1970-01-01T00:00:02Z]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<TimestampValue>()->value(),
                  absl::UnixEpoch() + absl::Seconds(1));
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<TimestampValue>()->value(),
                  absl::UnixEpoch() + absl::Seconds(2));
      });
}

TEST_P(ProtoStructValueTest, EnumListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_nested_enum", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_nested_enum(TestAllTypes::FOO);
        message.add_repeated_nested_enum(TestAllTypes::BAR);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(
            field->DebugString(),
            "[google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.FOO, "
            "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAR]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<EnumValue>()->number(), TestAllTypes::FOO);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<EnumValue>()->number(), TestAllTypes::BAR);
      });
}

TEST_P(ProtoStructValueTest, StructListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_nested_message", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_nested_message()->set_bb(1);
        message.add_repeated_nested_message()->set_bb(2);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(),
                  "[google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{"
                  "bb: 1}, "
                  "google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{"
                  "bb: 2}]");
        TestAllTypes::NestedMessage message;
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        message.set_bb(1);
        EXPECT_THAT(*field_value.As<ProtoStructValue>()->value(),
                    EqualsProto(message));
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        message.set_bb(2);
        EXPECT_THAT(*field_value.As<ProtoStructValue>()->value(),
                    EqualsProto(message));
      });
}

TEST_P(ProtoStructValueTest, BoolWrapperListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_bool_wrapper", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_bool_wrapper()->set_value(true);
        message.add_repeated_bool_wrapper()->set_value(false);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[true, false]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_TRUE(field_value.As<BoolValue>()->value());
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_FALSE(field_value.As<BoolValue>()->value());
      });
}

TEST_P(ProtoStructValueTest, Int32WrapperListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_int32_wrapper", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_int32_wrapper()->set_value(1);
        message.add_repeated_int32_wrapper()->set_value(0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1, 0]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<IntValue>()->value(), 1);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<IntValue>()->value(), 0);
      });
}

TEST_P(ProtoStructValueTest, Int64WrapperListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_int64_wrapper", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_int64_wrapper()->set_value(1);
        message.add_repeated_int64_wrapper()->set_value(0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1, 0]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<IntValue>()->value(), 1);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<IntValue>()->value(), 0);
      });
}

TEST_P(ProtoStructValueTest, Uint32WrapperListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_uint32_wrapper", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_uint32_wrapper()->set_value(1);
        message.add_repeated_uint32_wrapper()->set_value(0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1u, 0u]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<UintValue>()->value(), 1);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<UintValue>()->value(), 0);
      });
}

TEST_P(ProtoStructValueTest, Uint64WrapperListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_uint64_wrapper", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_uint64_wrapper()->set_value(1);
        message.add_repeated_uint64_wrapper()->set_value(0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1u, 0u]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<UintValue>()->value(), 1);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<UintValue>()->value(), 0);
      });
}

TEST_P(ProtoStructValueTest, FloatWrapperListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_float_wrapper", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_float_wrapper()->set_value(1.0);
        message.add_repeated_float_wrapper()->set_value(0.0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1.0, 0.0]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<DoubleValue>()->value(), 1.0);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<DoubleValue>()->value(), 0.0);
      });
}

TEST_P(ProtoStructValueTest, DoubleWrapperListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_double_wrapper", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_double_wrapper()->set_value(1.0);
        message.add_repeated_double_wrapper()->set_value(0.0);
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[1.0, 0.0]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<DoubleValue>()->value(), 1.0);
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<DoubleValue>()->value(), 0.0);
      });
}

TEST_P(ProtoStructValueTest, BytesWrapperListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_bytes_wrapper", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_bytes_wrapper()->set_value("foo");
        message.add_repeated_bytes_wrapper()->set_value("bar");
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[b\"foo\", b\"bar\"]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<BytesValue>()->ToString(), "foo");
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<BytesValue>()->ToString(), "bar");
      });
}

TEST_P(ProtoStructValueTest, StringWrapperListGetField) {
  TEST_GET_LIST_FIELD(
      memory_manager(), "repeated_string_wrapper", EmptyListFieldTester,
      [](TestAllTypes& message) {
        message.add_repeated_string_wrapper()->set_value("foo");
        message.add_repeated_string_wrapper()->set_value("bar");
      },
      [](ValueFactory& value_factory, const Handle<ListValue>& field) {
        EXPECT_EQ(field->size(), 2);
        EXPECT_FALSE(field->empty());
        EXPECT_EQ(field->DebugString(), "[\"foo\", \"bar\"]");
        ASSERT_OK_AND_ASSIGN(
            auto field_value,
            field->Get(ListValue::GetContext(value_factory), 0));
        EXPECT_EQ(field_value.As<StringValue>()->ToString(), "foo");
        ASSERT_OK_AND_ASSIGN(
            field_value, field->Get(ListValue::GetContext(value_factory), 1));
        EXPECT_EQ(field_value.As<StringValue>()->ToString(), "bar");
      });
}

template <typename MutableMapField, typename Pair>
void TestMapHasField(MemoryManager& memory_manager,
                     absl::string_view map_field_name,
                     MutableMapField mutable_map_field, Pair&& pair) {
  TypeFactory type_factory(memory_manager);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value_without,
                       ProtoValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasFieldByName(
                  StructValue::HasFieldContext(type_manager), map_field_name),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoValue::Create(
          value_factory, CreateTestMessage([&mutable_map_field,
                                            pair = std::forward<Pair>(pair)](
                                               TestAllTypes& message) mutable {
            (message.*mutable_map_field)()->insert(std::forward<Pair>(pair));
          })));
  EXPECT_THAT(value_with->HasFieldByName(
                  StructValue::HasFieldContext(type_manager), map_field_name),
              IsOkAndHolds(Eq(true)));
}

template <typename T>
std::decay_t<T> ProtoToNative(const T& t) {
  return t;
}

absl::Duration ProtoToNative(const google::protobuf::Duration& duration) {
  return absl::Seconds(duration.seconds()) +
         absl::Nanoseconds(duration.nanos());
}

absl::Time ProtoToNative(const google::protobuf::Timestamp& timestamp) {
  return absl::UnixEpoch() + absl::Seconds(timestamp.seconds()) +
         absl::Nanoseconds(timestamp.nanos());
}

google::protobuf::Duration NativeToProto(absl::Duration duration) {
  google::protobuf::Duration duration_proto;
  duration_proto.set_seconds(
      absl::ToInt64Seconds(absl::Trunc(duration, absl::Seconds(1))));
  duration -= absl::Trunc(duration, absl::Seconds(1));
  duration_proto.set_nanos(absl::ToInt64Nanoseconds(duration));
  return duration_proto;
}

google::protobuf::Timestamp NativeToProto(absl::Time time) {
  absl::Duration duration = time - absl::UnixEpoch();
  google::protobuf::Timestamp timestamp_proto;
  timestamp_proto.set_seconds(
      absl::ToInt64Seconds(absl::Trunc(duration, absl::Seconds(1))));
  duration -= absl::Trunc(duration, absl::Seconds(1));
  timestamp_proto.set_nanos(absl::ToInt64Nanoseconds(duration));
  return timestamp_proto;
}

template <typename T, typename MutableMapField, typename Creator,
          typename Valuer, typename Pair, typename Key>
void TestMapGetField(MemoryManager& memory_manager,
                     absl::string_view map_field_name,
                     absl::string_view debug_string,
                     MutableMapField mutable_map_field, Creator creator,
                     Valuer valuer, const Pair& pair1, const Pair& pair2,
                     const Key& missing_key) {
  TypeFactory type_factory(memory_manager);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value_without,
                       ProtoValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      value_without->GetFieldByName(StructValue::GetFieldContext(value_factory),
                                    map_field_name));
  EXPECT_TRUE(field->Is<MapValue>());
  EXPECT_EQ(field.As<MapValue>()->size(), 0);
  EXPECT_TRUE(field.As<MapValue>()->empty());
  EXPECT_EQ(field->DebugString(), "{}");
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoValue::Create(value_factory,
                         CreateTestMessage([&mutable_map_field, &pair1, &pair2](
                                               TestAllTypes& message) mutable {
                           (message.*mutable_map_field)()->insert(pair1);
                           (message.*mutable_map_field)()->insert(pair2);
                         })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetFieldByName(
                 StructValue::GetFieldContext(value_factory), map_field_name));
  EXPECT_TRUE(field->Is<MapValue>());
  EXPECT_EQ(field.As<MapValue>()->size(), 2);
  EXPECT_FALSE(field.As<MapValue>()->empty());
  EXPECT_EQ(field->DebugString(), debug_string);
  ASSERT_OK_AND_ASSIGN(
      auto field_value,
      field.As<MapValue>()->Get(MapValue::GetContext(value_factory),
                                Must((value_factory.*creator)(pair1.first))));
  if constexpr (std::is_same_v<T, ProtoStructValue>) {
    EXPECT_THAT(*((*field_value).template As<ProtoStructValue>()->value()),
                EqualsProto(pair1.second));
  } else if constexpr (std::is_same_v<T, NullValue>) {
    EXPECT_TRUE((*field_value)->template Is<NullValue>());
  } else {
    EXPECT_EQ(((*(*field_value).template As<T>()).*valuer)(),
              ProtoToNative(pair1.second));
  }
  EXPECT_THAT(
      field.As<MapValue>()->Has(MapValue::HasContext(),
                                Must((value_factory.*creator)(pair1.first))),
      IsOkAndHolds(Eq(true)));
  ASSERT_OK_AND_ASSIGN(
      field_value,
      field.As<MapValue>()->Get(MapValue::GetContext(value_factory),
                                Must((value_factory.*creator)(pair2.first))));
  if constexpr (std::is_same_v<T, ProtoStructValue>) {
    EXPECT_THAT(*((*field_value).template As<ProtoStructValue>()->value()),
                EqualsProto(pair2.second));
  } else if constexpr (std::is_same_v<T, NullValue>) {
    EXPECT_TRUE((*field_value)->template Is<NullValue>());
  } else {
    EXPECT_EQ(((*(*field_value).template As<T>()).*valuer)(),
              ProtoToNative(pair2.second));
  }
  EXPECT_THAT(
      field.As<MapValue>()->Has(MapValue::HasContext(),
                                Must((value_factory.*creator)(pair2.first))),
      IsOkAndHolds(Eq(true)));
  if constexpr (!std::is_null_pointer_v<Key>) {
    EXPECT_THAT(
        field.As<MapValue>()->Get(MapValue::GetContext(value_factory),
                                  Must((value_factory.*creator)(missing_key))),
        IsOkAndHolds(Eq(absl::nullopt)));
  }
  EXPECT_THAT(field.As<MapValue>()->Get(
                  MapValue::GetContext(value_factory),
                  value_factory.CreateErrorValue(absl::CancelledError())),
              CanonicalStatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(field.As<MapValue>()->Has(
                  MapValue::HasContext(),
                  value_factory.CreateErrorValue(absl::CancelledError())),
              CanonicalStatusIs(absl::StatusCode::kInvalidArgument));
  ASSERT_OK_AND_ASSIGN(
      auto keys,
      field.As<MapValue>()->ListKeys(MapValue::ListKeysContext(value_factory)));
  EXPECT_EQ(keys->size(), 2);
  EXPECT_FALSE(keys->empty());
  EXPECT_EQ(field.As<MapValue>()->type()->key(), keys->type()->element());
  EXPECT_OK(keys->Get(ListValue::GetContext(value_factory), 0));
}

template <typename T, typename MutableMapField, typename Valuer, typename Pair,
          typename Key>
void TestStringMapGetField(MemoryManager& memory_manager,
                           absl::string_view map_field_name,
                           absl::string_view debug_string,
                           MutableMapField mutable_map_field, Valuer valuer,
                           const Pair& pair1, const Pair& pair2,
                           const Key& missing_key) {
  TypeFactory type_factory(memory_manager);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value_without,
                       ProtoValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      value_without->GetFieldByName(StructValue::GetFieldContext(value_factory),
                                    map_field_name));
  EXPECT_TRUE(field->Is<MapValue>());
  EXPECT_EQ(field.As<MapValue>()->size(), 0);
  EXPECT_TRUE(field.As<MapValue>()->empty());
  EXPECT_EQ(field->DebugString(), "{}");
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoValue::Create(value_factory,
                         CreateTestMessage([&mutable_map_field, &pair1, &pair2](
                                               TestAllTypes& message) mutable {
                           (message.*mutable_map_field)()->insert(pair1);
                           (message.*mutable_map_field)()->insert(pair2);
                         })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetFieldByName(
                 StructValue::GetFieldContext(value_factory), map_field_name));
  EXPECT_TRUE(field->Is<MapValue>());
  EXPECT_EQ(field.As<MapValue>()->size(), 2);
  EXPECT_FALSE(field.As<MapValue>()->empty());
  EXPECT_EQ(field->DebugString(), debug_string);
  ASSERT_OK_AND_ASSIGN(auto field_value,
                       field.As<MapValue>()->Get(
                           MapValue::GetContext(value_factory),
                           Must(value_factory.CreateStringValue(pair1.first))));
  if constexpr (std::is_same_v<T, ProtoStructValue>) {
    EXPECT_THAT(*((*field_value).template As<ProtoStructValue>()->value()),
                EqualsProto(pair1.second));
  } else if constexpr (std::is_same_v<T, NullValue>) {
    EXPECT_TRUE((*field_value)->template Is<NullValue>());
  } else {
    EXPECT_EQ(((*(*field_value).template As<T>()).*valuer)(),
              ProtoToNative(pair1.second));
  }
  EXPECT_THAT(field.As<MapValue>()->Has(
                  MapValue::HasContext(),
                  Must(value_factory.CreateStringValue(pair1.first))),
              IsOkAndHolds(Eq(true)));
  ASSERT_OK_AND_ASSIGN(field_value,
                       field.As<MapValue>()->Get(
                           MapValue::GetContext(value_factory),
                           Must(value_factory.CreateStringValue(pair2.first))));
  if constexpr (std::is_same_v<T, ProtoStructValue>) {
    EXPECT_THAT(*((*field_value).template As<ProtoStructValue>()->value()),
                EqualsProto(pair2.second));
  } else if constexpr (std::is_same_v<T, NullValue>) {
    EXPECT_TRUE((*field_value)->template Is<NullValue>());
  } else {
    EXPECT_EQ(((*(*field_value).template As<T>()).*valuer)(),
              ProtoToNative(pair2.second));
  }
  EXPECT_THAT(field.As<MapValue>()->Has(
                  MapValue::HasContext(),
                  Must(value_factory.CreateStringValue(pair2.first))),
              IsOkAndHolds(Eq(true)));
  EXPECT_THAT(field.As<MapValue>()->Get(
                  MapValue::GetContext(value_factory),
                  Must(value_factory.CreateStringValue(missing_key))),
              IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(field.As<MapValue>()->Get(
                  MapValue::GetContext(value_factory),
                  value_factory.CreateErrorValue(absl::CancelledError())),
              CanonicalStatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(field.As<MapValue>()->Has(
                  MapValue::HasContext(),
                  value_factory.CreateErrorValue(absl::CancelledError())),
              CanonicalStatusIs(absl::StatusCode::kInvalidArgument));
  ASSERT_OK_AND_ASSIGN(
      auto keys,
      field.As<MapValue>()->ListKeys(MapValue::ListKeysContext(value_factory)));
  EXPECT_EQ(keys->size(), 2);
  EXPECT_FALSE(keys->empty());
  EXPECT_EQ(field.As<MapValue>()->type()->key(), keys->type()->element());
  EXPECT_OK(keys->Get(ListValue::GetContext(value_factory), 0));
}

TEST_P(ProtoStructValueTest, BoolNullValueMapHasField) {
  TestMapHasField(memory_manager(), "map_bool_null_value",
                  &TestAllTypes::mutable_map_bool_null_value,
                  std::make_pair(true, NULL_VALUE));
}

TEST_P(ProtoStructValueTest, BoolBoolMapHasField) {
  TestMapHasField(memory_manager(), "map_bool_bool",
                  &TestAllTypes::mutable_map_bool_bool,
                  std::make_pair(true, true));
}

TEST_P(ProtoStructValueTest, BoolInt32MapHasField) {
  TestMapHasField(memory_manager(), "map_bool_int32",
                  &TestAllTypes::mutable_map_bool_int32,
                  std::make_pair(true, 1));
}

TEST_P(ProtoStructValueTest, BoolInt64MapHasField) {
  TestMapHasField(memory_manager(), "map_bool_int64",
                  &TestAllTypes::mutable_map_bool_int64,
                  std::make_pair(true, 1));
}

TEST_P(ProtoStructValueTest, BoolUint32MapHasField) {
  TestMapHasField(memory_manager(), "map_bool_uint32",
                  &TestAllTypes::mutable_map_bool_uint32,
                  std::make_pair(true, 1u));
}

TEST_P(ProtoStructValueTest, BoolUint64MapHasField) {
  TestMapHasField(memory_manager(), "map_bool_uint64",
                  &TestAllTypes::mutable_map_bool_uint64,
                  std::make_pair(true, 1u));
}

TEST_P(ProtoStructValueTest, BoolFloatMapHasField) {
  TestMapHasField(memory_manager(), "map_bool_float",
                  &TestAllTypes::mutable_map_bool_float,
                  std::make_pair(true, 1.0f));
}

TEST_P(ProtoStructValueTest, BoolDoubleMapHasField) {
  TestMapHasField(memory_manager(), "map_bool_double",
                  &TestAllTypes::mutable_map_bool_double,
                  std::make_pair(true, 1.0));
}

TEST_P(ProtoStructValueTest, BoolBytesMapHasField) {
  TestMapHasField(memory_manager(), "map_bool_bytes",
                  &TestAllTypes::mutable_map_bool_bytes,
                  std::make_pair(true, "foo"));
}

TEST_P(ProtoStructValueTest, BoolStringMapHasField) {
  TestMapHasField(memory_manager(), "map_bool_string",
                  &TestAllTypes::mutable_map_bool_string,
                  std::make_pair(true, "foo"));
}

TEST_P(ProtoStructValueTest, BoolDurationMapHasField) {
  TestMapHasField(memory_manager(), "map_bool_duration",
                  &TestAllTypes::mutable_map_bool_duration,
                  std::make_pair(true, google::protobuf::Duration()));
}

TEST_P(ProtoStructValueTest, BoolTimestampMapHasField) {
  TestMapHasField(memory_manager(), "map_bool_timestamp",
                  &TestAllTypes::mutable_map_bool_timestamp,
                  std::make_pair(true, google::protobuf::Timestamp()));
}

TEST_P(ProtoStructValueTest, BoolEnumMapHasField) {
  TestMapHasField(memory_manager(), "map_bool_enum",
                  &TestAllTypes::mutable_map_bool_enum,
                  std::make_pair(true, TestAllTypes::BAR));
}

TEST_P(ProtoStructValueTest, BoolMessageMapHasField) {
  TestMapHasField(memory_manager(), "map_bool_message",
                  &TestAllTypes::mutable_map_bool_message,
                  std::make_pair(true, TestAllTypes::NestedMessage()));
}

TEST_P(ProtoStructValueTest, Int32NullValueMapHasField) {
  TestMapHasField(memory_manager(), "map_int32_null_value",
                  &TestAllTypes::mutable_map_int32_null_value,
                  std::make_pair(1, NULL_VALUE));
}

TEST_P(ProtoStructValueTest, Int32BoolMapHasField) {
  TestMapHasField(memory_manager(), "map_int32_bool",
                  &TestAllTypes::mutable_map_int32_bool,
                  std::make_pair(1, true));
}

TEST_P(ProtoStructValueTest, Int32Int32MapHasField) {
  TestMapHasField(memory_manager(), "map_int32_int32",
                  &TestAllTypes::mutable_map_int32_int32, std::make_pair(1, 1));
}

TEST_P(ProtoStructValueTest, Int32Int64MapHasField) {
  TestMapHasField(memory_manager(), "map_int32_int64",
                  &TestAllTypes::mutable_map_int32_int64, std::make_pair(1, 1));
}

TEST_P(ProtoStructValueTest, Int32Uint32MapHasField) {
  TestMapHasField(memory_manager(), "map_int32_uint32",
                  &TestAllTypes::mutable_map_int32_uint32,
                  std::make_pair(1, 1u));
}

TEST_P(ProtoStructValueTest, Int32Uint64MapHasField) {
  TestMapHasField(memory_manager(), "map_int32_uint64",
                  &TestAllTypes::mutable_map_int32_uint64,
                  std::make_pair(1, 1u));
}

TEST_P(ProtoStructValueTest, Int32FloatMapHasField) {
  TestMapHasField(memory_manager(), "map_int32_float",
                  &TestAllTypes::mutable_map_int32_float,
                  std::make_pair(1, 1.0f));
}

TEST_P(ProtoStructValueTest, Int32DoubleMapHasField) {
  TestMapHasField(memory_manager(), "map_int32_double",
                  &TestAllTypes::mutable_map_int32_double,
                  std::make_pair(1, 1.0));
}

TEST_P(ProtoStructValueTest, Int32BytesMapHasField) {
  TestMapHasField(memory_manager(), "map_int32_bytes",
                  &TestAllTypes::mutable_map_int32_bytes,
                  std::make_pair(1, "foo"));
}

TEST_P(ProtoStructValueTest, Int32StringMapHasField) {
  TestMapHasField(memory_manager(), "map_int32_string",
                  &TestAllTypes::mutable_map_int32_string,
                  std::make_pair(1, "foo"));
}

TEST_P(ProtoStructValueTest, Int32DurationMapHasField) {
  TestMapHasField(memory_manager(), "map_int32_duration",
                  &TestAllTypes::mutable_map_int32_duration,
                  std::make_pair(1, google::protobuf::Duration()));
}

TEST_P(ProtoStructValueTest, Int32TimestampMapHasField) {
  TestMapHasField(memory_manager(), "map_int32_timestamp",
                  &TestAllTypes::mutable_map_int32_timestamp,
                  std::make_pair(1, google::protobuf::Timestamp()));
}

TEST_P(ProtoStructValueTest, Int32EnumMapHasField) {
  TestMapHasField(memory_manager(), "map_int32_enum",
                  &TestAllTypes::mutable_map_int32_enum,
                  std::make_pair(1, TestAllTypes::BAR));
}

TEST_P(ProtoStructValueTest, Int32MessageMapHasField) {
  TestMapHasField(memory_manager(), "map_int32_message",
                  &TestAllTypes::mutable_map_int32_message,
                  std::make_pair(1, TestAllTypes::NestedMessage()));
}

TEST_P(ProtoStructValueTest, Int64NullValueMapHasField) {
  TestMapHasField(memory_manager(), "map_int64_null_value",
                  &TestAllTypes::mutable_map_int64_null_value,
                  std::make_pair(1, NULL_VALUE));
}

TEST_P(ProtoStructValueTest, Int64BoolMapHasField) {
  TestMapHasField(memory_manager(), "map_int64_bool",
                  &TestAllTypes::mutable_map_int64_bool,
                  std::make_pair(1, true));
}

TEST_P(ProtoStructValueTest, Int64Int32MapHasField) {
  TestMapHasField(memory_manager(), "map_int64_int32",
                  &TestAllTypes::mutable_map_int64_int32, std::make_pair(1, 1));
}

TEST_P(ProtoStructValueTest, Int64Int64MapHasField) {
  TestMapHasField(memory_manager(), "map_int64_int64",
                  &TestAllTypes::mutable_map_int64_int64, std::make_pair(1, 1));
}

TEST_P(ProtoStructValueTest, Int64Uint32MapHasField) {
  TestMapHasField(memory_manager(), "map_int64_uint32",
                  &TestAllTypes::mutable_map_int64_uint32,
                  std::make_pair(1, 1u));
}

TEST_P(ProtoStructValueTest, Int64Uint64MapHasField) {
  TestMapHasField(memory_manager(), "map_int64_uint64",
                  &TestAllTypes::mutable_map_int64_uint64,
                  std::make_pair(1, 1u));
}

TEST_P(ProtoStructValueTest, Int64FloatMapHasField) {
  TestMapHasField(memory_manager(), "map_int64_float",
                  &TestAllTypes::mutable_map_int64_float,
                  std::make_pair(1, 1.0f));
}

TEST_P(ProtoStructValueTest, Int64DoubleMapHasField) {
  TestMapHasField(memory_manager(), "map_int64_double",
                  &TestAllTypes::mutable_map_int64_double,
                  std::make_pair(1, 1.0));
}

TEST_P(ProtoStructValueTest, Int64BytesMapHasField) {
  TestMapHasField(memory_manager(), "map_int64_bytes",
                  &TestAllTypes::mutable_map_int64_bytes,
                  std::make_pair(1, "foo"));
}

TEST_P(ProtoStructValueTest, Int64StringMapHasField) {
  TestMapHasField(memory_manager(), "map_int64_string",
                  &TestAllTypes::mutable_map_int64_string,
                  std::make_pair(1, "foo"));
}

TEST_P(ProtoStructValueTest, Int64DurationMapHasField) {
  TestMapHasField(memory_manager(), "map_int64_duration",
                  &TestAllTypes::mutable_map_int64_duration,
                  std::make_pair(1, google::protobuf::Duration()));
}

TEST_P(ProtoStructValueTest, Int64TimestampMapHasField) {
  TestMapHasField(memory_manager(), "map_int64_timestamp",
                  &TestAllTypes::mutable_map_int64_timestamp,
                  std::make_pair(1, google::protobuf::Timestamp()));
}

TEST_P(ProtoStructValueTest, Int64EnumMapHasField) {
  TestMapHasField(memory_manager(), "map_int64_enum",
                  &TestAllTypes::mutable_map_int64_enum,
                  std::make_pair(1, TestAllTypes::BAR));
}

TEST_P(ProtoStructValueTest, Int64MessageMapHasField) {
  TestMapHasField(memory_manager(), "map_int64_message",
                  &TestAllTypes::mutable_map_int64_message,
                  std::make_pair(1, TestAllTypes::NestedMessage()));
}

TEST_P(ProtoStructValueTest, Uint32NullValueMapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_null_value",
                  &TestAllTypes::mutable_map_uint32_null_value,
                  std::make_pair(1u, NULL_VALUE));
}

TEST_P(ProtoStructValueTest, Uint32BoolMapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_bool",
                  &TestAllTypes::mutable_map_uint32_bool,
                  std::make_pair(1u, true));
}

TEST_P(ProtoStructValueTest, Uint32Int32MapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_int32",
                  &TestAllTypes::mutable_map_uint32_int32,
                  std::make_pair(1u, 1));
}

TEST_P(ProtoStructValueTest, Uint32Int64MapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_int64",
                  &TestAllTypes::mutable_map_uint32_int64,
                  std::make_pair(1u, 1));
}

TEST_P(ProtoStructValueTest, Uint32Uint32MapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_uint32",
                  &TestAllTypes::mutable_map_uint32_uint32,
                  std::make_pair(1u, 1u));
}

TEST_P(ProtoStructValueTest, Uint32Uint64MapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_uint64",
                  &TestAllTypes::mutable_map_uint32_uint64,
                  std::make_pair(1u, 1u));
}

TEST_P(ProtoStructValueTest, Uint32FloatMapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_float",
                  &TestAllTypes::mutable_map_uint32_float,
                  std::make_pair(1u, 1.0f));
}

TEST_P(ProtoStructValueTest, Uint32DoubleMapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_double",
                  &TestAllTypes::mutable_map_uint32_double,
                  std::make_pair(1u, 1.0));
}

TEST_P(ProtoStructValueTest, Uint32BytesMapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_bytes",
                  &TestAllTypes::mutable_map_uint32_bytes,
                  std::make_pair(1u, "foo"));
}

TEST_P(ProtoStructValueTest, Uint32StringMapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_string",
                  &TestAllTypes::mutable_map_uint32_string,
                  std::make_pair(1u, "foo"));
}

TEST_P(ProtoStructValueTest, Uint32DurationMapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_duration",
                  &TestAllTypes::mutable_map_uint32_duration,
                  std::make_pair(1u, google::protobuf::Duration()));
}

TEST_P(ProtoStructValueTest, Uint32TimestampMapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_timestamp",
                  &TestAllTypes::mutable_map_uint32_timestamp,
                  std::make_pair(1u, google::protobuf::Timestamp()));
}

TEST_P(ProtoStructValueTest, Uint32EnumMapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_enum",
                  &TestAllTypes::mutable_map_uint32_enum,
                  std::make_pair(1u, TestAllTypes::BAR));
}

TEST_P(ProtoStructValueTest, Uint32MessageMapHasField) {
  TestMapHasField(memory_manager(), "map_uint32_message",
                  &TestAllTypes::mutable_map_uint32_message,
                  std::make_pair(1u, TestAllTypes::NestedMessage()));
}

TEST_P(ProtoStructValueTest, Uint64NullValueMapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_null_value",
                  &TestAllTypes::mutable_map_uint64_null_value,
                  std::make_pair(1u, NULL_VALUE));
}

TEST_P(ProtoStructValueTest, Uint64BoolMapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_bool",
                  &TestAllTypes::mutable_map_uint64_bool,
                  std::make_pair(1u, true));
}

TEST_P(ProtoStructValueTest, Uint64Int32MapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_int32",
                  &TestAllTypes::mutable_map_uint64_int32,
                  std::make_pair(1u, 1));
}

TEST_P(ProtoStructValueTest, Uint64Int64MapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_int64",
                  &TestAllTypes::mutable_map_uint64_int64,
                  std::make_pair(1u, 1));
}

TEST_P(ProtoStructValueTest, Uint64Uint32MapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_uint32",
                  &TestAllTypes::mutable_map_uint64_uint32,
                  std::make_pair(1u, 1u));
}

TEST_P(ProtoStructValueTest, Uint64Uint64MapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_uint64",
                  &TestAllTypes::mutable_map_uint64_uint64,
                  std::make_pair(1u, 1u));
}

TEST_P(ProtoStructValueTest, Uint64FloatMapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_float",
                  &TestAllTypes::mutable_map_uint64_float,
                  std::make_pair(1u, 1.0f));
}

TEST_P(ProtoStructValueTest, Uint64DoubleMapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_double",
                  &TestAllTypes::mutable_map_uint64_double,
                  std::make_pair(1u, 1.0));
}

TEST_P(ProtoStructValueTest, Uint64BytesMapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_bytes",
                  &TestAllTypes::mutable_map_uint64_bytes,
                  std::make_pair(1u, "foo"));
}

TEST_P(ProtoStructValueTest, Uint64StringMapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_string",
                  &TestAllTypes::mutable_map_uint64_string,
                  std::make_pair(1u, "foo"));
}

TEST_P(ProtoStructValueTest, Uint64DurationMapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_duration",
                  &TestAllTypes::mutable_map_uint64_duration,
                  std::make_pair(1u, google::protobuf::Duration()));
}

TEST_P(ProtoStructValueTest, Uint64TimestampMapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_timestamp",
                  &TestAllTypes::mutable_map_uint64_timestamp,
                  std::make_pair(1u, google::protobuf::Timestamp()));
}

TEST_P(ProtoStructValueTest, Uint64EnumMapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_enum",
                  &TestAllTypes::mutable_map_uint64_enum,
                  std::make_pair(1u, TestAllTypes::BAR));
}

TEST_P(ProtoStructValueTest, Uint64MessageMapHasField) {
  TestMapHasField(memory_manager(), "map_uint64_message",
                  &TestAllTypes::mutable_map_uint64_message,
                  std::make_pair(1u, TestAllTypes::NestedMessage()));
}

TEST_P(ProtoStructValueTest, StringNullValueMapHasField) {
  TestMapHasField(memory_manager(), "map_string_null_value",
                  &TestAllTypes::mutable_map_string_null_value,
                  std::make_pair("foo", NULL_VALUE));
}

TEST_P(ProtoStructValueTest, StringBoolMapHasField) {
  TestMapHasField(memory_manager(), "map_string_bool",
                  &TestAllTypes::mutable_map_string_bool,
                  std::make_pair("foo", true));
}

TEST_P(ProtoStructValueTest, StringInt32MapHasField) {
  TestMapHasField(memory_manager(), "map_string_int32",
                  &TestAllTypes::mutable_map_string_int32,
                  std::make_pair("foo", 1));
}

TEST_P(ProtoStructValueTest, StringInt64MapHasField) {
  TestMapHasField(memory_manager(), "map_string_int64",
                  &TestAllTypes::mutable_map_string_int64,
                  std::make_pair("foo", 1));
}

TEST_P(ProtoStructValueTest, StringUint32MapHasField) {
  TestMapHasField(memory_manager(), "map_string_uint32",
                  &TestAllTypes::mutable_map_string_uint32,
                  std::make_pair("foo", 1u));
}

TEST_P(ProtoStructValueTest, StringUint64MapHasField) {
  TestMapHasField(memory_manager(), "map_string_uint64",
                  &TestAllTypes::mutable_map_string_uint64,
                  std::make_pair("foo", 1u));
}

TEST_P(ProtoStructValueTest, StringFloatMapHasField) {
  TestMapHasField(memory_manager(), "map_string_float",
                  &TestAllTypes::mutable_map_string_float,
                  std::make_pair("foo", 1.0f));
}

TEST_P(ProtoStructValueTest, StringDoubleMapHasField) {
  TestMapHasField(memory_manager(), "map_string_double",
                  &TestAllTypes::mutable_map_string_double,
                  std::make_pair("foo", 1.0));
}

TEST_P(ProtoStructValueTest, StringBytesMapHasField) {
  TestMapHasField(memory_manager(), "map_string_bytes",
                  &TestAllTypes::mutable_map_string_bytes,
                  std::make_pair("foo", "foo"));
}

TEST_P(ProtoStructValueTest, StringStringMapHasField) {
  TestMapHasField(memory_manager(), "map_string_string",
                  &TestAllTypes::mutable_map_string_string,
                  std::make_pair("foo", "foo"));
}

TEST_P(ProtoStructValueTest, StringDurationMapHasField) {
  TestMapHasField(memory_manager(), "map_string_duration",
                  &TestAllTypes::mutable_map_string_duration,
                  std::make_pair("foo", google::protobuf::Duration()));
}

TEST_P(ProtoStructValueTest, StringTimestampMapHasField) {
  TestMapHasField(memory_manager(), "map_string_timestamp",
                  &TestAllTypes::mutable_map_string_timestamp,
                  std::make_pair("foo", google::protobuf::Timestamp()));
}

TEST_P(ProtoStructValueTest, StringEnumMapHasField) {
  TestMapHasField(memory_manager(), "map_string_enum",
                  &TestAllTypes::mutable_map_string_enum,
                  std::make_pair("foo", TestAllTypes::BAR));
}

TEST_P(ProtoStructValueTest, StringMessageMapHasField) {
  TestMapHasField(memory_manager(), "map_string_message",
                  &TestAllTypes::mutable_map_string_message,
                  std::make_pair("foo", TestAllTypes::NestedMessage()));
}

TEST_P(ProtoStructValueTest, BoolNullValueMapGetField) {
  TestMapGetField<NullValue>(memory_manager(), "map_bool_null_value",
                             "{false: null, true: null}",
                             &TestAllTypes::mutable_map_bool_null_value,
                             &ValueFactory::CreateBoolValue, nullptr,
                             std::make_pair(false, NULL_VALUE),
                             std::make_pair(true, NULL_VALUE), nullptr);
}

TEST_P(ProtoStructValueTest, BoolBoolMapGetField) {
  TestMapGetField<BoolValue>(
      memory_manager(), "map_bool_bool", "{false: true, true: false}",
      &TestAllTypes::mutable_map_bool_bool, &ValueFactory::CreateBoolValue,
      &BoolValue::value, std::make_pair(false, true),
      std::make_pair(true, false), nullptr);
}

TEST_P(ProtoStructValueTest, BoolInt32MapGetField) {
  TestMapGetField<IntValue>(
      memory_manager(), "map_bool_int32", "{false: 1, true: 0}",
      &TestAllTypes::mutable_map_bool_int32, &ValueFactory::CreateBoolValue,
      &IntValue::value, std::make_pair(false, 1), std::make_pair(true, 0),
      nullptr);
}

TEST_P(ProtoStructValueTest, BoolInt64MapGetField) {
  TestMapGetField<IntValue>(
      memory_manager(), "map_bool_int64", "{false: 1, true: 0}",
      &TestAllTypes::mutable_map_bool_int64, &ValueFactory::CreateBoolValue,
      &IntValue::value, std::make_pair(false, 1), std::make_pair(true, 0),
      nullptr);
}

TEST_P(ProtoStructValueTest, BoolUint32MapGetField) {
  TestMapGetField<UintValue>(
      memory_manager(), "map_bool_uint32", "{false: 1u, true: 0u}",
      &TestAllTypes::mutable_map_bool_uint32, &ValueFactory::CreateBoolValue,
      &UintValue::value, std::make_pair(false, 1u), std::make_pair(true, 0u),
      nullptr);
}

TEST_P(ProtoStructValueTest, BoolUint64MapGetField) {
  TestMapGetField<UintValue>(
      memory_manager(), "map_bool_uint64", "{false: 1u, true: 0u}",
      &TestAllTypes::mutable_map_bool_uint64, &ValueFactory::CreateBoolValue,
      &UintValue::value, std::make_pair(false, 1u), std::make_pair(true, 0u),
      nullptr);
}

TEST_P(ProtoStructValueTest, BoolFloatMapGetField) {
  TestMapGetField<DoubleValue>(
      memory_manager(), "map_bool_float", "{false: 1.0, true: 0.0}",
      &TestAllTypes::mutable_map_bool_float, &ValueFactory::CreateBoolValue,
      &DoubleValue::value, std::make_pair(false, 1.0f),
      std::make_pair(true, 0.0f), nullptr);
}

TEST_P(ProtoStructValueTest, BoolDoubleMapGetField) {
  TestMapGetField<DoubleValue>(
      memory_manager(), "map_bool_double", "{false: 1.0, true: 0.0}",
      &TestAllTypes::mutable_map_bool_double, &ValueFactory::CreateBoolValue,
      &DoubleValue::value, std::make_pair(false, 1.0),
      std::make_pair(true, 0.0), nullptr);
}

TEST_P(ProtoStructValueTest, BoolBytesMapGetField) {
  TestMapGetField<BytesValue>(
      memory_manager(), "map_bool_bytes", "{false: b\"bar\", true: b\"foo\"}",
      &TestAllTypes::mutable_map_bool_bytes, &ValueFactory::CreateBoolValue,
      &BytesValue::ToString, std::make_pair(false, "bar"),
      std::make_pair(true, "foo"), nullptr);
}

TEST_P(ProtoStructValueTest, BoolStringMapGetField) {
  TestMapGetField<StringValue>(
      memory_manager(), "map_bool_string", "{false: \"bar\", true: \"foo\"}",
      &TestAllTypes::mutable_map_bool_string, &ValueFactory::CreateBoolValue,
      &StringValue::ToString, std::make_pair(false, "bar"),
      std::make_pair(true, "foo"), nullptr);
}

TEST_P(ProtoStructValueTest, BoolDurationMapGetField) {
  TestMapGetField<DurationValue>(
      memory_manager(), "map_bool_duration", "{false: 1s, true: 0}",
      &TestAllTypes::mutable_map_bool_duration, &ValueFactory::CreateBoolValue,
      &DurationValue::value,
      std::make_pair(false, NativeToProto(absl::Seconds(1))),
      std::make_pair(true, NativeToProto(absl::ZeroDuration())), nullptr);
}

TEST_P(ProtoStructValueTest, BoolTimestampMapGetField) {
  TestMapGetField<TimestampValue>(
      memory_manager(), "map_bool_timestamp",
      "{false: 1970-01-01T00:00:01Z, true: 1970-01-01T00:00:00Z}",
      &TestAllTypes::mutable_map_bool_timestamp, &ValueFactory::CreateBoolValue,
      &TimestampValue::value,
      std::make_pair(false,
                     NativeToProto(absl::UnixEpoch() + absl::Seconds(1))),
      std::make_pair(true,
                     NativeToProto(absl::UnixEpoch() + absl::ZeroDuration())),
      nullptr);
}

TEST_P(ProtoStructValueTest, BoolEnumMapGetField) {
  TestMapGetField<EnumValue>(
      memory_manager(), "map_bool_enum",
      "{false: google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAR, "
      "true: google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.FOO}",
      &TestAllTypes::mutable_map_bool_enum, &ValueFactory::CreateBoolValue,
      &EnumValue::number, std::make_pair(false, TestAllTypes::BAR),
      std::make_pair(true, TestAllTypes::FOO), nullptr);
}

TEST_P(ProtoStructValueTest, BoolMessageMapGetField) {
  TestMapGetField<ProtoStructValue>(
      memory_manager(), "map_bool_message",
      "{false: google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: "
      "1}, "
      "true: google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: 2}}",
      &TestAllTypes::mutable_map_bool_message, &ValueFactory::CreateBoolValue,
      nullptr, std::make_pair(false, CreateTestNestedMessage(1)),
      std::make_pair(true, CreateTestNestedMessage(2)), nullptr);
}

TEST_P(ProtoStructValueTest, Int32NullValueMapGetField) {
  TestMapGetField<NullValue>(
      memory_manager(), "map_int32_null_value", "{0: null, 1: null}",
      &TestAllTypes::mutable_map_int32_null_value,
      &ValueFactory::CreateIntValue, nullptr, std::make_pair(0, NULL_VALUE),
      std::make_pair(1, NULL_VALUE), 2);
}

TEST_P(ProtoStructValueTest, Int32BoolMapGetField) {
  TestMapGetField<BoolValue>(
      memory_manager(), "map_int32_bool", "{0: true, 1: false}",
      &TestAllTypes::mutable_map_int32_bool, &ValueFactory::CreateIntValue,
      &BoolValue::value, std::make_pair(0, true), std::make_pair(1, false), 2);
}

TEST_P(ProtoStructValueTest, Int32Int32MapGetField) {
  TestMapGetField<IntValue>(memory_manager(), "map_int32_int32", "{0: 1, 1: 0}",
                            &TestAllTypes::mutable_map_int32_int32,
                            &ValueFactory::CreateIntValue, &IntValue::value,
                            std::make_pair(0, 1), std::make_pair(1, 0), 2);
}

TEST_P(ProtoStructValueTest, Int32Int64MapGetField) {
  TestMapGetField<IntValue>(memory_manager(), "map_int32_int64", "{0: 1, 1: 0}",
                            &TestAllTypes::mutable_map_int32_int64,
                            &ValueFactory::CreateIntValue, &IntValue::value,
                            std::make_pair(0, 1), std::make_pair(1, 0), 2);
}

TEST_P(ProtoStructValueTest, Int32Uint32MapGetField) {
  TestMapGetField<UintValue>(
      memory_manager(), "map_int32_uint32", "{0: 1u, 1: 0u}",
      &TestAllTypes::mutable_map_int32_uint32, &ValueFactory::CreateIntValue,
      &UintValue::value, std::make_pair(0, 1u), std::make_pair(1, 0u), 2);
}

TEST_P(ProtoStructValueTest, Int32Uint64MapGetField) {
  TestMapGetField<UintValue>(
      memory_manager(), "map_int32_uint64", "{0: 1u, 1: 0u}",
      &TestAllTypes::mutable_map_int32_uint64, &ValueFactory::CreateIntValue,
      &UintValue::value, std::make_pair(0, 1u), std::make_pair(1, 0u), 2);
}

TEST_P(ProtoStructValueTest, Int32FloatMapGetField) {
  TestMapGetField<DoubleValue>(
      memory_manager(), "map_int32_float", "{0: 1.0, 1: 0.0}",
      &TestAllTypes::mutable_map_int32_float, &ValueFactory::CreateIntValue,
      &DoubleValue::value, std::make_pair(0, 1.0f), std::make_pair(1, 0.0f), 2);
}

TEST_P(ProtoStructValueTest, Int32DoubleMapGetField) {
  TestMapGetField<DoubleValue>(
      memory_manager(), "map_int32_double", "{0: 1.0, 1: 0.0}",
      &TestAllTypes::mutable_map_int32_double, &ValueFactory::CreateIntValue,
      &DoubleValue::value, std::make_pair(0, 1.0), std::make_pair(1, 0.0), 2);
}

TEST_P(ProtoStructValueTest, Int32BytesMapGetField) {
  TestMapGetField<BytesValue>(
      memory_manager(), "map_int32_bytes", "{0: b\"bar\", 1: b\"foo\"}",
      &TestAllTypes::mutable_map_int32_bytes, &ValueFactory::CreateIntValue,
      &BytesValue::ToString, std::make_pair(0, "bar"), std::make_pair(1, "foo"),
      2);
}

TEST_P(ProtoStructValueTest, Int32StringMapGetField) {
  TestMapGetField<StringValue>(
      memory_manager(), "map_int32_string", "{0: \"bar\", 1: \"foo\"}",
      &TestAllTypes::mutable_map_int32_string, &ValueFactory::CreateIntValue,
      &StringValue::ToString, std::make_pair(0, "bar"),
      std::make_pair(1, "foo"), 2);
}

TEST_P(ProtoStructValueTest, Int32DurationMapGetField) {
  TestMapGetField<DurationValue>(
      memory_manager(), "map_int32_duration", "{0: 1s, 1: 0}",
      &TestAllTypes::mutable_map_int32_duration, &ValueFactory::CreateIntValue,
      &DurationValue::value, std::make_pair(0, NativeToProto(absl::Seconds(1))),
      std::make_pair(1, NativeToProto(absl::ZeroDuration())), 2);
}

TEST_P(ProtoStructValueTest, Int32TimestampMapGetField) {
  TestMapGetField<TimestampValue>(
      memory_manager(), "map_int32_timestamp",
      "{0: 1970-01-01T00:00:01Z, 1: 1970-01-01T00:00:00Z}",
      &TestAllTypes::mutable_map_int32_timestamp, &ValueFactory::CreateIntValue,
      &TimestampValue::value,
      std::make_pair(0, NativeToProto(absl::UnixEpoch() + absl::Seconds(1))),
      std::make_pair(1,
                     NativeToProto(absl::UnixEpoch() + absl::ZeroDuration())),
      2);
}

TEST_P(ProtoStructValueTest, Int32EnumMapGetField) {
  TestMapGetField<EnumValue>(
      memory_manager(), "map_int32_enum",
      "{0: google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAR, "
      "1: google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.FOO}",
      &TestAllTypes::mutable_map_int32_enum, &ValueFactory::CreateIntValue,
      &EnumValue::number, std::make_pair(0, TestAllTypes::BAR),
      std::make_pair(1, TestAllTypes::FOO), 2);
}

TEST_P(ProtoStructValueTest, Int32MessageMapGetField) {
  TestMapGetField<ProtoStructValue>(
      memory_manager(), "map_int32_message",
      "{0: google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: "
      "1}, "
      "1: google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: 2}}",
      &TestAllTypes::mutable_map_int32_message, &ValueFactory::CreateIntValue,
      nullptr, std::make_pair(0, CreateTestNestedMessage(1)),
      std::make_pair(1, CreateTestNestedMessage(2)), 2);
}

TEST_P(ProtoStructValueTest, Int64NullValueMapGetField) {
  TestMapGetField<NullValue>(
      memory_manager(), "map_int64_null_value", "{0: null, 1: null}",
      &TestAllTypes::mutable_map_int64_null_value,
      &ValueFactory::CreateIntValue, nullptr, std::make_pair(0, NULL_VALUE),
      std::make_pair(1, NULL_VALUE), 2);
}

TEST_P(ProtoStructValueTest, Int64BoolMapGetField) {
  TestMapGetField<BoolValue>(
      memory_manager(), "map_int64_bool", "{0: true, 1: false}",
      &TestAllTypes::mutable_map_int64_bool, &ValueFactory::CreateIntValue,
      &BoolValue::value, std::make_pair(0, true), std::make_pair(1, false), 2);
}

TEST_P(ProtoStructValueTest, Int64Int32MapGetField) {
  TestMapGetField<IntValue>(memory_manager(), "map_int64_int32", "{0: 1, 1: 0}",
                            &TestAllTypes::mutable_map_int64_int32,
                            &ValueFactory::CreateIntValue, &IntValue::value,
                            std::make_pair(0, 1), std::make_pair(1, 0), 2);
}

TEST_P(ProtoStructValueTest, Int64Int64MapGetField) {
  TestMapGetField<IntValue>(memory_manager(), "map_int64_int64", "{0: 1, 1: 0}",
                            &TestAllTypes::mutable_map_int64_int64,
                            &ValueFactory::CreateIntValue, &IntValue::value,
                            std::make_pair(0, 1), std::make_pair(1, 0), 2);
}

TEST_P(ProtoStructValueTest, Int64Uint32MapGetField) {
  TestMapGetField<UintValue>(
      memory_manager(), "map_int64_uint32", "{0: 1u, 1: 0u}",
      &TestAllTypes::mutable_map_int64_uint32, &ValueFactory::CreateIntValue,
      &UintValue::value, std::make_pair(0, 1u), std::make_pair(1, 0u), 2);
}

TEST_P(ProtoStructValueTest, Int64Uint64MapGetField) {
  TestMapGetField<UintValue>(
      memory_manager(), "map_int64_uint64", "{0: 1u, 1: 0u}",
      &TestAllTypes::mutable_map_int64_uint64, &ValueFactory::CreateIntValue,
      &UintValue::value, std::make_pair(0, 1u), std::make_pair(1, 0u), 2);
}

TEST_P(ProtoStructValueTest, Int64FloatMapGetField) {
  TestMapGetField<DoubleValue>(
      memory_manager(), "map_int64_float", "{0: 1.0, 1: 0.0}",
      &TestAllTypes::mutable_map_int64_float, &ValueFactory::CreateIntValue,
      &DoubleValue::value, std::make_pair(0, 1.0f), std::make_pair(1, 0.0f), 2);
}

TEST_P(ProtoStructValueTest, Int64DoubleMapGetField) {
  TestMapGetField<DoubleValue>(
      memory_manager(), "map_int64_double", "{0: 1.0, 1: 0.0}",
      &TestAllTypes::mutable_map_int64_double, &ValueFactory::CreateIntValue,
      &DoubleValue::value, std::make_pair(0, 1.0), std::make_pair(1, 0.0), 2);
}

TEST_P(ProtoStructValueTest, Int64BytesMapGetField) {
  TestMapGetField<BytesValue>(
      memory_manager(), "map_int64_bytes", "{0: b\"bar\", 1: b\"foo\"}",
      &TestAllTypes::mutable_map_int64_bytes, &ValueFactory::CreateIntValue,
      &BytesValue::ToString, std::make_pair(0, "bar"), std::make_pair(1, "foo"),
      2);
}

TEST_P(ProtoStructValueTest, Int64StringMapGetField) {
  TestMapGetField<StringValue>(
      memory_manager(), "map_int64_string", "{0: \"bar\", 1: \"foo\"}",
      &TestAllTypes::mutable_map_int64_string, &ValueFactory::CreateIntValue,
      &StringValue::ToString, std::make_pair(0, "bar"),
      std::make_pair(1, "foo"), 2);
}

TEST_P(ProtoStructValueTest, Int64DurationMapGetField) {
  TestMapGetField<DurationValue>(
      memory_manager(), "map_int64_duration", "{0: 1s, 1: 0}",
      &TestAllTypes::mutable_map_int64_duration, &ValueFactory::CreateIntValue,
      &DurationValue::value, std::make_pair(0, NativeToProto(absl::Seconds(1))),
      std::make_pair(1, NativeToProto(absl::ZeroDuration())), 2);
}

TEST_P(ProtoStructValueTest, Int64TimestampMapGetField) {
  TestMapGetField<TimestampValue>(
      memory_manager(), "map_int64_timestamp",
      "{0: 1970-01-01T00:00:01Z, 1: 1970-01-01T00:00:00Z}",
      &TestAllTypes::mutable_map_int64_timestamp, &ValueFactory::CreateIntValue,
      &TimestampValue::value,
      std::make_pair(0, NativeToProto(absl::UnixEpoch() + absl::Seconds(1))),
      std::make_pair(1,
                     NativeToProto(absl::UnixEpoch() + absl::ZeroDuration())),
      2);
}

TEST_P(ProtoStructValueTest, Int64EnumMapGetField) {
  TestMapGetField<EnumValue>(
      memory_manager(), "map_int64_enum",
      "{0: google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAR, "
      "1: google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.FOO}",
      &TestAllTypes::mutable_map_int64_enum, &ValueFactory::CreateIntValue,
      &EnumValue::number, std::make_pair(0, TestAllTypes::BAR),
      std::make_pair(1, TestAllTypes::FOO), 2);
}

TEST_P(ProtoStructValueTest, Int64MessageMapGetField) {
  TestMapGetField<ProtoStructValue>(
      memory_manager(), "map_int64_message",
      "{0: google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: "
      "1}, "
      "1: google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: 2}}",
      &TestAllTypes::mutable_map_int64_message, &ValueFactory::CreateIntValue,
      nullptr, std::make_pair(0, CreateTestNestedMessage(1)),
      std::make_pair(1, CreateTestNestedMessage(2)), 2);
}

TEST_P(ProtoStructValueTest, Uint32NullValueMapGetField) {
  TestMapGetField<NullValue>(
      memory_manager(), "map_uint32_null_value", "{0u: null, 1u: null}",
      &TestAllTypes::mutable_map_uint32_null_value,
      &ValueFactory::CreateUintValue, nullptr, std::make_pair(0u, NULL_VALUE),
      std::make_pair(1u, NULL_VALUE), 2u);
}

TEST_P(ProtoStructValueTest, Uint32BoolMapGetField) {
  TestMapGetField<BoolValue>(
      memory_manager(), "map_uint32_bool", "{0u: true, 1u: false}",
      &TestAllTypes::mutable_map_uint32_bool, &ValueFactory::CreateUintValue,
      &BoolValue::value, std::make_pair(0u, true), std::make_pair(1u, false),
      2u);
}

TEST_P(ProtoStructValueTest, Uint32Int32MapGetField) {
  TestMapGetField<IntValue>(
      memory_manager(), "map_uint32_int32", "{0u: 1, 1u: 0}",
      &TestAllTypes::mutable_map_uint32_int32, &ValueFactory::CreateUintValue,
      &IntValue::value, std::make_pair(0u, 1), std::make_pair(1u, 0), 2u);
}

TEST_P(ProtoStructValueTest, Uint32Int64MapGetField) {
  TestMapGetField<IntValue>(
      memory_manager(), "map_uint32_int64", "{0u: 1, 1u: 0}",
      &TestAllTypes::mutable_map_uint32_int64, &ValueFactory::CreateUintValue,
      &IntValue::value, std::make_pair(0u, 1), std::make_pair(1u, 0), 2u);
}

TEST_P(ProtoStructValueTest, Uint32Uint32MapGetField) {
  TestMapGetField<UintValue>(
      memory_manager(), "map_uint32_uint32", "{0u: 1u, 1u: 0u}",
      &TestAllTypes::mutable_map_uint32_uint32, &ValueFactory::CreateUintValue,
      &UintValue::value, std::make_pair(0u, 1u), std::make_pair(1u, 0u), 2u);
}

TEST_P(ProtoStructValueTest, Uint32Uint64MapGetField) {
  TestMapGetField<UintValue>(
      memory_manager(), "map_uint32_uint64", "{0u: 1u, 1u: 0u}",
      &TestAllTypes::mutable_map_uint32_uint64, &ValueFactory::CreateUintValue,
      &UintValue::value, std::make_pair(0u, 1u), std::make_pair(1u, 0u), 2u);
}

TEST_P(ProtoStructValueTest, Uint32FloatMapGetField) {
  TestMapGetField<DoubleValue>(
      memory_manager(), "map_uint32_float", "{0u: 1.0, 1u: 0.0}",
      &TestAllTypes::mutable_map_uint32_float, &ValueFactory::CreateUintValue,
      &DoubleValue::value, std::make_pair(0u, 1.0f), std::make_pair(1u, 0.0f),
      2u);
}

TEST_P(ProtoStructValueTest, Uint32DoubleMapGetField) {
  TestMapGetField<DoubleValue>(
      memory_manager(), "map_uint32_double", "{0u: 1.0, 1u: 0.0}",
      &TestAllTypes::mutable_map_uint32_double, &ValueFactory::CreateUintValue,
      &DoubleValue::value, std::make_pair(0u, 1.0), std::make_pair(1u, 0.0),
      2u);
}

TEST_P(ProtoStructValueTest, Uint32BytesMapGetField) {
  TestMapGetField<BytesValue>(
      memory_manager(), "map_uint32_bytes", "{0u: b\"bar\", 1u: b\"foo\"}",
      &TestAllTypes::mutable_map_uint32_bytes, &ValueFactory::CreateUintValue,
      &BytesValue::ToString, std::make_pair(0u, "bar"),
      std::make_pair(1u, "foo"), 2u);
}

TEST_P(ProtoStructValueTest, Uint32StringMapGetField) {
  TestMapGetField<StringValue>(
      memory_manager(), "map_uint32_string", "{0u: \"bar\", 1u: \"foo\"}",
      &TestAllTypes::mutable_map_uint32_string, &ValueFactory::CreateUintValue,
      &StringValue::ToString, std::make_pair(0u, "bar"),
      std::make_pair(1u, "foo"), 2u);
}

TEST_P(ProtoStructValueTest, Uint32DurationMapGetField) {
  TestMapGetField<DurationValue>(
      memory_manager(), "map_uint32_duration", "{0u: 1s, 1u: 0}",
      &TestAllTypes::mutable_map_uint32_duration,
      &ValueFactory::CreateUintValue, &DurationValue::value,
      std::make_pair(0u, NativeToProto(absl::Seconds(1))),
      std::make_pair(1u, NativeToProto(absl::ZeroDuration())), 2u);
}

TEST_P(ProtoStructValueTest, Uint32TimestampMapGetField) {
  TestMapGetField<TimestampValue>(
      memory_manager(), "map_uint32_timestamp",
      "{0u: 1970-01-01T00:00:01Z, 1u: 1970-01-01T00:00:00Z}",
      &TestAllTypes::mutable_map_uint32_timestamp,
      &ValueFactory::CreateUintValue, &TimestampValue::value,
      std::make_pair(0u, NativeToProto(absl::UnixEpoch() + absl::Seconds(1))),
      std::make_pair(1u,
                     NativeToProto(absl::UnixEpoch() + absl::ZeroDuration())),
      2u);
}

TEST_P(ProtoStructValueTest, Uint32EnumMapGetField) {
  TestMapGetField<EnumValue>(
      memory_manager(), "map_uint32_enum",
      "{0u: google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAR, "
      "1u: google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.FOO}",
      &TestAllTypes::mutable_map_uint32_enum, &ValueFactory::CreateUintValue,
      &EnumValue::number, std::make_pair(0u, TestAllTypes::BAR),
      std::make_pair(1u, TestAllTypes::FOO), 2u);
}

TEST_P(ProtoStructValueTest, Uint32MessageMapGetField) {
  TestMapGetField<ProtoStructValue>(
      memory_manager(), "map_uint32_message",
      "{0u: google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: "
      "1}, "
      "1u: google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: 2}}",
      &TestAllTypes::mutable_map_uint32_message, &ValueFactory::CreateUintValue,
      nullptr, std::make_pair(0u, CreateTestNestedMessage(1)),
      std::make_pair(1u, CreateTestNestedMessage(2)), 2u);
}

TEST_P(ProtoStructValueTest, Uint64NullValueMapGetField) {
  TestMapGetField<NullValue>(
      memory_manager(), "map_uint64_null_value", "{0u: null, 1u: null}",
      &TestAllTypes::mutable_map_uint64_null_value,
      &ValueFactory::CreateUintValue, nullptr, std::make_pair(0u, NULL_VALUE),
      std::make_pair(1u, NULL_VALUE), 2u);
}

TEST_P(ProtoStructValueTest, Uint64BoolMapGetField) {
  TestMapGetField<BoolValue>(
      memory_manager(), "map_uint64_bool", "{0u: true, 1u: false}",
      &TestAllTypes::mutable_map_uint64_bool, &ValueFactory::CreateUintValue,
      &BoolValue::value, std::make_pair(0u, true), std::make_pair(1u, false),
      2u);
}

TEST_P(ProtoStructValueTest, Uint64Int32MapGetField) {
  TestMapGetField<IntValue>(
      memory_manager(), "map_uint64_int32", "{0u: 1, 1u: 0}",
      &TestAllTypes::mutable_map_uint64_int32, &ValueFactory::CreateUintValue,
      &IntValue::value, std::make_pair(0u, 1), std::make_pair(1u, 0), 2u);
}

TEST_P(ProtoStructValueTest, Uint64Int64MapGetField) {
  TestMapGetField<IntValue>(
      memory_manager(), "map_uint64_int64", "{0u: 1, 1u: 0}",
      &TestAllTypes::mutable_map_uint64_int64, &ValueFactory::CreateUintValue,
      &IntValue::value, std::make_pair(0u, 1), std::make_pair(1u, 0), 2u);
}

TEST_P(ProtoStructValueTest, Uint64Uint32MapGetField) {
  TestMapGetField<UintValue>(
      memory_manager(), "map_uint64_uint32", "{0u: 1u, 1u: 0u}",
      &TestAllTypes::mutable_map_uint64_uint32, &ValueFactory::CreateUintValue,
      &UintValue::value, std::make_pair(0u, 1u), std::make_pair(1u, 0u), 2u);
}

TEST_P(ProtoStructValueTest, Uint64Uint64MapGetField) {
  TestMapGetField<UintValue>(
      memory_manager(), "map_uint64_uint64", "{0u: 1u, 1u: 0u}",
      &TestAllTypes::mutable_map_uint64_uint64, &ValueFactory::CreateUintValue,
      &UintValue::value, std::make_pair(0u, 1u), std::make_pair(1u, 0u), 2u);
}

TEST_P(ProtoStructValueTest, Uint64FloatMapGetField) {
  TestMapGetField<DoubleValue>(
      memory_manager(), "map_uint64_float", "{0u: 1.0, 1u: 0.0}",
      &TestAllTypes::mutable_map_uint64_float, &ValueFactory::CreateUintValue,
      &DoubleValue::value, std::make_pair(0u, 1.0f), std::make_pair(1u, 0.0f),
      2u);
}

TEST_P(ProtoStructValueTest, Uint64DoubleMapGetField) {
  TestMapGetField<DoubleValue>(
      memory_manager(), "map_uint64_double", "{0u: 1.0, 1u: 0.0}",
      &TestAllTypes::mutable_map_uint64_double, &ValueFactory::CreateUintValue,
      &DoubleValue::value, std::make_pair(0u, 1.0), std::make_pair(1u, 0.0),
      2u);
}

TEST_P(ProtoStructValueTest, Uint64BytesMapGetField) {
  TestMapGetField<BytesValue>(
      memory_manager(), "map_uint64_bytes", "{0u: b\"bar\", 1u: b\"foo\"}",
      &TestAllTypes::mutable_map_uint64_bytes, &ValueFactory::CreateUintValue,
      &BytesValue::ToString, std::make_pair(0u, "bar"),
      std::make_pair(1u, "foo"), 2u);
}

TEST_P(ProtoStructValueTest, Uint64StringMapGetField) {
  TestMapGetField<StringValue>(
      memory_manager(), "map_uint64_string", "{0u: \"bar\", 1u: \"foo\"}",
      &TestAllTypes::mutable_map_uint64_string, &ValueFactory::CreateUintValue,
      &StringValue::ToString, std::make_pair(0u, "bar"),
      std::make_pair(1u, "foo"), 2u);
}

TEST_P(ProtoStructValueTest, Uint64DurationMapGetField) {
  TestMapGetField<DurationValue>(
      memory_manager(), "map_uint64_duration", "{0u: 1s, 1u: 0}",
      &TestAllTypes::mutable_map_uint64_duration,
      &ValueFactory::CreateUintValue, &DurationValue::value,
      std::make_pair(0u, NativeToProto(absl::Seconds(1))),
      std::make_pair(1u, NativeToProto(absl::ZeroDuration())), 2u);
}

TEST_P(ProtoStructValueTest, Uint64TimestampMapGetField) {
  TestMapGetField<TimestampValue>(
      memory_manager(), "map_uint64_timestamp",
      "{0u: 1970-01-01T00:00:01Z, 1u: 1970-01-01T00:00:00Z}",
      &TestAllTypes::mutable_map_uint64_timestamp,
      &ValueFactory::CreateUintValue, &TimestampValue::value,
      std::make_pair(0u, NativeToProto(absl::UnixEpoch() + absl::Seconds(1))),
      std::make_pair(1u,
                     NativeToProto(absl::UnixEpoch() + absl::ZeroDuration())),
      2u);
}

TEST_P(ProtoStructValueTest, Uint64EnumMapGetField) {
  TestMapGetField<EnumValue>(
      memory_manager(), "map_uint64_enum",
      "{0u: google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAR, "
      "1u: google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.FOO}",
      &TestAllTypes::mutable_map_uint64_enum, &ValueFactory::CreateUintValue,
      &EnumValue::number, std::make_pair(0u, TestAllTypes::BAR),
      std::make_pair(1u, TestAllTypes::FOO), 2u);
}

TEST_P(ProtoStructValueTest, Uint64MessageMapGetField) {
  TestMapGetField<ProtoStructValue>(
      memory_manager(), "map_uint64_message",
      "{0u: google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: "
      "1}, "
      "1u: google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: 2}}",
      &TestAllTypes::mutable_map_uint64_message, &ValueFactory::CreateUintValue,
      nullptr, std::make_pair(0u, CreateTestNestedMessage(1)),
      std::make_pair(1u, CreateTestNestedMessage(2)), 2u);
}

TEST_P(ProtoStructValueTest, StringNullValueMapGetField) {
  TestStringMapGetField<NullValue>(memory_manager(), "map_string_null_value",
                                   "{\"bar\": null, \"baz\": null}",
                                   &TestAllTypes::mutable_map_string_null_value,
                                   nullptr, std::make_pair("bar", NULL_VALUE),
                                   std::make_pair("baz", NULL_VALUE), "foo");
}

TEST_P(ProtoStructValueTest, StringBoolMapGetField) {
  TestStringMapGetField<BoolValue>(
      memory_manager(), "map_string_bool", "{\"bar\": true, \"baz\": false}",
      &TestAllTypes::mutable_map_string_bool, &BoolValue::value,
      std::make_pair("bar", true), std::make_pair("baz", false), "foo");
}

TEST_P(ProtoStructValueTest, StringInt32MapGetField) {
  TestStringMapGetField<IntValue>(
      memory_manager(), "map_string_int32", "{\"bar\": 1, \"baz\": 0}",
      &TestAllTypes::mutable_map_string_int32, &IntValue::value,
      std::make_pair("bar", 1), std::make_pair("baz", 0), "foo");
}

TEST_P(ProtoStructValueTest, StringInt64MapGetField) {
  TestStringMapGetField<IntValue>(
      memory_manager(), "map_string_int64", "{\"bar\": 1, \"baz\": 0}",
      &TestAllTypes::mutable_map_string_int64, &IntValue::value,
      std::make_pair("bar", 1), std::make_pair("baz", 0), "foo");
}

TEST_P(ProtoStructValueTest, StringUint32MapGetField) {
  TestStringMapGetField<UintValue>(
      memory_manager(), "map_string_uint32", "{\"bar\": 1u, \"baz\": 0u}",
      &TestAllTypes::mutable_map_string_uint32, &UintValue::value,
      std::make_pair("bar", 1u), std::make_pair("baz", 0u), "foo");
}

TEST_P(ProtoStructValueTest, StringUint64MapGetField) {
  TestStringMapGetField<UintValue>(
      memory_manager(), "map_string_uint64", "{\"bar\": 1u, \"baz\": 0u}",
      &TestAllTypes::mutable_map_string_uint64, &UintValue::value,
      std::make_pair("bar", 1u), std::make_pair("baz", 0u), "foo");
}

TEST_P(ProtoStructValueTest, StringFloatMapGetField) {
  TestStringMapGetField<DoubleValue>(
      memory_manager(), "map_string_float", "{\"bar\": 1.0, \"baz\": 0.0}",
      &TestAllTypes::mutable_map_string_float, &DoubleValue::value,
      std::make_pair("bar", 1.0f), std::make_pair("baz", 0.0f), "foo");
}

TEST_P(ProtoStructValueTest, StringDoubleMapGetField) {
  TestStringMapGetField<DoubleValue>(
      memory_manager(), "map_string_double", "{\"bar\": 1.0, \"baz\": 0.0}",
      &TestAllTypes::mutable_map_string_double, &DoubleValue::value,
      std::make_pair("bar", 1.0), std::make_pair("baz", 0.0), "foo");
}

TEST_P(ProtoStructValueTest, StringBytesMapGetField) {
  TestStringMapGetField<BytesValue>(
      memory_manager(), "map_string_bytes",
      "{\"bar\": b\"baz\", \"baz\": b\"bar\"}",
      &TestAllTypes::mutable_map_string_bytes, &BytesValue::ToString,
      std::make_pair("bar", "baz"), std::make_pair("baz", "bar"), "foo");
}

TEST_P(ProtoStructValueTest, StringStringMapGetField) {
  TestStringMapGetField<StringValue>(
      memory_manager(), "map_string_string",
      "{\"bar\": \"baz\", \"baz\": \"bar\"}",
      &TestAllTypes::mutable_map_string_string, &StringValue::ToString,
      std::make_pair("bar", "baz"), std::make_pair("baz", "bar"), "foo");
}

TEST_P(ProtoStructValueTest, StringDurationMapGetField) {
  TestStringMapGetField<DurationValue>(
      memory_manager(), "map_string_duration", "{\"bar\": 1s, \"baz\": 0}",
      &TestAllTypes::mutable_map_string_duration, &DurationValue::value,
      std::make_pair("bar", NativeToProto(absl::Seconds(1))),
      std::make_pair("baz", NativeToProto(absl::ZeroDuration())), "foo");
}

TEST_P(ProtoStructValueTest, StringTimestampMapGetField) {
  TestStringMapGetField<TimestampValue>(
      memory_manager(), "map_string_timestamp",
      "{\"bar\": 1970-01-01T00:00:01Z, \"baz\": 1970-01-01T00:00:00Z}",
      &TestAllTypes::mutable_map_string_timestamp, &TimestampValue::value,
      std::make_pair("bar",
                     NativeToProto(absl::UnixEpoch() + absl::Seconds(1))),
      std::make_pair("baz",
                     NativeToProto(absl::UnixEpoch() + absl::ZeroDuration())),
      "foo");
}

TEST_P(ProtoStructValueTest, StringEnumMapGetField) {
  TestStringMapGetField<EnumValue>(
      memory_manager(), "map_string_enum",
      "{\"bar\": google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.FOO, "
      "\"baz\": google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAR}",
      &TestAllTypes::mutable_map_string_enum, &EnumValue::number,
      std::make_pair("bar", TestAllTypes::FOO),
      std::make_pair("baz", TestAllTypes::BAR), "foo");
}

TEST_P(ProtoStructValueTest, StringMessageMapGetField) {
  TestStringMapGetField<ProtoStructValue>(
      memory_manager(), "map_string_message",
      "{\"bar\": google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: "
      "1}, "
      "\"baz\": google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: "
      "2}}",
      &TestAllTypes::mutable_map_string_message, nullptr,
      std::make_pair("bar", CreateTestNestedMessage(1)),
      std::make_pair("baz", CreateTestNestedMessage(2)), "foo");
}

TEST_P(ProtoStructValueTest, DebugString) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoValue::Create(value_factory,
                         CreateTestMessage([](TestAllTypes& message) {
                           message.set_single_bool(true);
                           message.set_single_int32(1);
                           message.set_single_int64(1);
                           message.set_single_uint32(1);
                           message.set_single_uint64(1);
                           message.set_single_float(1.0);
                           message.set_single_double(1.0);
                           message.set_single_bytes("foo");
                           message.set_single_string("foo");
                           message.set_standalone_enum(TestAllTypes::BAR);
                           message.mutable_standalone_message()->set_bb(1);
                           message.mutable_single_duration()->set_seconds(1);
                           message.mutable_single_timestamp()->set_seconds(1);
                         })));
  EXPECT_EQ(
      value->DebugString(),
      "google.api.expr.test.v1.proto3.TestAllTypes{"
      "single_int32: 1, single_int64: 1, single_uint32: 1u, single_uint64: 1u, "
      "single_float: 1.0, single_double: 1.0, single_bool: true, "
      "single_string: "
      "\"foo\", single_bytes: b\"foo\", "
      "standalone_message: "
      "google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: 1}, "
      "standalone_enum: "
      "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAR, "
      "single_duration: 1s, single_timestamp: 1970-01-01T00:00:01Z}");
}

TEST_P(ProtoStructValueTest, ListDebugString) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoValue::Create(value_factory,
                         CreateTestMessage([](TestAllTypes& message) {
                           message.add_repeated_bool(true);
                           message.add_repeated_bool(false);
                           message.add_repeated_int32(1);
                           message.add_repeated_int32(0);
                           message.add_repeated_int64(1);
                           message.add_repeated_int64(0);
                           message.add_repeated_uint32(1);
                           message.add_repeated_uint32(0);
                           message.add_repeated_uint64(1);
                           message.add_repeated_uint64(0);
                           message.add_repeated_float(1.0);
                           message.add_repeated_float(0.0);
                           message.add_repeated_double(1.0);
                           message.add_repeated_double(0.0);
                           message.add_repeated_bytes("foo");
                           message.add_repeated_bytes("bar");
                           message.add_repeated_string("foo");
                           message.add_repeated_string("bar");
                           message.add_repeated_nested_enum(TestAllTypes::FOO);
                           message.add_repeated_nested_enum(TestAllTypes::BAR);
                           message.add_repeated_nested_message()->set_bb(1);
                           message.add_repeated_nested_message()->set_bb(2);
                           message.add_repeated_duration()->set_seconds(1);
                           message.add_repeated_duration()->set_seconds(2);
                           message.add_repeated_timestamp()->set_seconds(1);
                           message.add_repeated_timestamp()->set_seconds(2);
                         })));
  EXPECT_EQ(
      value->DebugString(),
      "google.api.expr.test.v1.proto3.TestAllTypes{repeated_int32: [1, 0], "
      "repeated_int64: [1, 0], repeated_uint32: [1u, 0u], repeated_uint64: "
      "[1u, 0u], repeated_float: [1.0, 0.0], repeated_double: [1.0, 0.0], "
      "repeated_bool: [true, false], "
      "repeated_string: [\"foo\", \"bar\"], repeated_bytes: [b\"foo\", "
      "b\"bar\"], repeated_nested_message: "
      "[google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: 1}, "
      "google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: 2}], "
      "repeated_nested_enum: "
      "[google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.FOO, "
      "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAR], repeated_"
      "duration: [1s, 2s], repeated_timestamp: [1970-01-01T00:00:01Z, "
      "1970-01-01T00:00:02Z]}");
}

TEST_P(ProtoStructValueTest, StaticValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  TestAllTypes message = CreateTestMessage();
  ASSERT_OK_AND_ASSIGN(auto value, ProtoValue::Create(value_factory, message));
  EXPECT_TRUE(value->Is<ProtoStructValue>());
  TestAllTypes scratch;
  EXPECT_THAT(*value->value(scratch), EqualsProto(message));
}

TEST_P(ProtoStructValueTest, DynamicLValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  TestAllTypes message = CreateTestMessage();
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoValue::Create(value_factory,
                         static_cast<const google::protobuf::Message&>(message)));
  EXPECT_TRUE(value->Is<ProtoStructValue>());
  TestAllTypes scratch;
  EXPECT_THAT(*value.As<ProtoStructValue>()->value(scratch),
              EqualsProto(message));
}

TEST_P(ProtoStructValueTest, DynamicRValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoValue::Create(value_factory,
                         static_cast<google::protobuf::Message&&>(CreateTestMessage())));
  EXPECT_TRUE(value->Is<ProtoStructValue>());
}

void BuildDescriptorDatabase(google::protobuf::SimpleDescriptorDatabase* database) {
  google::protobuf::FileDescriptorProto proto;
  TestAllTypes::descriptor()->file()->CopyTo(&proto);
  ASSERT_TRUE(database->Add(proto));
  for (int index = 0;
       index < TestAllTypes::descriptor()->file()->dependency_count();
       index++) {
    proto.Clear();
    TestAllTypes::descriptor()->file()->dependency(index)->CopyTo(&proto);
    ASSERT_TRUE(database->Add(proto));
  }
}

TEST_P(ProtoStructValueTest, DynamicLValueDifferentDescriptors) {
  TypeFactory type_factory(memory_manager());
  google::protobuf::SimpleDescriptorDatabase database;
  BuildDescriptorDatabase(&database);
  google::protobuf::DescriptorPool pool(&database);
  google::protobuf::DynamicMessageFactory factory(&pool);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  const auto* desc =
      pool.FindMessageTypeByName(TestAllTypes::descriptor()->full_name());
  ASSERT_TRUE(desc != nullptr);
  const auto* prototype = factory.GetPrototype(desc);
  ASSERT_TRUE(prototype != nullptr);
  ASSERT_OK_AND_ASSIGN(auto value,
                       ProtoValue::Create(value_factory, *prototype));
  EXPECT_TRUE(value->Is<ProtoStructValue>());
}

TEST_P(ProtoStructValueTest, DynamicRValueDifferentDescriptors) {
  TypeFactory type_factory(memory_manager());
  google::protobuf::SimpleDescriptorDatabase database;
  BuildDescriptorDatabase(&database);
  google::protobuf::DescriptorPool pool(&database);
  google::protobuf::DynamicMessageFactory factory(&pool);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  const auto* desc =
      pool.FindMessageTypeByName(TestAllTypes::descriptor()->full_name());
  ASSERT_TRUE(desc != nullptr);
  const auto* prototype = factory.GetPrototype(desc);
  ASSERT_TRUE(prototype != nullptr);
  auto* message = prototype->New();
  ASSERT_OK_AND_ASSIGN(auto value,
                       ProtoValue::Create(value_factory, std::move(*message)));
  delete message;
  EXPECT_TRUE(value->Is<ProtoStructValue>());
}

using ::cel::base_internal::FieldIdFactory;

TEST_P(ProtoStructValueTest, NewFieldIteratorIds) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoValue::Create(value_factory,
                         CreateTestMessage([](TestAllTypes& message) {
                           message.set_single_bool(true);
                           message.set_single_int32(1);
                           message.set_single_int64(1);
                           message.set_single_uint32(1);
                           message.set_single_uint64(1);
                           message.set_single_float(1.0);
                           message.set_single_double(1.0);
                           message.set_single_bytes("foo");
                           message.set_single_string("foo");
                           message.set_standalone_enum(TestAllTypes::BAR);
                           message.mutable_standalone_message()->set_bb(1);
                           message.mutable_single_duration()->set_seconds(1);
                           message.mutable_single_timestamp()->set_seconds(1);
                         })));
  EXPECT_EQ(value->As<StructValue>().field_count(), 13);
  ASSERT_OK_AND_ASSIGN(auto iterator, value->As<StructValue>().NewFieldIterator(
                                          memory_manager()));
  std::set<StructType::FieldId> actual_ids;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(
        auto id, iterator->NextId(StructValue::GetFieldContext(value_factory)));
    actual_ids.insert(id);
  }
  EXPECT_THAT(iterator->NextId(StructValue::GetFieldContext(value_factory)),
              CanonicalStatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<StructType::FieldId> expected_ids = {
      FieldIdFactory::Make(13), FieldIdFactory::Make(1),
      FieldIdFactory::Make(2),  FieldIdFactory::Make(3),
      FieldIdFactory::Make(4),  FieldIdFactory::Make(11),
      FieldIdFactory::Make(12), FieldIdFactory::Make(15),
      FieldIdFactory::Make(14), FieldIdFactory::Make(24),
      FieldIdFactory::Make(23), FieldIdFactory::Make(101),
      FieldIdFactory::Make(102)};
  EXPECT_EQ(actual_ids, expected_ids);
}

TEST_P(ProtoStructValueTest, NewFieldIteratorValues) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoValue::Create(value_factory,
                         CreateTestMessage([](TestAllTypes& message) {
                           message.set_single_bool(true);
                           message.set_single_int32(1);
                           message.set_single_int64(1);
                           message.set_single_uint32(1);
                           message.set_single_uint64(1);
                           message.set_single_float(1.0);
                           message.set_single_double(1.0);
                           message.set_single_bytes("foo");
                           message.set_single_string("foo");
                           message.set_standalone_enum(TestAllTypes::BAR);
                           message.mutable_standalone_message()->set_bb(1);
                           message.mutable_single_duration()->set_seconds(1);
                           message.mutable_single_timestamp()->set_seconds(1);
                         })));
  EXPECT_EQ(value->As<StructValue>().field_count(), 13);
  ASSERT_OK_AND_ASSIGN(auto iterator, value->As<StructValue>().NewFieldIterator(
                                          memory_manager()));
  std::vector<Handle<Value>> actual_values;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(
        auto value,
        iterator->NextValue(StructValue::GetFieldContext(value_factory)));
    actual_values.push_back(std::move(value));
  }
  EXPECT_THAT(iterator->NextValue(StructValue::GetFieldContext(value_factory)),
              CanonicalStatusIs(absl::StatusCode::kFailedPrecondition));
  // We cannot really test actual_types, as hand translating TestAllTypes would
  // be obnoxious. Otherwise we would simply be testing the same logic against
  // itself, which would not be useful.
}

INSTANTIATE_TEST_SUITE_P(ProtoStructValueTest, ProtoStructValueTest,
                         cel::base_internal::MemoryManagerTestModeAll(),
                         cel::base_internal::MemoryManagerTestModeTupleName);

}  // namespace
}  // namespace cel::extensions

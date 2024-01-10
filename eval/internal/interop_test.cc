// Copyright 2022 Google LLC
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

#include "eval/internal/interop.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/api.pb.h"
#include "google/protobuf/empty.pb.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/time/time.h"
#include "base/memory.h"
#include "base/testing/value_matchers.h"
#include "base/type.h"
#include "base/type_manager.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/error_value.h"
#include "base/values/int_value.h"
#include "base/values/struct_value.h"
#include "eval/internal/errors.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "eval/public/structs/proto_message_type_adapter.h"
#include "eval/public/structs/trivial_legacy_type_info.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/testing.h"
#include "google/protobuf/message.h"

namespace cel::interop_internal {
namespace {

using ::cel::extensions::ProtoMemoryManagerRef;
using ::cel_testing::ValueOf;
using ::google::api::expr::runtime::CelProtoWrapper;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::ContainerBackedListImpl;
using ::google::api::expr::runtime::LegacyTypeInfoApis;
using ::google::api::expr::runtime::MessageWrapper;
using ::google::api::expr::runtime::TestMessage;
using ::google::api::expr::runtime::UnknownSet;
using testing::Eq;
using testing::HasSubstr;
using testing::Optional;
using testing::Truly;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

TEST(ValueInterop, NullFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateNull();
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<NullValue>());
}

TEST(ValueInterop, NullToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.GetNullValue();
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsNull());
}

TEST(ValueInterop, BoolFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateBool(true);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<BoolValue>());
  EXPECT_TRUE(value.As<BoolValue>()->NativeValue());
}

TEST(ValueInterop, BoolToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateBoolValue(true);
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsBool());
  EXPECT_TRUE(legacy_value.BoolOrDie());
}

TEST(ValueInterop, IntFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateInt64(1);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<IntValue>());
  EXPECT_EQ(value.As<IntValue>()->NativeValue(), 1);
}

TEST(ValueInterop, IntToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateIntValue(1);
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsInt64());
  EXPECT_EQ(legacy_value.Int64OrDie(), 1);
}

TEST(ValueInterop, UintFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateUint64(1);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<UintValue>());
  EXPECT_EQ(value.As<UintValue>()->NativeValue(), 1);
}

TEST(ValueInterop, UintToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateUintValue(1);
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsUint64());
  EXPECT_EQ(legacy_value.Uint64OrDie(), 1);
}

TEST(ValueInterop, DoubleFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateDouble(1.0);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<DoubleValue>());
  EXPECT_EQ(value.As<DoubleValue>()->NativeValue(), 1.0);
}

TEST(ValueInterop, DoubleToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateDoubleValue(1.0);
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsDouble());
  EXPECT_EQ(legacy_value.DoubleOrDie(), 1.0);
}

TEST(ValueInterop, DurationFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto duration = absl::ZeroDuration() + absl::Seconds(1);
  auto legacy_value = CelValue::CreateDuration(duration);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<DurationValue>());
  EXPECT_EQ(value.As<DurationValue>()->NativeValue(), duration);
}

TEST(ValueInterop, DurationToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto duration = absl::ZeroDuration() + absl::Seconds(1);
  ASSERT_OK_AND_ASSIGN(auto value, value_factory.CreateDurationValue(duration));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsDuration());
  EXPECT_EQ(legacy_value.DurationOrDie(), duration);
}

TEST(ValueInterop, CreateDurationOk) {
  auto duration = absl::ZeroDuration() + absl::Seconds(1);
  Handle<Value> value = CreateDurationValue(duration);
  EXPECT_TRUE(value->Is<DurationValue>());
  EXPECT_EQ(value.As<DurationValue>()->NativeValue(), duration);
}

TEST(ValueInterop, CreateDurationOutOfRangeHigh) {
  Handle<Value> value = CreateDurationValue(runtime_internal::kDurationHigh);
  EXPECT_TRUE(value->Is<ErrorValue>());
  EXPECT_THAT(value.As<ErrorValue>()->NativeValue(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Duration is out of range")));
}

TEST(ValueInterop, CreateDurationOutOfRangeLow) {
  Handle<Value> value = CreateDurationValue(runtime_internal::kDurationLow);
  EXPECT_TRUE(value->Is<ErrorValue>());
  EXPECT_THAT(value.As<ErrorValue>()->NativeValue(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Duration is out of range")));
}

TEST(ValueInterop, TimestampFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto timestamp = absl::UnixEpoch() + absl::Seconds(1);
  auto legacy_value = CelValue::CreateTimestamp(timestamp);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<TimestampValue>());
  EXPECT_EQ(value.As<TimestampValue>()->NativeValue(), timestamp);
}

TEST(ValueInterop, TimestampToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto timestamp = absl::UnixEpoch() + absl::Seconds(1);
  ASSERT_OK_AND_ASSIGN(auto value,
                       value_factory.CreateTimestampValue(timestamp));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsTimestamp());
  EXPECT_EQ(legacy_value.TimestampOrDie(), timestamp);
}

TEST(ValueInterop, ErrorFromLegacy) {
  auto error = absl::CancelledError();
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateError(&error);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<ErrorValue>());
  EXPECT_EQ(value.As<ErrorValue>()->NativeValue(), error);
}

TEST(ValueInterop, TypeFromLegacy) {
  google::protobuf::Arena arena;
  auto legacy_value = CelValue::CreateCelTypeView("struct.that.does.not.Exist");
  ASSERT_OK_AND_ASSIGN(auto modern_value,
                       FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(modern_value->Is<TypeValue>());
  EXPECT_EQ(modern_value.As<TypeValue>()->name(), "struct.that.does.not.Exist");
}

TEST(ValueInterop, TypeToLegacy) {
  google::protobuf::Arena arena;
  auto modern_value =
      CreateTypeValueFromView(&arena, "struct.that.does.not.Exist");
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, modern_value));
  EXPECT_TRUE(legacy_value.IsCelType());
  EXPECT_EQ(legacy_value.CelTypeOrDie().value(), "struct.that.does.not.Exist");
}

TEST(ValueInterop, ModernTypeToStringView) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateTypeValue(type_factory.GetBoolType());
  ASSERT_OK_AND_ASSIGN(CelValue legacy_value, ToLegacyValue(&arena, value));
  ASSERT_TRUE(legacy_value.IsCelType());
  EXPECT_EQ(legacy_value.CelTypeOrDie().value(), "bool");
}

TEST(ValueInterop, StringFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateStringView("test");
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<StringValue>());
  EXPECT_EQ(value.As<StringValue>()->ToString(), "test");
}

TEST(ValueInterop, StringToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value, value_factory.CreateStringValue("test"));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsString());
  EXPECT_EQ(legacy_value.StringOrDie().value(), "test");
}

TEST(ValueInterop, CordStringToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value,
                       value_factory.CreateStringValue(absl::Cord("test")));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsString());
  EXPECT_EQ(legacy_value.StringOrDie().value(), "test");
}

TEST(ValueInterop, BytesFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateBytesView("test");
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<BytesValue>());
  EXPECT_EQ(value.As<BytesValue>()->ToString(), "test");
}

TEST(ValueInterop, BytesToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value, value_factory.CreateBytesValue("test"));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsBytes());
  EXPECT_EQ(legacy_value.BytesOrDie().value(), "test");
}

TEST(ValueInterop, CordBytesToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value,
                       value_factory.CreateBytesValue(absl::Cord("test")));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsBytes());
  EXPECT_EQ(legacy_value.BytesOrDie().value(), "test");
}

TEST(ValueInterop, ListFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value =
      CelValue::CreateList(google::protobuf::Arena::Create<
                           google::api::expr::runtime::ContainerBackedListImpl>(
          &arena, std::vector<CelValue>{CelValue::CreateInt64(0),
                                        CelValue::CreateInt64(1)}));
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  ASSERT_TRUE(value->Is<ListValue>());
  EXPECT_THAT(value->As<ListValue>().AnyOf(
                  value_factory,
                  [](const Handle<Value>& value) {
                    return value->Is<IntValue>() &&
                           value->As<IntValue>().NativeValue() == 1;
                  }),
              IsOkAndHolds(true));
  EXPECT_THAT(value->As<ListValue>().AnyOf(
                  value_factory,
                  [](const Handle<Value>& value) {
                    return value->Is<IntValue>() &&
                           value->As<IntValue>().NativeValue() > 1;
                  }),
              IsOkAndHolds(false));
  EXPECT_THAT(value->As<ListValue>().AnyOf(value_factory,
                                           [](const Handle<Value>& value) {
                                             return absl::InternalError("test");
                                           }),
              StatusIs(absl::StatusCode::kInternal, "test"));
  EXPECT_THAT(value->As<ListValue>().Contains(value_factory,
                                              value_factory.CreateIntValue(1)),
              IsOkAndHolds(Truly([](const Handle<Value>& value) {
                return value->Is<BoolValue>() &&
                       value->As<BoolValue>().NativeValue() == true;
              })));
  EXPECT_THAT(value->As<ListValue>().Contains(value_factory,
                                              value_factory.CreateIntValue(42)),
              IsOkAndHolds(Truly([](const Handle<Value>& value) {
                return value->Is<BoolValue>() &&
                       value->As<BoolValue>().NativeValue() == false;
              })));
  EXPECT_EQ(value.As<ListValue>()->Size(), 2);
  ASSERT_OK_AND_ASSIGN(auto element,
                       value.As<ListValue>()->Get(value_factory, 0));
  EXPECT_TRUE(element->Is<IntValue>());
  EXPECT_EQ(element.As<IntValue>()->NativeValue(), 0);
}

class TestListValue final : public CEL_LIST_VALUE_CLASS {
 public:
  explicit TestListValue(const Handle<ListType>& type,
                         std::vector<int64_t> elements)
      : CEL_LIST_VALUE_CLASS(type), elements_(std::move(elements)) {
    ABSL_ASSERT(type->element()->Is<IntType>());
  }

  size_t Size() const override { return elements_.size(); }

  std::string DebugString() const override {
    return absl::StrCat("[", absl::StrJoin(elements_, ", "), "]");
  }

  const std::vector<int64_t>& value() const { return elements_; }

 protected:
  absl::StatusOr<Handle<Value>> GetImpl(ValueFactory& value_factory,
                                        size_t index) const override {
    return value_factory.CreateIntValue(elements_[index]);
  }

 private:
  std::vector<int64_t> elements_;

  CEL_DECLARE_LIST_VALUE(TestListValue);
};

CEL_IMPLEMENT_LIST_VALUE(TestListValue);

TEST(ValueInterop, ListToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto type,
                       value_factory.type_factory().CreateListType(
                           value_factory.type_factory().GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto value, value_factory.CreateListValue<TestListValue>(
                                       type, std::vector<int64_t>{0}));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsList());
  EXPECT_EQ(legacy_value.ListOrDie()->size(), 1);
  EXPECT_TRUE((*legacy_value.ListOrDie()).Get(&arena, 0).IsInt64());
  EXPECT_EQ((*legacy_value.ListOrDie()).Get(&arena, 0).Int64OrDie(), 0);
}

TEST(ValueInterop, ModernListRoundtrip) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto type,
                       value_factory.type_factory().CreateListType(
                           value_factory.type_factory().GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto value, value_factory.CreateListValue<TestListValue>(
                                       type, std::vector<int64_t>{0}));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  ASSERT_OK_AND_ASSIGN(auto modern_value,
                       FromLegacyValue(&arena, legacy_value));
  // Cheat, we want pointer equality.
  EXPECT_EQ(&*value, &*modern_value);
}

TEST(ValueInterop, LegacyListRoundtrip) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value =
      CelValue::CreateList(google::protobuf::Arena::Create<
                           google::api::expr::runtime::ContainerBackedListImpl>(
          &arena, std::vector<CelValue>{CelValue::CreateInt64(0)}));
  ASSERT_OK_AND_ASSIGN(auto modern_value, FromLegacyValue(&arena, value));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, modern_value));
  EXPECT_EQ(value.ListOrDie(), legacy_value.ListOrDie());
}

TEST(ValueInterop, LegacyListNewIteratorValues) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value =
      CelValue::CreateList(google::protobuf::Arena::Create<
                           google::api::expr::runtime::ContainerBackedListImpl>(
          &arena, std::vector<CelValue>{CelValue::CreateInt64(3),
                                        CelValue::CreateInt64(4),
                                        CelValue::CreateInt64(5)}));
  ASSERT_OK_AND_ASSIGN(auto modern_value, FromLegacyValue(&arena, value));
  ASSERT_OK_AND_ASSIGN(
      auto iterator, modern_value->As<ListValue>().NewIterator(value_factory));
  std::set<int64_t> actual_values;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto value, iterator->Next());
    actual_values.insert(value->As<IntValue>().NativeValue());
  }
  EXPECT_THAT(iterator->Next(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<int64_t> expected_values = {3, 4, 5};
  EXPECT_EQ(actual_values, expected_values);
}

TEST(ValueInterop, MapFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto* legacy_map =
      google::protobuf::Arena::Create<google::api::expr::runtime::CelMapBuilder>(&arena);
  ASSERT_OK(legacy_map->Add(CelValue::CreateInt64(1),
                            CelValue::CreateStringView("foo")));
  auto legacy_value = CelValue::CreateMap(legacy_map);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<MapValue>());
  EXPECT_EQ(value.As<MapValue>()->Size(), 1);
  auto entry_key = value_factory.CreateIntValue(1);
  EXPECT_THAT(value.As<MapValue>()->Has(value_factory, entry_key),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
  ASSERT_OK_AND_ASSIGN(auto entry_value,
                       value.As<MapValue>()->Get(value_factory, entry_key));
  EXPECT_TRUE(entry_value->Is<StringValue>());
  EXPECT_EQ(entry_value.As<StringValue>()->ToString(), "foo");
}

class TestMapValue final : public CEL_MAP_VALUE_CLASS {
 public:
  explicit TestMapValue(const Handle<MapType>& type,
                        std::map<int64_t, std::string> entries)
      : CEL_MAP_VALUE_CLASS(type), entries_(std::move(entries)) {}

  std::string DebugString() const override {
    std::string output;
    output.push_back('{');
    for (const auto& entry : entries_) {
      if (output.size() > 1) {
        output.append(", ");
      }
      absl::StrAppend(&output, entry.first, ": \"",
                      absl::CHexEscape(entry.second), "\"");
    }
    output.push_back('}');
    return output;
  }

  size_t Size() const override { return entries_.size(); }

  bool IsEmpty() const override { return entries_.empty(); }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         value_factory.type_factory().CreateListType(
                             value_factory.type_factory().GetIntType()));
    std::vector<int64_t> keys;
    keys.reserve(entries_.size());
    for (const auto& entry : entries_) {
      keys.push_back(entry.first);
    }
    return value_factory.CreateListValue<TestListValue>(type, std::move(keys));
  }

 private:
  absl::StatusOr<std::pair<Handle<Value>, bool>> FindImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    auto existing = entries_.find(key.As<IntValue>()->NativeValue());
    if (existing == entries_.end()) {
      return std::make_pair(Handle<Value>(), false);
    }
    return std::make_pair(
        value_factory.CreateUncheckedStringValue(existing->second), true);
  }

  absl::StatusOr<Handle<Value>> HasImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    return value_factory.CreateBoolValue(
        entries_.find(key.As<IntValue>()->NativeValue()) != entries_.end());
  }

  std::map<int64_t, std::string> entries_;

  CEL_DECLARE_MAP_VALUE(TestMapValue);
};

CEL_IMPLEMENT_MAP_VALUE(TestMapValue);

TEST(ValueInterop, MapToLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto type,
                       value_factory.type_factory().CreateMapType(
                           value_factory.type_factory().GetIntType(),
                           value_factory.type_factory().GetStringType()));
  ASSERT_OK_AND_ASSIGN(auto value,
                       value_factory.CreateMapValue<TestMapValue>(
                           type, std::map<int64_t, std::string>{{1, "foo"}}));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  ASSERT_OK_AND_ASSIGN(auto modern_value,
                       FromLegacyValue(&arena, legacy_value));
  EXPECT_EQ(&*value, &*modern_value);
}

TEST(ValueInterop, ModernMapRoundtrip) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto type,
                       value_factory.type_factory().CreateMapType(
                           value_factory.type_factory().GetIntType(),
                           value_factory.type_factory().GetStringType()));
  ASSERT_OK_AND_ASSIGN(auto value,
                       value_factory.CreateMapValue<TestMapValue>(
                           type, std::map<int64_t, std::string>{{1, "foo"}}));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsMap());
  EXPECT_EQ(legacy_value.MapOrDie()->size(), 1);
  EXPECT_TRUE((*legacy_value.MapOrDie())
                  .Get(&arena, CelValue::CreateInt64(1))
                  .value()
                  .IsString());
  EXPECT_EQ((*legacy_value.MapOrDie())
                .Get(&arena, CelValue::CreateInt64(1))
                .value()
                .StringOrDie()
                .value(),
            "foo");
}

TEST(ValueInterop, LegacyMapRoundtrip) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = CelValue::CreateMap(
      google::protobuf::Arena::Create<google::api::expr::runtime::CelMapBuilder>(&arena));
  ASSERT_OK_AND_ASSIGN(auto modern_value, FromLegacyValue(&arena, value));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, modern_value));
  EXPECT_EQ(value.MapOrDie(), legacy_value.MapOrDie());
}

TEST(ValueInterop, LegacyMapNewIteratorKeys) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto* map_builder =
      google::protobuf::Arena::Create<google::api::expr::runtime::CelMapBuilder>(&arena);
  ASSERT_OK(map_builder->Add(CelValue::CreateStringView("foo"),
                             CelValue::CreateInt64(1)));
  ASSERT_OK(map_builder->Add(CelValue::CreateStringView("bar"),
                             CelValue::CreateInt64(2)));
  ASSERT_OK(map_builder->Add(CelValue::CreateStringView("baz"),
                             CelValue::CreateInt64(3)));
  auto value = CelValue::CreateMap(map_builder);
  ASSERT_OK_AND_ASSIGN(auto modern_value, FromLegacyValue(&arena, value));
  ASSERT_OK_AND_ASSIGN(auto iterator,
                       modern_value->As<MapValue>().NewIterator(value_factory));
  std::set<std::string> actual_keys;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto key, iterator->Next());
    actual_keys.insert(key->As<StringValue>().ToString());
  }
  EXPECT_THAT(iterator->Next(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<std::string> expected_keys = {"foo", "bar", "baz"};
  EXPECT_EQ(actual_keys, expected_keys);
}

TEST(ValueInterop, StructFromLegacy) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  google::protobuf::Api api;
  api.set_name("foo");
  auto legacy_value = CelProtoWrapper::CreateMessage(&api, &arena);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_EQ(value->kind(), Kind::kStruct);
  EXPECT_EQ(value->type()->kind(), Kind::kStruct);
  EXPECT_EQ(value->type()->name(), "google.protobuf.Api");
  EXPECT_THAT(value.As<StructValue>()->HasFieldByName(type_manager, "name"),
              IsOkAndHolds(Eq(true)));
  EXPECT_THAT(value.As<StructValue>()->HasFieldByNumber(type_manager, 1),
              StatusIs(absl::StatusCode::kUnimplemented));
  ASSERT_OK_AND_ASSIGN(
      auto value_name_field,
      value.As<StructValue>()->GetFieldByName(value_factory, "name"));
  ASSERT_TRUE(value_name_field->Is<StringValue>());
  EXPECT_EQ(value_name_field.As<StringValue>()->ToString(), "foo");
  EXPECT_THAT(value.As<StructValue>()->GetFieldByNumber(value_factory, 1),
              StatusIs(absl::StatusCode::kUnimplemented));
  auto value_wrapper = LegacyStructValueAccess::ToMessageWrapper(
      *value.As<base_internal::LegacyStructValue>());
  auto legacy_value_wrapper = legacy_value.MessageWrapperOrDie();
  EXPECT_EQ(legacy_value_wrapper.HasFullProto(), value_wrapper.HasFullProto());
  EXPECT_EQ(legacy_value_wrapper.message_ptr(), value_wrapper.message_ptr());
  EXPECT_EQ(legacy_value_wrapper.legacy_type_info(),
            value_wrapper.legacy_type_info());
}

TEST(ValueInterop, StructFromLegacyMessageLite) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  google::protobuf::Empty opaque;
  MessageWrapper wrapper(
      static_cast<const google::protobuf::MessageLite*>(&opaque),
      google::api::expr::runtime::TrivialTypeInfo::GetInstance());
  CelValue legacy_value = CelValue::CreateMessageWrapper(wrapper);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_EQ(value->kind(), Kind::kStruct);
  EXPECT_EQ(value->type()->kind(), Kind::kStruct);
  EXPECT_EQ(value->type()->name(), "opaque type");
  EXPECT_THAT(
      value.As<StructValue>()->HasFieldByName(type_manager, "name"),
      StatusIs(absl::StatusCode::kNotFound, HasSubstr("no_such_field")));
  EXPECT_THAT(value.As<StructValue>()->HasFieldByNumber(type_manager, 1),
              StatusIs(absl::StatusCode::kUnimplemented));
  EXPECT_EQ(value.As<StructValue>()->DebugString(), "opaque type");
  auto value_wrapper = LegacyStructValueAccess::ToMessageWrapper(
      *value.As<base_internal::LegacyStructValue>());
  auto legacy_value_wrapper = legacy_value.MessageWrapperOrDie();
  EXPECT_EQ(legacy_value_wrapper.HasFullProto(), value_wrapper.HasFullProto());
  EXPECT_EQ(legacy_value_wrapper.message_ptr(), value_wrapper.message_ptr());
  EXPECT_EQ(legacy_value_wrapper.legacy_type_info(),
            value_wrapper.legacy_type_info());
}

TEST(ValueInterop, StructTypeFromLegacyTypeInfo) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  google::protobuf::LinkMessageReflection<google::api::expr::runtime::TestMessage>();

  google::api::expr::runtime::ProtoMessageTypeAdapter adapter(
      TestMessage::descriptor(), google::protobuf::MessageFactory::generated_factory());

  Handle<Type> type = CreateStructTypeFromLegacyTypeInfo(
      static_cast<const LegacyTypeInfoApis*>(&adapter));

  EXPECT_EQ(type->name(), "google.api.expr.runtime.TestMessage");
}

TEST(ValueInterop, AbstractStructTypeFromLegacyTypeInfo) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  google::protobuf::LinkMessageReflection<google::api::expr::runtime::TestMessage>();

  google::api::expr::runtime::ProtoMessageTypeAdapter adapter(
      TestMessage::descriptor(), google::protobuf::MessageFactory::generated_factory());

  ASSERT_OK_AND_ASSIGN(Handle<Type> type,
                       type_factory.CreateStructType<LegacyAbstractStructType>(
                           static_cast<const LegacyTypeInfoApis&>(adapter)));

  EXPECT_EQ(type->name(), "google.api.expr.runtime.TestMessage");
  ASSERT_TRUE(type->Is<StructType>());

  ASSERT_OK_AND_ASSIGN(auto field_or, type->As<StructType>().FindFieldByName(
                                          type_manager, "string_value"));
  EXPECT_THAT(field_or, Optional(Truly([](const StructType::Field& field) {
                return field.name == "string_value" && field.number == 7;
              })))
      << "expected field string_value: 7";

  EXPECT_THAT(type->As<StructType>().FindFieldByNumber(type_manager, 7),
              StatusIs(absl::StatusCode::kUnimplemented));
  ASSERT_OK_AND_ASSIGN(auto builder,
                       type->As<StructType>().NewValueBuilder(value_factory));
  EXPECT_OK(
      builder->SetFieldByName("string_value", value_factory.GetStringValue()));
  EXPECT_OK(builder->SetFieldByNumber(7, value_factory.GetStringValue()));
  ASSERT_OK_AND_ASSIGN(auto value, std::move(*builder).Build());
  EXPECT_EQ(type->As<StructType>().field_count(), 0);
  EXPECT_THAT(type->As<StructType>().NewFieldIterator(type_manager),
              StatusIs(absl::StatusCode::kUnimplemented));
  EXPECT_TRUE(type->Is<LegacyAbstractStructType>());
}

TEST(ValueInterop, StructTypeLegacyTypeInfoRoundTrip) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  google::protobuf::LinkMessageReflection<google::api::expr::runtime::TestMessage>();

  google::api::expr::runtime::ProtoMessageTypeAdapter adapter(
      TestMessage::descriptor(), google::protobuf::MessageFactory::generated_factory());

  Handle<Type> type = CreateStructTypeFromLegacyTypeInfo(
      static_cast<const LegacyTypeInfoApis*>(&adapter));

  EXPECT_EQ(LegacyTypeInfoFromType(type),
            static_cast<const LegacyTypeInfoApis*>(&adapter));
  EXPECT_EQ(LegacyTypeInfoFromType(type_factory.GetBoolType()), nullptr);
}

TEST(ValueInterop, LegacyStructRoundtrip) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  google::protobuf::Api api;
  api.set_name("foo");
  auto value = CelProtoWrapper::CreateMessage(&api, &arena);
  ASSERT_OK_AND_ASSIGN(auto modern_value, FromLegacyValue(&arena, value));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, modern_value));
  auto value_wrapper = value.MessageWrapperOrDie();
  auto legacy_value_wrapper = legacy_value.MessageWrapperOrDie();
  EXPECT_EQ(legacy_value_wrapper.HasFullProto(), value_wrapper.HasFullProto());
  EXPECT_EQ(legacy_value_wrapper.message_ptr(), value_wrapper.message_ptr());
  EXPECT_EQ(legacy_value_wrapper.legacy_type_info(),
            value_wrapper.legacy_type_info());
}

TEST(ValueInterop, LegacyStructEquality) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  google::protobuf::Api api;
  api.set_name("foo");
  ASSERT_OK_AND_ASSIGN(
      auto lhs_value,
      FromLegacyValue(&arena, CelProtoWrapper::CreateMessage(&api, &arena)));
  ASSERT_OK_AND_ASSIGN(
      auto rhs_value,
      FromLegacyValue(&arena, CelProtoWrapper::CreateMessage(&api, &arena)));
  EXPECT_EQ(lhs_value, rhs_value);
}

using ::cel::base_internal::FieldIdFactory;

TEST(ValueInterop, LegacyStructNewFieldIteratorIds) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  google::protobuf::Api api;
  api.set_name("foo");
  api.set_version("bar");
  ASSERT_OK_AND_ASSIGN(
      auto value,
      FromLegacyValue(&arena, CelProtoWrapper::CreateMessage(&api, &arena)));
  EXPECT_EQ(value->As<StructValue>().field_count(), 2);
  ASSERT_OK_AND_ASSIGN(
      auto iterator, value->As<StructValue>().NewFieldIterator(value_factory));
  std::set<StructType::FieldId> actual_ids;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto id, iterator->NextId());
    actual_ids.insert(id);
  }
  EXPECT_THAT(iterator->NextId(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<StructType::FieldId> expected_ids = {
      FieldIdFactory::Make("name"), FieldIdFactory::Make("version")};
  EXPECT_EQ(actual_ids, expected_ids);
}

TEST(ValueInterop, LegacyStructNewFieldIteratorValues) {
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  google::protobuf::Api api;
  api.set_name("foo");
  api.set_version("bar");
  ASSERT_OK_AND_ASSIGN(
      auto value,
      FromLegacyValue(&arena, CelProtoWrapper::CreateMessage(&api, &arena)));
  EXPECT_EQ(value->As<StructValue>().field_count(), 2);
  ASSERT_OK_AND_ASSIGN(
      auto iterator, value->As<StructValue>().NewFieldIterator(value_factory));
  std::set<std::string> actual_values;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto value, iterator->NextValue());
    actual_values.insert(value->As<StringValue>().ToString());
  }
  EXPECT_THAT(iterator->NextId(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<std::string> expected_values = {"bar", "foo"};
  EXPECT_EQ(actual_values, expected_values);
}

TEST(ValueInterop, UnknownFromLegacy) {
  AttributeSet attributes({Attribute("foo")});
  FunctionResultSet function_results(
      FunctionResult(FunctionDescriptor("bar", false, std::vector<Kind>{}), 1));
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  UnknownSet unknown_set(attributes, function_results);
  auto legacy_value = CelValue::CreateUnknownSet(&unknown_set);
  ASSERT_OK_AND_ASSIGN(auto value, FromLegacyValue(&arena, legacy_value));
  EXPECT_TRUE(value->Is<UnknownValue>());
  EXPECT_EQ(value.As<UnknownValue>()->attribute_set(), attributes);
  EXPECT_EQ(value.As<UnknownValue>()->function_result_set(), function_results);
}

TEST(ValueInterop, UnknownToLegacy) {
  AttributeSet attributes({Attribute("foo")});
  FunctionResultSet function_results(
      FunctionResult(FunctionDescriptor("bar", false, std::vector<Kind>{}), 1));
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);
  TypeFactory type_factory(memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateUnknownValue(attributes, function_results);
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(&arena, value));
  EXPECT_TRUE(legacy_value.IsUnknownSet());
  EXPECT_EQ(legacy_value.UnknownSetOrDie()->unknown_attributes(), attributes);
  EXPECT_EQ(legacy_value.UnknownSetOrDie()->unknown_function_results(),
            function_results);
}

TEST(Kind, Interop) {
  EXPECT_EQ(sizeof(Kind), sizeof(CelValue::Type));
  EXPECT_EQ(alignof(Kind), alignof(CelValue::Type));
  EXPECT_EQ(static_cast<int>(Kind::kNullType),
            static_cast<int>(CelValue::LegacyType::kNullType));
  EXPECT_EQ(static_cast<int>(Kind::kBool),
            static_cast<int>(CelValue::LegacyType::kBool));
  EXPECT_EQ(static_cast<int>(Kind::kInt),
            static_cast<int>(CelValue::LegacyType::kInt64));
  EXPECT_EQ(static_cast<int>(Kind::kUint),
            static_cast<int>(CelValue::LegacyType::kUint64));
  EXPECT_EQ(static_cast<int>(Kind::kDouble),
            static_cast<int>(CelValue::LegacyType::kDouble));
  EXPECT_EQ(static_cast<int>(Kind::kString),
            static_cast<int>(CelValue::LegacyType::kString));
  EXPECT_EQ(static_cast<int>(Kind::kBytes),
            static_cast<int>(CelValue::LegacyType::kBytes));
  EXPECT_EQ(static_cast<int>(Kind::kStruct),
            static_cast<int>(CelValue::LegacyType::kMessage));
  EXPECT_EQ(static_cast<int>(Kind::kDuration),
            static_cast<int>(CelValue::LegacyType::kDuration));
  EXPECT_EQ(static_cast<int>(Kind::kTimestamp),
            static_cast<int>(CelValue::LegacyType::kTimestamp));
  EXPECT_EQ(static_cast<int>(Kind::kList),
            static_cast<int>(CelValue::LegacyType::kList));
  EXPECT_EQ(static_cast<int>(Kind::kMap),
            static_cast<int>(CelValue::LegacyType::kMap));
  EXPECT_EQ(static_cast<int>(Kind::kUnknown),
            static_cast<int>(CelValue::LegacyType::kUnknownSet));
  EXPECT_EQ(static_cast<int>(Kind::kType),
            static_cast<int>(CelValue::LegacyType::kCelType));
  EXPECT_EQ(static_cast<int>(Kind::kError),
            static_cast<int>(CelValue::LegacyType::kError));
  EXPECT_EQ(static_cast<int>(Kind::kAny),
            static_cast<int>(CelValue::LegacyType::kAny));
}

}  // namespace
}  // namespace cel::interop_internal

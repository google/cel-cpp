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
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/time/time.h"
#include "base/memory_manager.h"
#include "base/type_manager.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/unknown_set.h"
#include "internal/testing.h"

namespace cel::interop_internal {
namespace {

using google::api::expr::runtime::CelValue;
using google::api::expr::runtime::ContainerBackedListImpl;
using google::api::expr::runtime::UnknownSet;
using testing::Eq;
using cel::internal::IsOkAndHolds;

TEST(ValueInterop, NullFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateNull();
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const NullValue>());
}

TEST(ValueInterop, NullToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.GetNullValue();
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsNull());
}

TEST(ValueInterop, BoolFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateBool(true);
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const BoolValue>());
  EXPECT_TRUE(value.As<const BoolValue>()->value());
}

TEST(ValueInterop, BoolToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateBoolValue(true);
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsBool());
  EXPECT_TRUE(legacy_value.BoolOrDie());
}

TEST(ValueInterop, IntFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateInt64(1);
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const IntValue>());
  EXPECT_EQ(value.As<const IntValue>()->value(), 1);
}

TEST(ValueInterop, IntToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateIntValue(1);
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsInt64());
  EXPECT_EQ(legacy_value.Int64OrDie(), 1);
}

TEST(ValueInterop, UintFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateUint64(1);
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const UintValue>());
  EXPECT_EQ(value.As<const UintValue>()->value(), 1);
}

TEST(ValueInterop, UintToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateUintValue(1);
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsUint64());
  EXPECT_EQ(legacy_value.Uint64OrDie(), 1);
}

TEST(ValueInterop, DoubleFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateDouble(1.0);
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const DoubleValue>());
  EXPECT_EQ(value.As<const DoubleValue>()->value(), 1.0);
}

TEST(ValueInterop, DoubleToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateDoubleValue(1.0);
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsDouble());
  EXPECT_EQ(legacy_value.DoubleOrDie(), 1.0);
}

TEST(ValueInterop, DurationFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto duration = absl::ZeroDuration() + absl::Seconds(1);
  auto legacy_value = CelValue::CreateDuration(duration);
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const DurationValue>());
  EXPECT_EQ(value.As<const DurationValue>()->value(), duration);
}

TEST(ValueInterop, DurationToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto duration = absl::ZeroDuration() + absl::Seconds(1);
  ASSERT_OK_AND_ASSIGN(auto value, value_factory.CreateDurationValue(duration));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsDuration());
  EXPECT_EQ(legacy_value.DurationOrDie(), duration);
}

TEST(ValueInterop, TimestampFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto timestamp = absl::UnixEpoch() + absl::Seconds(1);
  auto legacy_value = CelValue::CreateTimestamp(timestamp);
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const TimestampValue>());
  EXPECT_EQ(value.As<const TimestampValue>()->value(), timestamp);
}

TEST(ValueInterop, TimestampToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto timestamp = absl::UnixEpoch() + absl::Seconds(1);
  ASSERT_OK_AND_ASSIGN(auto value,
                       value_factory.CreateTimestampValue(timestamp));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsTimestamp());
  EXPECT_EQ(legacy_value.TimestampOrDie(), timestamp);
}

TEST(ValueInterop, ErrorFromLegacy) {
  auto error = absl::CancelledError();
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateError(&error);
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const ErrorValue>());
  EXPECT_EQ(value.As<const ErrorValue>()->value(), error);
}

TEST(ValueInterop, TypeFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateCelTypeView("bool");
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const TypeValue>());
  EXPECT_EQ(value.As<const TypeValue>()->value(),
            type_manager.type_factory().GetBoolType());
}

TEST(ValueInterop, TypeToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateTypeValue(type_factory.GetBoolType());
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsCelType());
  EXPECT_EQ(legacy_value.CelTypeOrDie().value(), "bool");
}

TEST(ValueInterop, StringFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateStringView("test");
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const StringValue>());
  EXPECT_EQ(value.As<const StringValue>()->ToString(), "test");
}

TEST(ValueInterop, StringToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value, value_factory.CreateStringValue("test"));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsString());
  EXPECT_EQ(legacy_value.StringOrDie().value(), "test");
}

TEST(ValueInterop, CordStringToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value,
                       value_factory.CreateStringValue(absl::Cord("test")));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsString());
  EXPECT_EQ(legacy_value.StringOrDie().value(), "test");
}

TEST(ValueInterop, BytesFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateBytesView("test");
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const BytesValue>());
  EXPECT_EQ(value.As<const BytesValue>()->ToString(), "test");
}

TEST(ValueInterop, BytesToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value, value_factory.CreateBytesValue("test"));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsBytes());
  EXPECT_EQ(legacy_value.BytesOrDie().value(), "test");
}

TEST(ValueInterop, CordBytesToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto value,
                       value_factory.CreateBytesValue(absl::Cord("test")));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsBytes());
  EXPECT_EQ(legacy_value.BytesOrDie().value(), "test");
}

TEST(ValueInterop, ListFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto legacy_value = CelValue::CreateList(
      memory_manager
          ->New<google::api::expr::runtime::ContainerBackedListImpl>(
              std::vector<CelValue>{CelValue::CreateInt64(0)})
          .release());
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const ListValue>());
  EXPECT_EQ(value.As<const ListValue>()->size(), 1);
  ASSERT_OK_AND_ASSIGN(auto element,
                       value.As<const ListValue>()->Get(value_factory, 0));
  EXPECT_TRUE(element.Is<const IntValue>());
  EXPECT_EQ(element.As<const IntValue>()->value(), 0);
}

class TestListValue final : public ListValue {
 public:
  explicit TestListValue(const Persistent<const ListType>& type,
                         std::vector<int64_t> elements)
      : ListValue(type), elements_(std::move(elements)) {
    ABSL_ASSERT(type->element().Is<IntType>());
  }

  size_t size() const override { return elements_.size(); }

  absl::StatusOr<Persistent<const Value>> Get(ValueFactory& value_factory,
                                              size_t index) const override {
    if (index >= size()) {
      return absl::OutOfRangeError("");
    }
    return value_factory.CreateIntValue(elements_[index]);
  }

  std::string DebugString() const override {
    return absl::StrCat("[", absl::StrJoin(elements_, ", "), "]");
  }

  const std::vector<int64_t>& value() const { return elements_; }

 private:
  bool Equals(const Value& other) const override {
    return Is(other) &&
           elements_ == static_cast<const TestListValue&>(other).elements_;
  }

  void HashValue(absl::HashState state) const override {
    absl::HashState::combine(std::move(state), type(), elements_);
  }

  std::vector<int64_t> elements_;

  CEL_DECLARE_LIST_VALUE(TestListValue);
};

CEL_IMPLEMENT_LIST_VALUE(TestListValue);

TEST(ValueInterop, ListToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto type,
                       value_factory.type_factory().CreateListType(
                           value_factory.type_factory().GetIntType()));
  ASSERT_OK_AND_ASSIGN(auto value, value_factory.CreateListValue<TestListValue>(
                                       type, std::vector<int64_t>{0}));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsList());
  EXPECT_EQ(legacy_value.ListOrDie()->size(), 1);
  EXPECT_TRUE((*legacy_value.ListOrDie())[0].IsInt64());
  EXPECT_EQ((*legacy_value.ListOrDie())[0].Int64OrDie(), 0);
}

TEST(ValueInterop, MapFromLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto* legacy_map =
      memory_manager->New<google::api::expr::runtime::CelMapBuilder>()
          .release();
  ASSERT_OK(legacy_map->Add(CelValue::CreateInt64(1),
                            CelValue::CreateStringView("foo")));
  auto legacy_value = CelValue::CreateMap(legacy_map);
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const MapValue>());
  EXPECT_EQ(value.As<const MapValue>()->size(), 1);
  auto entry_key = value_factory.CreateIntValue(1);
  EXPECT_THAT(value.As<const MapValue>()->Has(entry_key),
              IsOkAndHolds(Eq(true)));
  ASSERT_OK_AND_ASSIGN(auto entry_value, value.As<const MapValue>()->Get(
                                             value_factory, entry_key));
  EXPECT_TRUE(entry_value.Is<const StringValue>());
  EXPECT_EQ(entry_value.As<const StringValue>()->ToString(), "foo");
}

class TestMapValue final : public MapValue {
 public:
  explicit TestMapValue(const Persistent<const MapType>& type,
                        std::map<int64_t, std::string> entries)
      : MapValue(type), entries_(std::move(entries)) {}

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

  size_t size() const override { return entries_.size(); }

  bool empty() const override { return entries_.empty(); }

  bool Equals(const Value& other) const override {
    return Is(other) &&
           entries_ == static_cast<const TestMapValue&>(other).entries_;
  }

  void HashValue(absl::HashState state) const override {
    absl::HashState::combine(std::move(state), type(), entries_);
  }

  absl::StatusOr<Persistent<const Value>> Get(
      ValueFactory& value_factory,
      const Persistent<const Value>& key) const override {
    auto existing = entries_.find(key.As<const IntValue>()->value());
    if (existing == entries_.end()) {
      return Persistent<const Value>();
    }
    return value_factory.CreateStringValue(existing->second);
  }

  absl::StatusOr<bool> Has(const Persistent<const Value>& key) const override {
    return entries_.find(key.As<const IntValue>()->value()) != entries_.end();
  }

  absl::StatusOr<Persistent<const ListValue>> ListKeys(
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
  std::map<int64_t, std::string> entries_;

  CEL_DECLARE_MAP_VALUE(TestMapValue);
};

CEL_IMPLEMENT_MAP_VALUE(TestMapValue);

TEST(ValueInterop, MapToLegacy) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto type,
                       value_factory.type_factory().CreateMapType(
                           value_factory.type_factory().GetIntType(),
                           value_factory.type_factory().GetStringType()));
  ASSERT_OK_AND_ASSIGN(auto value,
                       value_factory.CreateMapValue<TestMapValue>(
                           type, std::map<int64_t, std::string>{{1, "foo"}}));
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
  EXPECT_TRUE(legacy_value.IsMap());
  EXPECT_EQ(legacy_value.MapOrDie()->size(), 1);
  EXPECT_TRUE(
      (*legacy_value.MapOrDie())[CelValue::CreateInt64(1)].value().IsString());
  EXPECT_EQ((*legacy_value.MapOrDie())[CelValue::CreateInt64(1)]
                .value()
                .StringOrDie()
                .value(),
            "foo");
}

TEST(ValueInterop, UnknownFromLegacy) {
  AttributeSet attributes({Attribute("foo")});
  FunctionResultSet function_results(
      FunctionResult(FunctionDescriptor("bar", false, std::vector<Kind>{}), 1));
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  UnknownSet unknown_set(attributes, function_results);
  auto legacy_value = CelValue::CreateUnknownSet(&unknown_set);
  ASSERT_OK_AND_ASSIGN(auto value,
                       FromLegacyValue(value_factory, legacy_value));
  EXPECT_TRUE(value.Is<const UnknownValue>());
  EXPECT_EQ(value.As<const UnknownValue>()->attribute_set(), attributes);
  EXPECT_EQ(value.As<const UnknownValue>()->function_result_set(),
            function_results);
}

TEST(ValueInterop, UnknownToLegacy) {
  AttributeSet attributes({Attribute("foo")});
  FunctionResultSet function_results(
      FunctionResult(FunctionDescriptor("bar", false, std::vector<Kind>{}), 1));
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateUnknownValue(attributes, function_results);
  ASSERT_OK_AND_ASSIGN(auto legacy_value, ToLegacyValue(value_factory, value));
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

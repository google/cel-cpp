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

#include "common/value_factory.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/types/type_cache.h"
#include "common/value.h"
#include "common/values/value_cache.h"
#include "internal/overloaded.h"

namespace cel {

namespace {

using common_internal::ProcessLocalTypeCache;
using common_internal::ProcessLocalValueCache;

ValueView JsonToValue(const Json& json, ValueFactory& value_factory,
                      Value& scratch) {
  return absl::visit(
      internal::Overloaded{
          [](JsonNull) -> ValueView { return NullValueView(); },
          [](JsonBool value) -> ValueView { return BoolValueView(value); },
          [](JsonNumber value) -> ValueView { return DoubleValueView(value); },
          [](const JsonString& value) -> ValueView {
            return StringValueView(value);
          },
          [&value_factory, &scratch](const JsonArray& value) -> ValueView {
            scratch = value_factory.CreateListValueFromJsonArray(value);
            return scratch;
          },
          [&value_factory, &scratch](const JsonObject& value) -> ValueView {
            scratch = value_factory.CreateMapValueFromJsonObject(value);
            return scratch;
          },
      },
      json);
}

void JsonDebugString(const Json& json, std::string& out);

void JsonArrayDebugString(const JsonArray& json, std::string& out) {
  out.push_back('[');
  auto element = json.begin();
  if (element != json.end()) {
    JsonDebugString(*element, out);
    ++element;
    for (; element != json.end(); ++element) {
      out.append(", ");
      JsonDebugString(*element, out);
    }
  }
  out.push_back(']');
}

void JsonObjectEntryDebugString(const JsonString& key, const Json& value,
                                std::string& out) {
  out.append(StringValueView(key).DebugString());
  out.append(": ");
  JsonDebugString(value, out);
}

void JsonObjectDebugString(const JsonObject& json, std::string& out) {
  std::vector<JsonString> keys;
  keys.reserve(json.size());
  for (const auto& entry : json) {
    keys.push_back(entry.first);
  }
  std::stable_sort(keys.begin(), keys.end());
  out.push_back('{');
  auto key = keys.begin();
  if (key != keys.end()) {
    JsonObjectEntryDebugString(*key, json.find(*key)->second, out);
    ++key;
    for (; key != keys.end(); ++key) {
      out.append(", ");
      JsonObjectEntryDebugString(*key, json.find(*key)->second, out);
    }
  }
  out.push_back('}');
}

void JsonDebugString(const Json& json, std::string& out) {
  absl::visit(internal::Overloaded{
                  [&out](JsonNull) -> void {
                    out.append(NullValueView().DebugString());
                  },
                  [&out](JsonBool value) -> void {
                    out.append(BoolValueView(value).DebugString());
                  },
                  [&out](JsonNumber value) -> void {
                    out.append(DoubleValueView(value).DebugString());
                  },
                  [&out](const JsonString& value) -> void {
                    out.append(StringValueView(value).DebugString());
                  },
                  [&out](const JsonArray& value) -> void {
                    JsonArrayDebugString(value, out);
                  },
                  [&out](const JsonObject& value) -> void {
                    JsonObjectDebugString(value, out);
                  },
              },
              json);
}

class JsonListValue final : public ListValueInterface {
 public:
  explicit JsonListValue(JsonArray array) : array_(std::move(array)) {}

  std::string DebugString() const override {
    std::string out;
    JsonArrayDebugString(array_, out);
    return out;
  }

  bool IsEmpty() const override { return array_.empty(); }

  size_t Size() const override { return array_.size(); }

  absl::StatusOr<JsonArray> ConvertToJsonArray() const override {
    return array_;
  }

 private:
  TypeView get_type() const noexcept override {
    return ProcessLocalTypeCache::Get()->GetDynListType();
  }

  absl::StatusOr<ValueView> GetImpl(ValueFactory& value_factory, size_t index,
                                    Value& scratch) const override {
    return JsonToValue(array_[index], value_factory, scratch);
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<JsonListValue>();
  }

  const JsonArray array_;
};

class JsonMapValueKeyIterator final : public ValueIterator {
 public:
  explicit JsonMapValueKeyIterator(
      const JsonObject& object ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : begin_(object.begin()), end_(object.end()) {}

  bool HasNext() override { return begin_ != end_; }

  absl::StatusOr<ValueView> Next(Value&) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when "
          "ValueIterator::HasNext() returns false");
    }
    const auto& key = begin_->first;
    ++begin_;
    return StringValueView(key);
  }

 private:
  typename JsonObject::const_iterator begin_;
  typename JsonObject::const_iterator end_;
};

class JsonMapValue final : public MapValueInterface {
 public:
  explicit JsonMapValue(JsonObject object) : object_(std::move(object)) {}

  std::string DebugString() const override {
    std::string out;
    JsonObjectDebugString(object_, out);
    return out;
  }

  bool IsEmpty() const override { return object_.empty(); }

  size_t Size() const override { return object_.size(); }

  // Returns a new list value whose elements are the keys of this map.
  absl::StatusOr<ListValueView> ListKeys(TypeFactory& type_factory,
                                         ValueFactory& value_factory,
                                         ListValue& scratch) const override {
    JsonArrayBuilder keys;
    keys.reserve(object_.size());
    for (const auto& entry : object_) {
      keys.push_back(entry.first);
    }
    scratch =
        ListValue(value_factory.GetMemoryManager().MakeShared<JsonListValue>(
            std::move(keys).Build()));
    return scratch;
  }

  // By default, implementations do not guarantee any iteration order. Unless
  // specified otherwise, assume the iteration order is random.
  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueFactory& value_factory) const override {
    return std::make_unique<JsonMapValueKeyIterator>(object_);
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject() const override {
    return object_;
  }

 private:
  // Called by `Find` after performing various argument checks.
  absl::StatusOr<absl::optional<ValueView>> FindImpl(
      ValueFactory& value_factory, ValueView key,
      Value& scratch) const override {
    return Cast<StringValueView>(key).NativeValue(internal::Overloaded{
        [this, &value_factory,
         &scratch](absl::string_view value) -> absl::optional<ValueView> {
          if (auto entry = object_.find(value); entry != object_.end()) {
            return JsonToValue(entry->second, value_factory, scratch);
          }
          return absl::nullopt;
        },
        [this, &value_factory,
         &scratch](const absl::Cord& value) -> absl::optional<ValueView> {
          if (auto entry = object_.find(value); entry != object_.end()) {
            return JsonToValue(entry->second, value_factory, scratch);
          }
          return absl::nullopt;
        },
    });
  }

  // Called by `Has` after performing various argument checks.
  absl::StatusOr<bool> HasImpl(ValueView key) const override {
    return Cast<StringValueView>(key).NativeValue(internal::Overloaded{
        [this](absl::string_view value) -> bool {
          return object_.contains(value);
        },
        [this](const absl::Cord& value) -> bool {
          return object_.contains(value);
        },
    });
  }

  TypeView get_type() const noexcept override {
    return ProcessLocalTypeCache::Get()->GetStringDynMapType();
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<JsonMapValue>();
  }

  const JsonObject object_;
};

}  // namespace

Value ValueFactory::CreateValueFromJson(Json json) {
  return absl::visit(
      internal::Overloaded{
          [](JsonNull) -> Value { return NullValue(); },
          [](JsonBool value) -> Value { return BoolValue(value); },
          [](JsonNumber value) -> Value { return DoubleValue(value); },
          [](const JsonString& value) -> Value { return StringValue(value); },
          [this](JsonArray value) -> Value {
            return CreateListValueFromJsonArray(std::move(value));
          },
          [this](JsonObject value) -> Value {
            return CreateMapValueFromJsonObject(std::move(value));
          },
      },
      std::move(json));
}

ListValue ValueFactory::CreateListValueFromJsonArray(JsonArray json) {
  if (json.empty()) {
    return ListValue(GetZeroDynListValue());
  }
  return ListValue(
      GetMemoryManager().MakeShared<JsonListValue>(std::move(json)));
}

MapValue ValueFactory::CreateMapValueFromJsonObject(JsonObject json) {
  if (json.empty()) {
    return MapValue(GetZeroStringDynMapValue());
  }
  return MapValue(GetMemoryManager().MakeShared<JsonMapValue>(std::move(json)));
}

ListValue ValueFactory::CreateZeroListValue(ListTypeView type) {
  if (auto list_value = ProcessLocalValueCache::Get()->GetEmptyListValue(type);
      list_value.has_value()) {
    return ListValue(*list_value);
  }
  return CreateZeroListValueImpl(type);
}

MapValue ValueFactory::CreateZeroMapValue(MapTypeView type) {
  if (auto map_value = ProcessLocalValueCache::Get()->GetEmptyMapValue(type);
      map_value.has_value()) {
    return MapValue(*map_value);
  }
  return CreateZeroMapValueImpl(type);
}

OptionalValue ValueFactory::CreateZeroOptionalValue(OptionalTypeView type) {
  if (auto optional_value =
          ProcessLocalValueCache::Get()->GetEmptyOptionalValue(type);
      optional_value.has_value()) {
    return OptionalValue(*optional_value);
  }
  return CreateZeroOptionalValueImpl(type);
}

ListValueView ValueFactory::GetZeroDynListValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynListValue();
}

MapValueView ValueFactory::GetZeroDynDynMapValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynDynMapValue();
}

MapValueView ValueFactory::GetZeroStringDynMapValue() {
  return ProcessLocalValueCache::Get()->GetEmptyStringDynMapValue();
}

OptionalValueView ValueFactory::GetZeroDynOptionalValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynOptionalValue();
}

}  // namespace cel

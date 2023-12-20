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

#include "base/value_factory.h"

#include <limits>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/value.h"
#include "base/values/list_value.h"
#include "base/values/string_value.h"
#include "common/json.h"
#include "internal/overloaded.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/utf8.h"

namespace cel {

namespace {

using base_internal::HandleFactory;
using base_internal::InlinedCordBytesValue;
using base_internal::InlinedCordStringValue;
using base_internal::InlinedStringViewBytesValue;
using base_internal::InlinedStringViewStringValue;
using base_internal::StringBytesValue;
using base_internal::StringStringValue;

}  // namespace

Handle<NullValue> NullValue::Get(ValueFactory& value_factory) {
  return value_factory.GetNullValue();
}

Handle<ErrorValue> ValueFactory::CreateErrorValue(absl::Status status) {
  if (ABSL_PREDICT_FALSE(status.ok())) {
    status = absl::UnknownError(
        "If you are seeing this message the caller attempted to construct an "
        "error value from a successful status. Refusing to fail successfully.");
  }
  return HandleFactory<ErrorValue>::Make<ErrorValue>(std::move(status));
}

Handle<BoolValue> BoolValue::False(ValueFactory& value_factory) {
  return value_factory.CreateBoolValue(false);
}

Handle<BoolValue> BoolValue::True(ValueFactory& value_factory) {
  return value_factory.CreateBoolValue(true);
}

Handle<DoubleValue> DoubleValue::NaN(ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      std::numeric_limits<double>::quiet_NaN());
}

Handle<DoubleValue> DoubleValue::PositiveInfinity(ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      std::numeric_limits<double>::infinity());
}

Handle<DoubleValue> DoubleValue::NegativeInfinity(ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      -std::numeric_limits<double>::infinity());
}

Handle<DurationValue> DurationValue::Zero(ValueFactory& value_factory) {
  // Should never fail, tests assert this.
  return value_factory.CreateDurationValue(absl::ZeroDuration()).value();
}

Handle<TimestampValue> TimestampValue::UnixEpoch(ValueFactory& value_factory) {
  // Should never fail, tests assert this.
  return value_factory.CreateTimestampValue(absl::UnixEpoch()).value();
}

Handle<StringValue> StringValue::Empty(ValueFactory& value_factory) {
  return value_factory.GetStringValue();
}

absl::StatusOr<Handle<StringValue>> StringValue::Concat(
    ValueFactory& value_factory, const StringValue& lhs,
    const StringValue& rhs) {
  absl::Cord cord;
  cord.Append(lhs.ToCord());
  cord.Append(rhs.ToCord());
  return value_factory.CreateStringValue(std::move(cord));
}

Handle<BytesValue> BytesValue::Empty(ValueFactory& value_factory) {
  return value_factory.GetBytesValue();
}

absl::StatusOr<Handle<BytesValue>> BytesValue::Concat(
    ValueFactory& value_factory, const BytesValue& lhs, const BytesValue& rhs) {
  absl::Cord cord;
  cord.Append(lhs.ToCord());
  cord.Append(rhs.ToCord());
  return value_factory.CreateBytesValue(std::move(cord));
}

absl::StatusOr<Handle<BytesValue>> ValueFactory::CreateBytesValue(
    std::string value) {
  if (value.empty()) {
    return GetEmptyBytesValue();
  }
  return HandleFactory<BytesValue>::Make<StringBytesValue>(memory_manager(),
                                                           std::move(value));
}

absl::StatusOr<Handle<BytesValue>> ValueFactory::CreateBytesValue(
    absl::Cord value) {
  if (value.empty()) {
    return GetEmptyBytesValue();
  }
  return HandleFactory<BytesValue>::Make<InlinedCordBytesValue>(
      std::move(value));
}

absl::StatusOr<Handle<StringValue>> ValueFactory::CreateStringValue(
    std::string value) {
  // Avoid persisting empty strings which may have underlying storage after
  // mutating.
  if (value.empty()) {
    return GetEmptyStringValue();
  }
  auto [count, ok] = internal::Utf8Validate(value);
  if (ABSL_PREDICT_FALSE(!ok)) {
    return absl::InvalidArgumentError(
        "Illegal byte sequence in UTF-8 encoded string");
  }
  return HandleFactory<StringValue>::Make<StringStringValue>(memory_manager(),
                                                             std::move(value));
}

Handle<StringValue> ValueFactory::CreateUncheckedStringValue(
    std::string value) {
  // Avoid persisting empty strings which may have underlying storage after
  // mutating.
  if (value.empty()) {
    return GetEmptyStringValue();
  }

  return HandleFactory<StringValue>::Make<StringStringValue>(memory_manager(),
                                                             std::move(value));
}

Handle<StringValue> ValueFactory::CreateUncheckedStringValue(absl::Cord value) {
  // Avoid persisting empty strings which may have underlying storage after
  // mutating.
  if (value.empty()) {
    return GetEmptyStringValue();
  }

  return HandleFactory<StringValue>::Make<InlinedCordStringValue>(
      std::move(value));
}

absl::StatusOr<Handle<StringValue>> ValueFactory::CreateStringValue(
    absl::Cord value) {
  if (value.empty()) {
    return GetEmptyStringValue();
  }
  auto [count, ok] = internal::Utf8Validate(value);
  if (ABSL_PREDICT_FALSE(!ok)) {
    return absl::InvalidArgumentError(
        "Illegal byte sequence in UTF-8 encoded string");
  }
  return HandleFactory<StringValue>::Make<InlinedCordStringValue>(
      std::move(value));
}

absl::StatusOr<Handle<DurationValue>> ValueFactory::CreateDurationValue(
    absl::Duration value) {
  CEL_RETURN_IF_ERROR(internal::ValidateDuration(value));
  return CreateUncheckedDurationValue(value);
}

absl::StatusOr<Handle<TimestampValue>> ValueFactory::CreateTimestampValue(
    absl::Time value) {
  CEL_RETURN_IF_ERROR(internal::ValidateTimestamp(value));
  return CreateUncheckedTimestampValue(value);
}

Handle<UnknownValue> ValueFactory::CreateUnknownValue(
    AttributeSet attribute_set, FunctionResultSet function_result_set) {
  return HandleFactory<UnknownValue>::Make<UnknownValue>(
      base_internal::UnknownSet(std::move(attribute_set),
                                std::move(function_result_set)));
}

absl::StatusOr<Handle<BytesValue>> ValueFactory::CreateBytesValueFromView(
    absl::string_view value) {
  return HandleFactory<BytesValue>::Make<InlinedStringViewBytesValue>(value);
}

absl::StatusOr<Handle<StringValue>> ValueFactory::CreateStringValueFromView(
    absl::string_view value) {
  return HandleFactory<StringValue>::Make<InlinedStringViewStringValue>(value);
}

Handle<Value> ValueFactory::CreateValueFromJson(Json json) {
  return absl::visit(
      internal::Overloaded{
          [this](JsonNull) -> Handle<Value> { return GetNullValue(); },
          [this](JsonBool value) -> Handle<Value> {
            return CreateBoolValue(value);
          },
          [this](JsonNumber value) -> Handle<Value> {
            return CreateDoubleValue(value);
          },
          [this](JsonString value) -> Handle<Value> {
            return CreateUncheckedStringValue(std::move(value));
          },
          [this](JsonArray value) -> Handle<Value> {
            return CreateListValueFromJson(std::move(value));
          },
          [this](JsonObject value) -> Handle<Value> {
            return CreateMapValueFromJson(std::move(value));
          }},
      std::move(json));
}

namespace {

void JsonAppendDebugString(const Json& json, std::string& out);

void JsonArrayAppendDebugString(const JsonArray& array, std::string& out) {
  auto begin = array.begin();
  auto end = array.end();
  out.push_back('[');
  if (begin != end) {
    JsonAppendDebugString(*begin++, out);
    for (; begin != end; ++begin) {
      out.append(", ");
      JsonAppendDebugString(*begin, out);
    }
  }
  out.push_back(']');
}

void JsonObjectAppendDebugString(const JsonObject& object, std::string& out) {
  auto begin = object.begin();
  auto end = object.end();
  out.push_back('{');
  if (begin != end) {
    out.append(StringValue::DebugString(begin->first));
    out.append(": ");
    JsonAppendDebugString(begin->second, out);
    ++begin;
    for (; begin != end; ++begin) {
      out.append(", ");
      out.append(StringValue::DebugString(begin->first));
      out.append(": ");
      JsonAppendDebugString(begin->second, out);
    }
  }
  out.push_back('}');
}

void JsonAppendDebugString(const Json& json, std::string& out) {
  absl::visit(
      internal::Overloaded{
          [&out](JsonNull) { out.append("null"); },
          [&out](JsonBool value) { out.append(value ? "true" : "false"); },
          [&out](JsonNumber value) {
            out.append(DoubleValue::DebugString(value));
          },
          [&out](const JsonString& value) {
            out.append(StringValue::DebugString(value));
          },
          [&out](const JsonArray& value) {
            JsonArrayAppendDebugString(value, out);
          },
          [&out](const JsonObject& value) {
            JsonObjectAppendDebugString(value, out);
          }},
      json);
}

std::string JsonArrayDebugString(const JsonArray& array) {
  std::string out;
  JsonArrayAppendDebugString(array, out);
  return out;
}

std::string JsonObjectDebugString(const JsonObject& object) {
  std::string out;
  JsonObjectAppendDebugString(object, out);
  return out;
}

class JsonListValue final : public CEL_LIST_VALUE_CLASS {
 public:
  JsonListValue(Handle<ListType> type, JsonArray array)
      : CEL_LIST_VALUE_CLASS(std::move(type)), array_(std::move(array)) {}

  std::string DebugString() const override {
    return JsonArrayDebugString(array_);
  }

  absl::StatusOr<JsonArray> ConvertToJsonArray(ValueFactory&) const override {
    return array_;
  }

  size_t Size() const override { return array_.size(); }

  bool IsEmpty() const override { return array_.empty(); }

 protected:
  absl::StatusOr<Handle<Value>> GetImpl(ValueFactory& value_factory,
                                        size_t index) const override {
    return value_factory.CreateValueFromJson(array_[index]);
  }

 private:
  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<JsonListValue>();
  }

  const JsonArray array_;
};

class JsonMapValue final : public CEL_MAP_VALUE_CLASS {
 public:
  JsonMapValue(Handle<MapType> type, JsonObject object)
      : CEL_MAP_VALUE_CLASS(std::move(type)), object_(std::move(object)) {}

  std::string DebugString() const override {
    return JsonObjectDebugString(object_);
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject(ValueFactory&) const override {
    return object_;
  }

  size_t Size() const override { return object_.size(); }

  bool IsEmpty() const override { return object_.empty(); }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         value_factory.type_factory().CreateListType(
                             value_factory.type_factory().GetStringType()));
    JsonArrayBuilder builder;
    builder.reserve(object_.size());
    for (const auto& entry : object_) {
      builder.push_back(entry.first);
    }
    return HandleFactory<ListValue>::Make<JsonListValue>(
        value_factory.memory_manager(), std::move(type),
        std::move(builder).Build());
  }

 private:
  absl::StatusOr<std::pair<Handle<Value>, bool>> FindImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    // TODO(uncreated-issue/32): fix this for heterogeneous equality
    if (!key->Is<StringValue>()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Expected key to be string type: ", key->type()->DebugString()));
    }
    return key->As<StringValue>().Visit(
        [this, &value_factory](const auto& value)
            -> absl::StatusOr<std::pair<Handle<Value>, bool>> {
          if (auto it = object_.find(value); it != object_.end()) {
            return std::make_pair(value_factory.CreateValueFromJson(it->second),
                                  true);
          }
          return std::make_pair(Handle<Value>(), false);
        });
  }

  absl::StatusOr<Handle<Value>> HasImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    // TODO(uncreated-issue/32): fix this for heterogeneous equality
    if (!key->Is<StringValue>()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Expected key to be string type: ", key->type()->DebugString()));
    }
    return value_factory.CreateBoolValue(
        key->As<StringValue>().Visit([this](const auto& value) -> bool {
          return object_.find(value) != object_.end();
        }));
  }

  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<JsonMapValue>();
  }

  const JsonObject object_;
};

}  // namespace

Handle<ListValue> ValueFactory::CreateListValueFromJson(JsonArray array) {
  return HandleFactory<ListValue>::Make<JsonListValue>(
      memory_manager(), type_factory().GetJsonListType(), std::move(array));
}

Handle<MapValue> ValueFactory::CreateMapValueFromJson(JsonObject object) {
  return HandleFactory<MapValue>::Make<JsonMapValue>(
      memory_manager(), type_factory().GetJsonMapType(), std::move(object));
}

}  // namespace cel

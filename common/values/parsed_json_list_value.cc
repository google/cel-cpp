// Copyright 2024 Google LLC
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

#include "common/values/parsed_json_list_value.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/parsed_json_value.h"
#include "internal/json.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel {

namespace common_internal {

absl::Status CheckWellKnownListValueMessage(
    const google::protobuf::MessageLite& message) {
  return internal::CheckJsonList(message);
}

}  // namespace common_internal

ParsedJsonListValue::ParsedJsonListValue()
    : ParsedJsonListValue(common_internal::WrapEternal(
          &google::protobuf::ListValue::default_instance())) {}

std::string ParsedJsonListValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return "[]";
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return internal::JsonListDebugString(*generated_message);
  }
  return internal::JsonListDebugString(
      google::protobuf::DownCastMessage<google::protobuf::Message>(*value_));
}

absl::Status ParsedJsonListValue::SerializeTo(AnyToJsonConverter& converter,
                                              absl::Cord& value) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    value.Clear();
    return absl::OkStatus();
  }
  if (!value_->SerializePartialToCord(&value)) {
    return absl::UnknownError("failed to serialize protocol buffer message");
  }
  return absl::OkStatus();
}

absl::StatusOr<Json> ParsedJsonListValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return JsonArray();
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return internal::ProtoJsonListToNativeJsonList(*generated_message);
  }
  return internal::ProtoJsonListToNativeJsonList(
      google::protobuf::DownCastMessage<google::protobuf::Message>(*value_));
}

absl::Status ParsedJsonListValue::Equal(ValueManager& value_manager,
                                        const Value& other,
                                        Value& result) const {
  ABSL_DCHECK(*this);
  if (auto other_value = other.AsParsedJsonList(); other_value) {
    result = BoolValue(*this == *other_value);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsList(); other_value) {
    return common_internal::ListValueEqual(value_manager, ListValue(*this),
                                           *other_value, result);
  }
  result = BoolValue(false);
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedJsonListValue::Equal(ValueManager& value_manager,
                                                 const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

namespace {

size_t SizeGenerated(const google::protobuf::ListValue& message) {
  return static_cast<size_t>(
      well_known_types::ListValueReflection::ValuesSize(message));
}

size_t SizeDynamic(const google::protobuf::Message& message) {
  return static_cast<size_t>(
      well_known_types::GetListValueReflectionOrDie(message.GetDescriptor())
          .ValuesSize(message));
}

}  // namespace

size_t ParsedJsonListValue::Size() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return 0;
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return SizeGenerated(*generated_message);
  }
  return SizeDynamic(google::protobuf::DownCastMessage<google::protobuf::Message>(*value_));
}

namespace {

absl::Status GetGenerated(Borrowed<const google::protobuf::ListValue> message,
                          ValueManager& value_manager, size_t index,
                          Value& result) {
  if (ABSL_PREDICT_FALSE(
          index >=
          static_cast<size_t>(
              well_known_types::ListValueReflection::ValuesSize(*message)))) {
    return absl::InvalidArgumentError("index out of bounds");
  }
  result = common_internal::ParsedJsonValue(
      Borrowed(message, &well_known_types::ListValueReflection::Values(
                            *message, static_cast<int>(index))));
  return absl::OkStatus();
}

absl::Status GetDynamic(Borrowed<const google::protobuf::Message> message,
                        ValueManager& value_manager, size_t index,
                        Value& result) {
  const auto reflection =
      well_known_types::GetListValueReflectionOrDie(message->GetDescriptor());
  if (ABSL_PREDICT_FALSE(
          index >= static_cast<size_t>(reflection.ValuesSize(*message)))) {
    return absl::InvalidArgumentError("index out of bounds");
  }
  result = common_internal::ParsedJsonValue(
      value_manager.GetMemoryManager().arena(),
      Borrowed(message, &reflection.Values(*message, static_cast<int>(index))));
  return absl::OkStatus();
}

}  // namespace

// See ListValueInterface::Get for documentation.
absl::Status ParsedJsonListValue::Get(ValueManager& value_manager, size_t index,
                                      Value& result) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return absl::InvalidArgumentError("index out of bounds");
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return GetGenerated(Borrowed(value_, generated_message), value_manager,
                        index, result);
  }
  return GetDynamic(Borrowed(value_, google::protobuf::DownCastMessage<google::protobuf::Message>(
                                         cel::to_address(value_))),
                    value_manager, index, result);
}

absl::StatusOr<Value> ParsedJsonListValue::Get(ValueManager& value_manager,
                                               size_t index) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, index, result));
  return result;
}

absl::Status ParsedJsonListValue::ForEach(ValueManager& value_manager,
                                          ForEachCallback callback) const {
  return ForEach(value_manager,
                 [callback = std::move(callback)](size_t, const Value& value)
                     -> absl::StatusOr<bool> { return callback(value); });
}

namespace {

absl::Status ForEachGenerated(
    Borrowed<const google::protobuf::ListValue> message,
    ValueManager& value_manager,
    ParsedJsonListValue::ForEachWithIndexCallback callback) {
  Value scratch;
  const int size = well_known_types::ListValueReflection::ValuesSize(*message);
  for (int i = 0; i < size; ++i) {
    scratch = common_internal::ParsedJsonValue(Borrowed(
        message, &well_known_types::ListValueReflection::Values(*message, i)));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(static_cast<size_t>(i), scratch));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::Status ForEachDynamic(
    Borrowed<const google::protobuf::Message> message, ValueManager& value_manager,
    ParsedJsonListValue::ForEachWithIndexCallback callback) {
  Value scratch;
  const auto reflection =
      well_known_types::GetListValueReflectionOrDie(message->GetDescriptor());
  const int size = reflection.ValuesSize(*message);
  for (int i = 0; i < size; ++i) {
    scratch = common_internal::ParsedJsonValue(
        value_manager.GetMemoryManager().arena(),
        Borrowed(message, &reflection.Values(*message, i)));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(static_cast<size_t>(i), scratch));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status ParsedJsonListValue::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return absl::OkStatus();
  }
  Value scratch;
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return ForEachGenerated(Borrowed(value_, generated_message), value_manager,
                            callback);
  }
  return ForEachDynamic(
      Borrowed(value_, google::protobuf::DownCastMessage<google::protobuf::Message>(
                           cel::to_address(value_))),
      value_manager, callback);
}

namespace {

class GeneratedParsedJsonListValueIterator final : public ValueIterator {
 public:
  explicit GeneratedParsedJsonListValueIterator(
      Owned<const google::protobuf::ListValue> message)
      : message_(std::move(message)),
        size_(well_known_types::ListValueReflection::ValuesSize(*message_)) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(ValueManager& value_manager, Value& result) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "`ValueIterator::Next` called after `ValueIterator::HasNext` "
          "returned false");
    }
    result = common_internal::ParsedJsonValue(Borrowed(
        message_,
        &well_known_types::ListValueReflection::Values(*message_, index_)));
    ++index_;
    return absl::OkStatus();
  }

 private:
  const Owned<const google::protobuf::ListValue> message_;
  const int size_;
  int index_ = 0;
};

class DynamicParsedJsonListValueIterator final : public ValueIterator {
 public:
  explicit DynamicParsedJsonListValueIterator(
      Owned<const google::protobuf::Message> message)
      : message_(std::move(message)),
        reflection_(well_known_types::GetListValueReflectionOrDie(
            message_->GetDescriptor())),
        size_(reflection_.ValuesSize(*message_)) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(ValueManager& value_manager, Value& result) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "`ValueIterator::Next` called after `ValueIterator::HasNext` "
          "returned false");
    }
    result = common_internal::ParsedJsonValue(
        value_manager.GetMemoryManager().arena(),
        Borrowed(message_, &reflection_.Values(*message_, index_)));
    ++index_;
    return absl::OkStatus();
  }

 private:
  const Owned<const google::protobuf::Message> message_;
  const well_known_types::ListValueReflection reflection_;
  const int size_;
  int index_ = 0;
};

}  // namespace

absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>>
ParsedJsonListValue::NewIterator(ValueManager& value_manager) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return NewEmptyValueIterator();
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return std::make_unique<GeneratedParsedJsonListValueIterator>(
        Owned(value_, generated_message));
  }
  return std::make_unique<DynamicParsedJsonListValueIterator>(
      Owned(value_,
            google::protobuf::DownCastMessage<google::protobuf::Message>(cel::to_address(value_))));
}

namespace {

absl::optional<internal::Number> AsNumber(const Value& value) {
  if (auto int_value = value.AsInt(); int_value) {
    return internal::Number::FromInt64(*int_value);
  }
  if (auto uint_value = value.AsUint(); uint_value) {
    return internal::Number::FromUint64(*uint_value);
  }
  if (auto double_value = value.AsDouble(); double_value) {
    return internal::Number::FromDouble(*double_value);
  }
  return absl::nullopt;
}

// Should only be called from `Contains()`.
absl::Status ContainsGenerated(
    Borrowed<const google::protobuf::ListValue> message,
    ValueManager& value_manager, const Value& other, Value& result) {
  // Other must be comparable to `null`, `double`, `string`, `list`, or `map`.
  if (other.IsNull()) {
    for (const auto& element :
         well_known_types::ListValueReflection::Values(*message)) {
      const auto element_kind_case =
          well_known_types::ValueReflection::GetKindCase(element);
      if (element_kind_case == google::protobuf::Value::KIND_NOT_SET ||
          element_kind_case == google::protobuf::Value::kNullValue) {
        result = BoolValue(true);
        return absl::OkStatus();
      }
    }
  } else if (const auto other_value = other.AsBool(); other_value) {
    for (const auto& element :
         well_known_types::ListValueReflection::Values(*message)) {
      if (well_known_types::ValueReflection::GetKindCase(element) ==
              google::protobuf::Value::kBoolValue &&
          well_known_types::ValueReflection::GetBoolValue(element) ==
              *other_value) {
        result = BoolValue(true);
        return absl::OkStatus();
      }
    }
  } else if (const auto other_value = AsNumber(other); other_value) {
    for (const auto& element :
         well_known_types::ListValueReflection::Values(*message)) {
      if (well_known_types::ValueReflection::GetKindCase(element) ==
              google::protobuf::Value::kNumberValue &&
          internal::Number::FromDouble(
              well_known_types::ValueReflection::GetNumberValue(element)) ==
              *other_value) {
        result = BoolValue(true);
        return absl::OkStatus();
      }
    }
  } else if (const auto other_value = other.AsString(); other_value) {
    for (const auto& element :
         well_known_types::ListValueReflection::Values(*message)) {
      if (well_known_types::ValueReflection::GetKindCase(element) ==
              google::protobuf::Value::kStringValue &&
          well_known_types::ValueReflection::GetStringValue(element) ==
              *other_value) {
        result = BoolValue(true);
        return absl::OkStatus();
      }
    }
  } else if (const auto other_value = other.AsList(); other_value) {
    for (const auto& element :
         well_known_types::ListValueReflection::Values(*message)) {
      if (well_known_types::ValueReflection::GetKindCase(element) ==
          google::protobuf::Value::kListValue) {
        CEL_RETURN_IF_ERROR(other_value->Equal(
            value_manager,
            ParsedJsonListValue(Owned(Owner(message), &element.list_value())),
            result));
        if (result.IsTrue()) {
          return absl::OkStatus();
        }
      }
    }
  } else if (const auto other_value = other.AsMap(); other_value) {
    for (const auto& element :
         well_known_types::ListValueReflection::Values(*message)) {
      if (well_known_types::ValueReflection::GetKindCase(element) ==
          google::protobuf::Value::kStructValue) {
        CEL_RETURN_IF_ERROR(other_value->Equal(
            value_manager,
            ParsedJsonMapValue(Owned(Owner(message), &element.struct_value())),
            result));
        if (result.IsTrue()) {
          return absl::OkStatus();
        }
      }
    }
  }
  result = BoolValue(false);
  return absl::OkStatus();
}

// Should only be called from `Contains()`.
absl::Status ContainsDynamic(Borrowed<const google::protobuf::Message> message,
                             ValueManager& value_manager, const Value& other,
                             Value& result) {
  // Other must be comparable to `null`, `double`, `string`, `list`, or `map`.
  const auto reflection =
      well_known_types::GetListValueReflectionOrDie(message->GetDescriptor());
  if (reflection.ValuesSize(*message) > 0) {
    const auto value_reflection = well_known_types::GetValueReflectionOrDie(
        reflection.GetValueDescriptor());
    if (other.IsNull()) {
      for (const auto& element : reflection.Values(*message)) {
        const auto element_kind_case = value_reflection.GetKindCase(element);
        if (element_kind_case == google::protobuf::Value::KIND_NOT_SET ||
            element_kind_case == google::protobuf::Value::kNullValue) {
          result = BoolValue(true);
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = other.AsBool(); other_value) {
      for (const auto& element : reflection.Values(*message)) {
        if (value_reflection.GetKindCase(element) ==
                google::protobuf::Value::kBoolValue &&
            value_reflection.GetBoolValue(element) == *other_value) {
          result = BoolValue(true);
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = AsNumber(other); other_value) {
      for (const auto& element : reflection.Values(*message)) {
        if (value_reflection.GetKindCase(element) ==
                google::protobuf::Value::kNumberValue &&
            internal::Number::FromDouble(
                value_reflection.GetNumberValue(element)) == *other_value) {
          result = BoolValue(true);
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = other.AsString(); other_value) {
      std::string scratch;
      for (const auto& element : reflection.Values(*message)) {
        if (value_reflection.GetKindCase(element) ==
                google::protobuf::Value::kStringValue &&
            absl::visit(
                [&](const auto& alternative) -> bool {
                  return *other_value == alternative;
                },
                well_known_types::AsVariant(
                    value_reflection.GetStringValue(element, scratch)))) {
          result = BoolValue(true);
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = other.AsList(); other_value) {
      for (const auto& element : reflection.Values(*message)) {
        if (value_reflection.GetKindCase(element) ==
            google::protobuf::Value::kListValue) {
          CEL_RETURN_IF_ERROR(other_value->Equal(
              value_manager,
              ParsedJsonListValue(Owned(
                  Owner(message), &value_reflection.GetListValue(element))),
              result));
          if (result.IsTrue()) {
            return absl::OkStatus();
          }
        }
      }
    } else if (const auto other_value = other.AsMap(); other_value) {
      for (const auto& element : reflection.Values(*message)) {
        if (value_reflection.GetKindCase(element) ==
            google::protobuf::Value::kStructValue) {
          CEL_RETURN_IF_ERROR(other_value->Equal(
              value_manager,
              ParsedJsonMapValue(Owned(
                  Owner(message), &value_reflection.GetStructValue(element))),
              result));
          if (result.IsTrue()) {
            return absl::OkStatus();
          }
        }
      }
    }
  }
  result = BoolValue(false);
  return absl::OkStatus();
}

}  // namespace

absl::Status ParsedJsonListValue::Contains(ValueManager& value_manager,
                                           const Value& other,
                                           Value& result) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    result = BoolValue(false);
    return absl::OkStatus();
  }
  if (ABSL_PREDICT_FALSE(other.IsError() || other.IsUnknown())) {
    result = other;
    return absl::OkStatus();
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return ContainsGenerated(Borrowed(value_, generated_message), value_manager,
                             other, result);
  }
  return ContainsDynamic(
      Borrowed(value_, google::protobuf::DownCastMessage<google::protobuf::Message>(
                           cel::to_address(value_))),
      value_manager, other, result);
}

absl::StatusOr<Value> ParsedJsonListValue::Contains(ValueManager& value_manager,
                                                    const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Contains(value_manager, other, result));
  return result;
}

bool operator==(const ParsedJsonListValue& lhs,
                const ParsedJsonListValue& rhs) {
  if (cel::to_address(lhs.value_) == cel::to_address(rhs.value_)) {
    return true;
  }
  if (cel::to_address(lhs.value_) == nullptr ||
      cel::to_address(rhs.value_) == nullptr) {
    return false;
  }
  return internal::JsonListEquals(*lhs.value_, *rhs.value_);
}

}  // namespace cel

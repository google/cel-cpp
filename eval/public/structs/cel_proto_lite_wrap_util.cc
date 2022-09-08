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

#include "eval/public/structs/cel_proto_lite_wrap_util.h"

#include <math.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/wrappers.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "eval/public/cel_value.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/casts.h"
#include "internal/overflow.h"
#include "internal/proto_time_encoding.h"

namespace google::api::expr::runtime::internal {

namespace {

using cel::internal::DecodeDuration;
using cel::internal::DecodeTime;
using cel::internal::EncodeTime;
using google::protobuf::Any;
using google::protobuf::BoolValue;
using google::protobuf::BytesValue;
using google::protobuf::DoubleValue;
using google::protobuf::Duration;
using google::protobuf::FloatValue;
using google::protobuf::Int32Value;
using google::protobuf::Int64Value;
using google::protobuf::ListValue;
using google::protobuf::StringValue;
using google::protobuf::Struct;
using google::protobuf::Timestamp;
using google::protobuf::UInt32Value;
using google::protobuf::UInt64Value;
using google::protobuf::Value;
using google::protobuf::Arena;

// kMaxIntJSON is defined as the Number.MAX_SAFE_INTEGER value per EcmaScript 6.
constexpr int64_t kMaxIntJSON = (1ll << 53) - 1;

// kMinIntJSON is defined as the Number.MIN_SAFE_INTEGER value per EcmaScript 6.
constexpr int64_t kMinIntJSON = -kMaxIntJSON;

// Supported well known types.
typedef enum {
  kUnknown,
  kBoolValue,
  kDoubleValue,
  kFloatValue,
  kInt32Value,
  kInt64Value,
  kUInt32Value,
  kUInt64Value,
  kDuration,
  kTimestamp,
  kStruct,
  kListValue,
  kValue,
  kStringValue,
  kBytesValue,
  kAny
} WellKnownType;

// GetWellKnownType translates a string type name into a WellKnowType.
WellKnownType GetWellKnownType(absl::string_view type_name) {
  static auto* well_known_types_map =
      new absl::flat_hash_map<std::string, WellKnownType>(
          {{"google.protobuf.BoolValue", kBoolValue},
           {"google.protobuf.DoubleValue", kDoubleValue},
           {"google.protobuf.FloatValue", kFloatValue},
           {"google.protobuf.Int32Value", kInt32Value},
           {"google.protobuf.Int64Value", kInt64Value},
           {"google.protobuf.UInt32Value", kUInt32Value},
           {"google.protobuf.UInt64Value", kUInt64Value},
           {"google.protobuf.Duration", kDuration},
           {"google.protobuf.Timestamp", kTimestamp},
           {"google.protobuf.Struct", kStruct},
           {"google.protobuf.ListValue", kListValue},
           {"google.protobuf.Value", kValue},
           {"google.protobuf.StringValue", kStringValue},
           {"google.protobuf.BytesValue", kBytesValue},
           {"google.protobuf.Any", kAny}});
  if (!well_known_types_map->contains(type_name)) {
    return kUnknown;
  }
  return well_known_types_map->at(type_name);
}

// IsJSONSafe indicates whether the int is safely representable as a floating
// point value in JSON.
static bool IsJSONSafe(int64_t i) {
  return i >= kMinIntJSON && i <= kMaxIntJSON;
}

// IsJSONSafe indicates whether the uint is safely representable as a floating
// point value in JSON.
static bool IsJSONSafe(uint64_t i) {
  return i <= static_cast<uint64_t>(kMaxIntJSON);
}

// Map implementation wrapping google.protobuf.ListValue
class DynamicList : public CelList {
 public:
  DynamicList(const ListValue* values, const LegacyTypeInfoApis* type_info,
              Arena* arena)
      : arena_(arena), type_info_(type_info), values_(values) {}

  CelValue operator[](int index) const override;

  // List size
  int size() const override { return values_->values_size(); }

 private:
  Arena* arena_;
  const LegacyTypeInfoApis* type_info_;
  const ListValue* values_;
};

// Map implementation wrapping google.protobuf.Struct.
class DynamicMap : public CelMap {
 public:
  DynamicMap(const Struct* values, const LegacyTypeInfoApis* type_info,
             Arena* arena)
      : arena_(arena),
        values_(values),
        type_info_(type_info),
        key_list_(values) {}

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    CelValue::StringHolder str_key;
    if (!key.GetValue(&str_key)) {
      // Not a string key.
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid map key type: '", CelValue::TypeName(key.type()), "'"));
    }

    return values_->fields().contains(std::string(str_key.value()));
  }

  absl::optional<CelValue> operator[](CelValue key) const override;

  int size() const override { return values_->fields_size(); }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return &key_list_;
  }

 private:
  // List of keys in Struct.fields map.
  // It utilizes lazy initialization, to avoid performance penalties.
  class DynamicMapKeyList : public CelList {
   public:
    explicit DynamicMapKeyList(const Struct* values)
        : values_(values), keys_(), initialized_(false) {}

    // Index access
    CelValue operator[](int index) const override {
      CheckInit();
      return keys_[index];
    }

    // List size
    int size() const override {
      CheckInit();
      return values_->fields_size();
    }

   private:
    void CheckInit() const {
      absl::MutexLock lock(&mutex_);
      if (!initialized_) {
        for (const auto& it : values_->fields()) {
          keys_.push_back(CelValue::CreateString(&it.first));
        }
        initialized_ = true;
      }
    }

    const Struct* values_;
    mutable absl::Mutex mutex_;
    mutable std::vector<CelValue> keys_;
    mutable bool initialized_;
  };

  Arena* arena_;
  const Struct* values_;
  const LegacyTypeInfoApis* type_info_;
  const DynamicMapKeyList key_list_;
};
}  // namespace

CelValue CreateCelValue(const Duration& duration,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateDuration(DecodeDuration(duration));
}

CelValue CreateCelValue(const Timestamp& timestamp,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateTimestamp(DecodeTime(timestamp));
}

CelValue CreateCelValue(const ListValue& list_values,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateList(
      Arena::Create<DynamicList>(arena, &list_values, type_info, arena));
}

CelValue CreateCelValue(const Struct& struct_value,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateMap(
      Arena::Create<DynamicMap>(arena, &struct_value, type_info, arena));
}

CelValue CreateCelValue(const Any& any_value,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  auto type_url = any_value.type_url();
  auto pos = type_url.find_last_of('/');
  if (pos == absl::string_view::npos) {
    // TODO(issues/25) What error code?
    // Malformed type_url
    return CreateErrorValue(arena, "Malformed type_url string");
  }

  std::string full_name = std::string(type_url.substr(pos + 1));
  WellKnownType type = GetWellKnownType(full_name);
  switch (type) {
    case kDoubleValue: {
      DoubleValue* nested_message = Arena::CreateMessage<DoubleValue>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into DoubleValue");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kFloatValue: {
      FloatValue* nested_message = Arena::CreateMessage<FloatValue>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into FloatValue");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kInt32Value: {
      Int32Value* nested_message = Arena::CreateMessage<Int32Value>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into Int32Value");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kInt64Value: {
      Int64Value* nested_message = Arena::CreateMessage<Int64Value>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into Int64Value");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kUInt32Value: {
      UInt32Value* nested_message = Arena::CreateMessage<UInt32Value>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into UInt32Value");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kUInt64Value: {
      UInt64Value* nested_message = Arena::CreateMessage<UInt64Value>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into UInt64Value");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kBoolValue: {
      BoolValue* nested_message = Arena::CreateMessage<BoolValue>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into BoolValue");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kTimestamp: {
      Timestamp* nested_message = Arena::CreateMessage<Timestamp>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into Timestamp");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kDuration: {
      Duration* nested_message = Arena::CreateMessage<Duration>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into Duration");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kStringValue: {
      StringValue* nested_message = Arena::CreateMessage<StringValue>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into StringValue");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kBytesValue: {
      BytesValue* nested_message = Arena::CreateMessage<BytesValue>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into BytesValue");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kListValue: {
      ListValue* nested_message = Arena::CreateMessage<ListValue>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into ListValue");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kStruct: {
      Struct* nested_message = Arena::CreateMessage<Struct>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into Struct");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kValue: {
      Value* nested_message = Arena::CreateMessage<Value>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into Value");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kAny: {
      Any* nested_message = Arena::CreateMessage<Any>(arena);
      if (!any_value.UnpackTo(nested_message)) {
        // Failed to unpack.
        // TODO(issues/25) What error code?
        return CreateErrorValue(arena, "Failed to unpack Any into Any");
      }
      return CreateCelValue(*nested_message, type_info, arena);
    } break;
    case kUnknown:
      return CreateErrorValue(
          arena, "Unpacking of type " + full_name + " is not supported.");
  }
}

CelValue CreateCelValue(bool value, const LegacyTypeInfoApis* type_info,
                        Arena* arena) {
  return CelValue::CreateBool(value);
}

CelValue CreateCelValue(int32_t value, const LegacyTypeInfoApis* type_info,
                        Arena* arena) {
  return CelValue::CreateInt64(value);
}

CelValue CreateCelValue(int64_t value, const LegacyTypeInfoApis* type_info,
                        Arena* arena) {
  return CelValue::CreateInt64(value);
}

CelValue CreateCelValue(uint32_t value, const LegacyTypeInfoApis* type_info,
                        Arena* arena) {
  return CelValue::CreateUint64(value);
}

CelValue CreateCelValue(uint64_t value, const LegacyTypeInfoApis* type_info,
                        Arena* arena) {
  return CelValue::CreateUint64(value);
}

CelValue CreateCelValue(float value, const LegacyTypeInfoApis* type_info,
                        Arena* arena) {
  return CelValue::CreateDouble(value);
}

CelValue CreateCelValue(double value, const LegacyTypeInfoApis* type_info,
                        Arena* arena) {
  return CelValue::CreateDouble(value);
}

CelValue CreateCelValue(const std::string& value,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateString(&value);
}

CelValue CreateCelValue(const absl::Cord& value,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateBytes(Arena::Create<std::string>(arena, value));
}

CelValue CreateCelValue(const BoolValue& wrapper,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateBool(wrapper.value());
}

CelValue CreateCelValue(const Int32Value& wrapper,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateInt64(wrapper.value());
}

CelValue CreateCelValue(const UInt32Value& wrapper,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateUint64(wrapper.value());
}

CelValue CreateCelValue(const Int64Value& wrapper,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateInt64(wrapper.value());
}

CelValue CreateCelValue(const UInt64Value& wrapper,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateUint64(wrapper.value());
}

CelValue CreateCelValue(const FloatValue& wrapper,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateDouble(wrapper.value());
}

CelValue CreateCelValue(const DoubleValue& wrapper,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateDouble(wrapper.value());
}

CelValue CreateCelValue(const StringValue& wrapper,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  return CelValue::CreateString(&wrapper.value());
}

CelValue CreateCelValue(const BytesValue& wrapper,
                        const LegacyTypeInfoApis* type_info, Arena* arena) {
  // BytesValue stores value as Cord
  return CelValue::CreateBytes(
      Arena::Create<std::string>(arena, std::string(wrapper.value())));
}

CelValue CreateCelValue(const Value& value, const LegacyTypeInfoApis* type_info,
                        Arena* arena) {
  switch (value.kind_case()) {
    case Value::KindCase::kNullValue:
      return CelValue::CreateNull();
    case Value::KindCase::kNumberValue:
      return CelValue::CreateDouble(value.number_value());
    case Value::KindCase::kStringValue:
      return CelValue::CreateString(&value.string_value());
    case Value::KindCase::kBoolValue:
      return CelValue::CreateBool(value.bool_value());
    case Value::KindCase::kStructValue:
      return CreateCelValue(value.struct_value(), type_info, arena);
    case Value::KindCase::kListValue:
      return CreateCelValue(value.list_value(), type_info, arena);
    default:
      return CelValue::CreateNull();
  }
}

CelValue DynamicList::operator[](int index) const {
  return CreateCelValue(values_->values(index), type_info_, arena_);
}

absl::optional<CelValue> DynamicMap::operator[](CelValue key) const {
  CelValue::StringHolder str_key;
  if (!key.GetValue(&str_key)) {
    // Not a string key.
    return CreateErrorValue(arena_, absl::InvalidArgumentError(absl::StrCat(
                                        "Invalid map key type: '",
                                        CelValue::TypeName(key.type()), "'")));
  }

  auto it = values_->fields().find(std::string(str_key.value()));
  if (it == values_->fields().end()) {
    return absl::nullopt;
  }

  return CreateCelValue(it->second, type_info_, arena_);
}

absl::StatusOr<CelValue> UnwrapFromWellKnownType(
    const google::protobuf::MessageLite* message, const LegacyTypeInfoApis* type_info,
    Arena* arena) {
  if (message == nullptr) {
    return CelValue::CreateNull();
  }
  WellKnownType type = GetWellKnownType(message->GetTypeName());
  switch (type) {
    case kDoubleValue: {
      auto value =
          cel::internal::down_cast<const google::protobuf::DoubleValue*>(
              message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kFloatValue: {
      auto value =
          cel::internal::down_cast<const google::protobuf::FloatValue*>(
              message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kInt32Value: {
      auto value =
          cel::internal::down_cast<const google::protobuf::Int32Value*>(
              message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kInt64Value: {
      auto value =
          cel::internal::down_cast<const google::protobuf::Int64Value*>(
              message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kUInt32Value: {
      auto value =
          cel::internal::down_cast<const google::protobuf::UInt32Value*>(
              message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kUInt64Value: {
      auto value =
          cel::internal::down_cast<const google::protobuf::UInt64Value*>(
              message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kBoolValue: {
      auto value =
          cel::internal::down_cast<const google::protobuf::BoolValue*>(message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kTimestamp: {
      auto value =
          cel::internal::down_cast<const google::protobuf::Timestamp*>(message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kDuration: {
      auto value =
          cel::internal::down_cast<const google::protobuf::Duration*>(message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kStruct: {
      auto value =
          cel::internal::down_cast<const google::protobuf::Struct*>(message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kListValue: {
      auto value =
          cel::internal::down_cast<const google::protobuf::ListValue*>(message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kValue: {
      auto value =
          cel::internal::down_cast<const google::protobuf::Value*>(message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kStringValue: {
      auto value =
          cel::internal::down_cast<const google::protobuf::StringValue*>(
              message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kBytesValue: {
      auto value =
          cel::internal::down_cast<const google::protobuf::BytesValue*>(
              message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kAny: {
      auto value =
          cel::internal::down_cast<const google::protobuf::Any*>(message);
      return CreateCelValue(*value, type_info, arena);
    } break;
    case kUnknown:
      return absl::NotFoundError(message->GetTypeName() +
                                 " is not well known type.");
  }
}

absl::StatusOr<Duration*> CreateMessageFromValue(const CelValue& cel_value,
                                                 Duration* wrapper,
                                                 google::protobuf::Arena* arena) {
  absl::Duration val;
  if (!cel_value.GetValue(&val)) {
    return absl::InternalError("cel_value is expected to have Duration type.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<Duration>(arena);
  }
  absl::Status status = cel::internal::EncodeDuration(val, wrapper);
  if (!status.ok()) {
    return status;
  }
  return wrapper;
}

absl::StatusOr<BoolValue*> CreateMessageFromValue(const CelValue& cel_value,
                                                  BoolValue* wrapper,
                                                  google::protobuf::Arena* arena) {
  bool val;
  if (!cel_value.GetValue(&val)) {
    return absl::InternalError("cel_value is expected to have Bool type.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<BoolValue>(arena);
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::StatusOr<BytesValue*> CreateMessageFromValue(const CelValue& cel_value,
                                                   BytesValue* wrapper,
                                                   google::protobuf::Arena* arena) {
  CelValue::BytesHolder view_val;
  if (!cel_value.GetValue(&view_val)) {
    return absl::InternalError("cel_value is expected to have Bytes type.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<BytesValue>(arena);
  }
  wrapper->set_value(view_val.value().data(), view_val.value().size());
  return wrapper;
}

absl::StatusOr<DoubleValue*> CreateMessageFromValue(const CelValue& cel_value,
                                                    DoubleValue* wrapper,
                                                    google::protobuf::Arena* arena) {
  double val;
  if (!cel_value.GetValue(&val)) {
    return absl::InternalError("cel_value is expected to have Double type.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<DoubleValue>(arena);
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::StatusOr<FloatValue*> CreateMessageFromValue(const CelValue& cel_value,
                                                   FloatValue* wrapper,
                                                   google::protobuf::Arena* arena) {
  double val;
  if (!cel_value.GetValue(&val)) {
    return absl::InternalError("cel_value is expected to have Double type.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<FloatValue>(arena);
  }
  // Abort the conversion if the value is outside the float range.
  if (val > std::numeric_limits<float>::max()) {
    wrapper->set_value(std::numeric_limits<float>::infinity());
    return wrapper;
  }
  if (val < std::numeric_limits<float>::lowest()) {
    wrapper->set_value(-std::numeric_limits<float>::infinity());
    return wrapper;
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::StatusOr<Int32Value*> CreateMessageFromValue(const CelValue& cel_value,
                                                   Int32Value* wrapper,
                                                   google::protobuf::Arena* arena) {
  int64_t val;
  if (!cel_value.GetValue(&val)) {
    return absl::InternalError("cel_value is expected to have Int64 type.");
  }
  // Abort the conversion if the value is outside the int32_t range.
  if (!cel::internal::CheckedInt64ToInt32(val).ok()) {
    return absl::InternalError(
        "Integer overflow on Int32 to Int64 conversion.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<Int32Value>(arena);
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::StatusOr<Int64Value*> CreateMessageFromValue(const CelValue& cel_value,
                                                   Int64Value* wrapper,
                                                   google::protobuf::Arena* arena) {
  int64_t val;
  if (!cel_value.GetValue(&val)) {
    return absl::InternalError("cel_value is expected to have Int64 type.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<Int64Value>(arena);
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::StatusOr<StringValue*> CreateMessageFromValue(const CelValue& cel_value,
                                                    StringValue* wrapper,
                                                    google::protobuf::Arena* arena) {
  CelValue::StringHolder view_val;
  if (!cel_value.GetValue(&view_val)) {
    return absl::InternalError("cel_value is expected to have String type.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<StringValue>(arena);
  }
  wrapper->set_value(view_val.value().data(), view_val.value().size());
  return wrapper;
}

absl::StatusOr<Timestamp*> CreateMessageFromValue(const CelValue& cel_value,
                                                  Timestamp* wrapper,
                                                  google::protobuf::Arena* arena) {
  absl::Time val;
  if (!cel_value.GetValue(&val)) {
    return absl::InternalError("cel_value is expected to have Timestamp type.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<Timestamp>(arena);
  }
  absl::Status status = EncodeTime(val, wrapper);
  if (!status.ok()) {
    return status;
  }
  return wrapper;
}

absl::StatusOr<UInt32Value*> CreateMessageFromValue(const CelValue& cel_value,
                                                    UInt32Value* wrapper,
                                                    google::protobuf::Arena* arena) {
  uint64_t val;
  if (!cel_value.GetValue(&val)) {
    return absl::InternalError("cel_value is expected to have UInt64 type.");
  }
  // Abort the conversion if the value is outside the int32_t range.
  if (!cel::internal::CheckedUint64ToUint32(val).ok()) {
    return absl::InternalError(
        "Integer overflow on UInt32 to UInt64 conversion.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<UInt32Value>(arena);
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::StatusOr<UInt64Value*> CreateMessageFromValue(const CelValue& cel_value,
                                                    UInt64Value* wrapper,
                                                    google::protobuf::Arena* arena) {
  uint64_t val;
  if (!cel_value.GetValue(&val)) {
    return absl::InternalError("cel_value is expected to have UInt64 type.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<UInt64Value>(arena);
  }
  wrapper->set_value(val);
  return wrapper;
}

absl::StatusOr<ListValue*> CreateMessageFromValue(const CelValue& cel_value,
                                                  ListValue* wrapper,
                                                  google::protobuf::Arena* arena) {
  if (!cel_value.IsList()) {
    return absl::InternalError("cel_value is expected to have List type.");
  }
  const google::api::expr::runtime::CelList& list = *cel_value.ListOrDie();
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<ListValue>(arena);
  }
  for (int i = 0; i < list.size(); i++) {
    auto element = list[i];
    Value* element_value = nullptr;
    CEL_ASSIGN_OR_RETURN(element_value,
                         CreateMessageFromValue(element, element_value, arena));
    if (element_value == nullptr) {
      return absl::InternalError("Couldn't create value for a list element.");
    }
    wrapper->add_values()->Swap(element_value);
  }
  return wrapper;
}

absl::StatusOr<Struct*> CreateMessageFromValue(const CelValue& cel_value,
                                               Struct* wrapper,
                                               google::protobuf::Arena* arena) {
  if (!cel_value.IsMap()) {
    return absl::InternalError("cel_value is expected to have Map type.");
  }
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<Struct>(arena);
  }
  const google::api::expr::runtime::CelMap& map = *cel_value.MapOrDie();
  const auto& keys = *map.ListKeys().value();
  auto fields = wrapper->mutable_fields();
  for (int i = 0; i < keys.size(); i++) {
    auto k = keys[i];
    // If the key is not a string type, abort the conversion.
    if (!k.IsString()) {
      return absl::InternalError("map key is expected to have String type.");
    }
    std::string key(k.StringOrDie().value());

    auto v = map[k];
    if (!v.has_value()) {
      return absl::InternalError("map value is expected to have value.");
    }
    Value* field_value = nullptr;
    CEL_ASSIGN_OR_RETURN(field_value,
                         CreateMessageFromValue(v.value(), field_value, arena));
    if (field_value == nullptr) {
      return absl::InternalError("Couldn't create value for a field element.");
    }
    (*fields)[key].Swap(field_value);
  }
  return wrapper;
}

absl::StatusOr<Value*> CreateMessageFromValue(const CelValue& cel_value,
                                              Value* wrapper,
                                              google::protobuf::Arena* arena) {
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<Value>(arena);
  }
  CelValue::Type type = cel_value.type();
  switch (type) {
    case CelValue::Type::kBool: {
      bool val;
      if (cel_value.GetValue(&val)) {
        wrapper->set_bool_value(val);
      }
    } break;
    case CelValue::Type::kBytes: {
      // Base64 encode byte strings to ensure they can safely be transpored
      // in a JSON string.
      CelValue::BytesHolder val;
      if (cel_value.GetValue(&val)) {
        wrapper->set_string_value(absl::Base64Escape(val.value()));
      }
    } break;
    case CelValue::Type::kDouble: {
      double val;
      if (cel_value.GetValue(&val)) {
        wrapper->set_number_value(val);
      }
    } break;
    case CelValue::Type::kDuration: {
      // Convert duration values to a protobuf JSON format.
      absl::Duration val;
      if (cel_value.GetValue(&val)) {
        auto encode = cel::internal::EncodeDurationToString(val);
        if (!encode.ok()) {
          return encode.status();
        }
        wrapper->set_string_value(*encode);
      }
    } break;
    case CelValue::Type::kInt64: {
      int64_t val;
      // Convert int64_t values within the int53 range to doubles, otherwise
      // serialize the value to a string.
      if (cel_value.GetValue(&val)) {
        if (IsJSONSafe(val)) {
          wrapper->set_number_value(val);
        } else {
          wrapper->set_string_value(absl::StrCat(val));
        }
      }
    } break;
    case CelValue::Type::kString: {
      CelValue::StringHolder val;
      if (cel_value.GetValue(&val)) {
        wrapper->set_string_value(val.value().data(), val.value().size());
      }
    } break;
    case CelValue::Type::kTimestamp: {
      // Convert timestamp values to a protobuf JSON format.
      absl::Time val;
      if (cel_value.GetValue(&val)) {
        auto encode = cel::internal::EncodeTimeToString(val);
        if (!encode.ok()) {
          return encode.status();
        }
        wrapper->set_string_value(*encode);
      }
    } break;
    case CelValue::Type::kUint64: {
      uint64_t val;
      // Convert uint64_t values within the int53 range to doubles, otherwise
      // serialize the value to a string.
      if (cel_value.GetValue(&val)) {
        if (IsJSONSafe(val)) {
          wrapper->set_number_value(val);
        } else {
          wrapper->set_string_value(absl::StrCat(val));
        }
      }
    } break;
    case CelValue::Type::kList: {
      ListValue* list_wrapper = nullptr;
      CEL_ASSIGN_OR_RETURN(
          list_wrapper, CreateMessageFromValue(cel_value, list_wrapper, arena));
      wrapper->mutable_list_value()->Swap(list_wrapper);
    } break;
    case CelValue::Type::kMap: {
      Struct* struct_wrapper = nullptr;
      CEL_ASSIGN_OR_RETURN(
          struct_wrapper,
          CreateMessageFromValue(cel_value, struct_wrapper, arena));
      wrapper->mutable_struct_value()->Swap(struct_wrapper);
    } break;
    case CelValue::Type::kNullType:
      wrapper->set_null_value(google::protobuf::NULL_VALUE);
      break;
    default:
      return absl::InternalError(
          "Encoding CelValue of type " + CelValue::TypeName(type) +
          " into google::protobuf::Value is not supported.");
  }
  return wrapper;
}

absl::StatusOr<Any*> CreateMessageFromValue(const CelValue& cel_value,
                                            Any* wrapper,
                                            google::protobuf::Arena* arena) {
  if (wrapper == nullptr) {
    wrapper = google::protobuf::Arena::CreateMessage<Any>(arena);
  }
  CelValue::Type type = cel_value.type();
  // In open source, any->PackFrom() returns void rather than boolean.
  switch (type) {
    case CelValue::Type::kBool: {
      BoolValue* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    case CelValue::Type::kBytes: {
      BytesValue* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    case CelValue::Type::kDouble: {
      DoubleValue* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    case CelValue::Type::kDuration: {
      Duration* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    case CelValue::Type::kInt64: {
      Int64Value* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    case CelValue::Type::kString: {
      StringValue* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    case CelValue::Type::kTimestamp: {
      Timestamp* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    case CelValue::Type::kUint64: {
      UInt64Value* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    case CelValue::Type::kList: {
      ListValue* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    case CelValue::Type::kMap: {
      Struct* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    case CelValue::Type::kNullType: {
      Value* v = nullptr;
      CEL_ASSIGN_OR_RETURN(v, CreateMessageFromValue(cel_value, v, arena));
      wrapper->PackFrom(*v);
    } break;
    default:
      return absl::InternalError(
          "Packing CelValue of type " + CelValue::TypeName(type) +
          " into google::protobuf::Any is not supported.");
      break;
  }
  return wrapper;
}

}  // namespace google::api::expr::runtime::internal

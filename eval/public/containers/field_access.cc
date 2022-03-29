// Copyright 2021 Google LLC
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

#include "eval/public/containers/field_access.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/map_field.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "internal/casts.h"
#include "internal/overflow.h"

namespace google::api::expr::runtime {

namespace {

using ::google::protobuf::Arena;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::MapValueConstRef;
using ::google::protobuf::Message;
using ::google::protobuf::Reflection;

// Well-known type protobuf type names which require special get / set behavior.
constexpr absl::string_view kProtobufAny = "google.protobuf.Any";
constexpr absl::string_view kTypeGoogleApisComPrefix = "type.googleapis.com/";

// Singular message fields and repeated message fields have similar access model
// To provide common approach, we implement accessor classes, based on CRTP.
// FieldAccessor is CRTP base class, specifying Get.. method family.
template <class Derived>
class FieldAccessor {
 public:
  bool GetBool() const { return static_cast<const Derived*>(this)->GetBool(); }

  int64_t GetInt32() const {
    return static_cast<const Derived*>(this)->GetInt32();
  }

  uint64_t GetUInt32() const {
    return static_cast<const Derived*>(this)->GetUInt32();
  }

  int64_t GetInt64() const {
    return static_cast<const Derived*>(this)->GetInt64();
  }

  uint64_t GetUInt64() const {
    return static_cast<const Derived*>(this)->GetUInt64();
  }

  double GetFloat() const {
    return static_cast<const Derived*>(this)->GetFloat();
  }

  double GetDouble() const {
    return static_cast<const Derived*>(this)->GetDouble();
  }

  const std::string* GetString(std::string* buffer) const {
    return static_cast<const Derived*>(this)->GetString(buffer);
  }

  const Message* GetMessage() const {
    return static_cast<const Derived*>(this)->GetMessage();
  }

  int64_t GetEnumValue() const {
    return static_cast<const Derived*>(this)->GetEnumValue();
  }

  // This method provides message field content, wrapped in CelValue.
  // If value provided successfully, returns Ok.
  // arena Arena to use for allocations if needed.
  // result pointer to object to store value in.
  absl::Status CreateValueFromFieldAccessor(Arena* arena, CelValue* result) {
    switch (field_desc_->cpp_type()) {
      case FieldDescriptor::CPPTYPE_BOOL: {
        bool value = GetBool();
        *result = CelValue::CreateBool(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_INT32: {
        int64_t value = GetInt32();
        *result = CelValue::CreateInt64(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_INT64: {
        int64_t value = GetInt64();
        *result = CelValue::CreateInt64(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_UINT32: {
        uint64_t value = GetUInt32();
        *result = CelValue::CreateUint64(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_UINT64: {
        uint64_t value = GetUInt64();
        *result = CelValue::CreateUint64(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_FLOAT: {
        double value = GetFloat();
        *result = CelValue::CreateDouble(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_DOUBLE: {
        double value = GetDouble();
        *result = CelValue::CreateDouble(value);
        break;
      }
      case FieldDescriptor::CPPTYPE_STRING: {
        std::string buffer;
        const std::string* value = GetString(&buffer);
        if (value == &buffer) {
          value = google::protobuf::Arena::Create<std::string>(arena, std::move(buffer));
        }
        switch (field_desc_->type()) {
          case FieldDescriptor::TYPE_STRING:
            *result = CelValue::CreateString(value);
            break;
          case FieldDescriptor::TYPE_BYTES:
            *result = CelValue::CreateBytes(value);
            break;
          default:
            return absl::Status(absl::StatusCode::kInvalidArgument,
                                "Error handling C++ string conversion");
        }
        break;
      }
      case FieldDescriptor::CPPTYPE_MESSAGE: {
        const google::protobuf::Message* msg_value = GetMessage();
        *result = CelProtoWrapper::CreateMessage(msg_value, arena);
        break;
      }
      case FieldDescriptor::CPPTYPE_ENUM: {
        int enum_value = GetEnumValue();
        *result = CelValue::CreateInt64(enum_value);
        break;
      }
      default:
        return absl::Status(absl::StatusCode::kInvalidArgument,
                            "Unhandled C++ type conversion");
    }

    return absl::OkStatus();
  }

 protected:
  FieldAccessor(const Message* msg, const FieldDescriptor* field_desc)
      : msg_(msg), field_desc_(field_desc) {}

  const Message* msg_;
  const FieldDescriptor* field_desc_;
};

const absl::flat_hash_set<std::string>& WellKnownWrapperTypes() {
  static auto* wrapper_types = new absl::flat_hash_set<std::string>{
      "google.protobuf.BoolValue",   "google.protobuf.DoubleValue",
      "google.protobuf.FloatValue",  "google.protobuf.Int64Value",
      "google.protobuf.Int32Value",  "google.protobuf.UInt64Value",
      "google.protobuf.UInt32Value", "google.protobuf.StringValue",
      "google.protobuf.BytesValue",
  };
  return *wrapper_types;
}

bool IsWrapperType(const FieldDescriptor* field_descriptor) {
  return WellKnownWrapperTypes().find(
             field_descriptor->message_type()->full_name()) !=
         WellKnownWrapperTypes().end();
}

// Accessor class, to work with singular fields
class ScalarFieldAccessor : public FieldAccessor<ScalarFieldAccessor> {
 public:
  ScalarFieldAccessor(const Message* msg, const FieldDescriptor* field_desc,
                      bool unset_wrapper_as_null)
      : FieldAccessor(msg, field_desc),
        unset_wrapper_as_null_(unset_wrapper_as_null) {}

  bool GetBool() const { return GetReflection()->GetBool(*msg_, field_desc_); }

  int64_t GetInt32() const {
    return GetReflection()->GetInt32(*msg_, field_desc_);
  }

  uint64_t GetUInt32() const {
    return GetReflection()->GetUInt32(*msg_, field_desc_);
  }

  int64_t GetInt64() const {
    return GetReflection()->GetInt64(*msg_, field_desc_);
  }

  uint64_t GetUInt64() const {
    return GetReflection()->GetUInt64(*msg_, field_desc_);
  }

  double GetFloat() const {
    return GetReflection()->GetFloat(*msg_, field_desc_);
  }

  double GetDouble() const {
    return GetReflection()->GetDouble(*msg_, field_desc_);
  }

  const std::string* GetString(std::string* buffer) const {
    return &GetReflection()->GetStringReference(*msg_, field_desc_, buffer);
  }

  const Message* GetMessage() const {
    // Unset wrapper types have special semantics.
    // If set, return the unwrapped value, else return 'null'.
    if (unset_wrapper_as_null_ &&
        !GetReflection()->HasField(*msg_, field_desc_) &&
        IsWrapperType(field_desc_)) {
      return nullptr;
    }
    return &GetReflection()->GetMessage(*msg_, field_desc_);
  }

  int64_t GetEnumValue() const {
    return GetReflection()->GetEnumValue(*msg_, field_desc_);
  }

  const Reflection* GetReflection() const { return msg_->GetReflection(); }

 private:
  bool unset_wrapper_as_null_;
};

// Accessor class, to work with repeated fields.
class RepeatedFieldAccessor : public FieldAccessor<RepeatedFieldAccessor> {
 public:
  RepeatedFieldAccessor(const Message* msg, const FieldDescriptor* field_desc,
                        int index)
      : FieldAccessor(msg, field_desc), index_(index) {}

  bool GetBool() const {
    return GetReflection()->GetRepeatedBool(*msg_, field_desc_, index_);
  }

  int64_t GetInt32() const {
    return GetReflection()->GetRepeatedInt32(*msg_, field_desc_, index_);
  }

  uint64_t GetUInt32() const {
    return GetReflection()->GetRepeatedUInt32(*msg_, field_desc_, index_);
  }

  int64_t GetInt64() const {
    return GetReflection()->GetRepeatedInt64(*msg_, field_desc_, index_);
  }

  uint64_t GetUInt64() const {
    return GetReflection()->GetRepeatedUInt64(*msg_, field_desc_, index_);
  }

  double GetFloat() const {
    return GetReflection()->GetRepeatedFloat(*msg_, field_desc_, index_);
  }

  double GetDouble() const {
    return GetReflection()->GetRepeatedDouble(*msg_, field_desc_, index_);
  }

  const std::string* GetString(std::string* buffer) const {
    return &GetReflection()->GetRepeatedStringReference(*msg_, field_desc_,
                                                        index_, buffer);
  }

  const Message* GetMessage() const {
    return &GetReflection()->GetRepeatedMessage(*msg_, field_desc_, index_);
  }

  int64_t GetEnumValue() const {
    return GetReflection()->GetRepeatedEnumValue(*msg_, field_desc_, index_);
  }

  const Reflection* GetReflection() const { return msg_->GetReflection(); }

 private:
  int index_;
};

// Accessor class, to work with map values
class MapValueAccessor : public FieldAccessor<MapValueAccessor> {
 public:
  MapValueAccessor(const Message* msg, const FieldDescriptor* field_desc,
                   const MapValueConstRef* value_ref)
      : FieldAccessor(msg, field_desc), value_ref_(value_ref) {}

  bool GetBool() const { return value_ref_->GetBoolValue(); }

  int64_t GetInt32() const { return value_ref_->GetInt32Value(); }

  uint64_t GetUInt32() const { return value_ref_->GetUInt32Value(); }

  int64_t GetInt64() const { return value_ref_->GetInt64Value(); }

  uint64_t GetUInt64() const { return value_ref_->GetUInt64Value(); }

  double GetFloat() const { return value_ref_->GetFloatValue(); }

  double GetDouble() const { return value_ref_->GetDoubleValue(); }

  const std::string* GetString(std::string* /*buffer*/) const {
    return &value_ref_->GetStringValue();
  }

  const Message* GetMessage() const { return &value_ref_->GetMessageValue(); }

  int64_t GetEnumValue() const { return value_ref_->GetEnumValue(); }

  const Reflection* GetReflection() const { return msg_->GetReflection(); }

 private:
  const MapValueConstRef* value_ref_;
};

}  // namespace

absl::Status CreateValueFromSingleField(const google::protobuf::Message* msg,
                                        const FieldDescriptor* desc,
                                        google::protobuf::Arena* arena,
                                        CelValue* result) {
  return CreateValueFromSingleField(
      msg, desc, ProtoWrapperTypeOptions::kUnsetProtoDefault, arena, result);
}

absl::Status CreateValueFromSingleField(const google::protobuf::Message* msg,
                                        const FieldDescriptor* desc,
                                        ProtoWrapperTypeOptions options,
                                        google::protobuf::Arena* arena,
                                        CelValue* result) {
  ScalarFieldAccessor accessor(
      msg, desc, (options == ProtoWrapperTypeOptions::kUnsetNull));
  return accessor.CreateValueFromFieldAccessor(arena, result);
}

absl::Status CreateValueFromRepeatedField(const google::protobuf::Message* msg,
                                          const FieldDescriptor* desc,
                                          google::protobuf::Arena* arena, int index,
                                          CelValue* result) {
  RepeatedFieldAccessor accessor(msg, desc, index);
  return accessor.CreateValueFromFieldAccessor(arena, result);
}

absl::Status CreateValueFromMapValue(const google::protobuf::Message* msg,
                                     const FieldDescriptor* desc,
                                     const MapValueConstRef* value_ref,
                                     google::protobuf::Arena* arena, CelValue* result) {
  MapValueAccessor accessor(msg, desc, value_ref);
  return accessor.CreateValueFromFieldAccessor(arena, result);
}

// Singular message fields and repeated message fields have similar access model
// To provide common approach, we implement field setter classes, based on CRTP.
// FieldAccessor is CRTP base class, specifying Get.. method family.
template <class Derived>
class FieldSetter {
 public:
  bool AssignBool(const CelValue& cel_value) const {
    bool value;

    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetBool(value);
    return true;
  }

  bool AssignInt32(const CelValue& cel_value) const {
    int64_t value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    if (!cel::internal::CheckedInt64ToInt32(value).ok()) {
      return false;
    }
    static_cast<const Derived*>(this)->SetInt32(value);
    return true;
  }

  bool AssignUInt32(const CelValue& cel_value) const {
    uint64_t value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    if (!cel::internal::CheckedUint64ToUint32(value).ok()) {
      return false;
    }
    static_cast<const Derived*>(this)->SetUInt32(value);
    return true;
  }

  bool AssignInt64(const CelValue& cel_value) const {
    int64_t value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetInt64(value);
    return true;
  }

  bool AssignUInt64(const CelValue& cel_value) const {
    uint64_t value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetUInt64(value);
    return true;
  }

  bool AssignFloat(const CelValue& cel_value) const {
    double value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetFloat(value);
    return true;
  }

  bool AssignDouble(const CelValue& cel_value) const {
    double value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetDouble(value);
    return true;
  }

  bool AssignString(const CelValue& cel_value) const {
    CelValue::StringHolder value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetString(value);
    return true;
  }

  bool AssignBytes(const CelValue& cel_value) const {
    CelValue::BytesHolder value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    static_cast<const Derived*>(this)->SetBytes(value);
    return true;
  }

  bool AssignEnum(const CelValue& cel_value) const {
    int64_t value;
    if (!cel_value.GetValue(&value)) {
      return false;
    }
    if (!cel::internal::CheckedInt64ToInt32(value).ok()) {
      return false;
    }
    static_cast<const Derived*>(this)->SetEnum(value);
    return true;
  }

  bool AssignMessage(const CelValue& cel_value) const {
    // Assigning a NULL to a message is OK, but a no-op.
    if (cel_value.IsNull()) {
      return true;
    }

    if (CelValue::MessageWrapper wrapper;
        cel_value.GetValue(&wrapper) && wrapper.HasFullProto()) {
      static_cast<const Derived*>(this)->SetMessage(
          cel::internal::down_cast<const Message*>(wrapper.message_ptr()));
      return true;
    }

    return false;
  }

  // This method provides message field content, wrapped in CelValue.
  // If value provided successfully, returns Ok.
  // arena Arena to use for allocations if needed.
  // result pointer to object to store value in.
  bool SetFieldFromCelValue(const CelValue& value) {
    switch (field_desc_->cpp_type()) {
      case FieldDescriptor::CPPTYPE_BOOL: {
        return AssignBool(value);
      }
      case FieldDescriptor::CPPTYPE_INT32: {
        return AssignInt32(value);
      }
      case FieldDescriptor::CPPTYPE_INT64: {
        return AssignInt64(value);
      }
      case FieldDescriptor::CPPTYPE_UINT32: {
        return AssignUInt32(value);
      }
      case FieldDescriptor::CPPTYPE_UINT64: {
        return AssignUInt64(value);
      }
      case FieldDescriptor::CPPTYPE_FLOAT: {
        return AssignFloat(value);
      }
      case FieldDescriptor::CPPTYPE_DOUBLE: {
        return AssignDouble(value);
      }
      case FieldDescriptor::CPPTYPE_STRING: {
        switch (field_desc_->type()) {
          case FieldDescriptor::TYPE_STRING:

            return AssignString(value);
          case FieldDescriptor::TYPE_BYTES:
            return AssignBytes(value);
          default:
            return false;
        }
        break;
      }
      case FieldDescriptor::CPPTYPE_MESSAGE: {
        // When the field is a message, it might be a well-known type with a
        // non-proto representation that requires special handling before it
        // can be set on the field.
        auto wrapped_value = CelProtoWrapper::MaybeWrapValue(
            field_desc_->message_type(), value, arena_);
        return AssignMessage(wrapped_value.value_or(value));
      }
      case FieldDescriptor::CPPTYPE_ENUM: {
        return AssignEnum(value);
      }
      default:
        return false;
    }

    return true;
  }

 protected:
  FieldSetter(Message* msg, const FieldDescriptor* field_desc, Arena* arena)
      : msg_(msg), field_desc_(field_desc), arena_(arena) {}

  Message* msg_;
  const FieldDescriptor* field_desc_;
  Arena* arena_;
};

// Accessor class, to work with singular fields
class ScalarFieldSetter : public FieldSetter<ScalarFieldSetter> {
 public:
  ScalarFieldSetter(Message* msg, const FieldDescriptor* field_desc,
                    Arena* arena)
      : FieldSetter(msg, field_desc, arena) {}

  bool SetBool(bool value) const {
    GetReflection()->SetBool(msg_, field_desc_, value);
    return true;
  }

  bool SetInt32(int32_t value) const {
    GetReflection()->SetInt32(msg_, field_desc_, value);
    return true;
  }

  bool SetUInt32(uint32_t value) const {
    GetReflection()->SetUInt32(msg_, field_desc_, value);
    return true;
  }

  bool SetInt64(int64_t value) const {
    GetReflection()->SetInt64(msg_, field_desc_, value);
    return true;
  }

  bool SetUInt64(uint64_t value) const {
    GetReflection()->SetUInt64(msg_, field_desc_, value);
    return true;
  }

  bool SetFloat(float value) const {
    GetReflection()->SetFloat(msg_, field_desc_, value);
    return true;
  }

  bool SetDouble(double value) const {
    GetReflection()->SetDouble(msg_, field_desc_, value);
    return true;
  }

  bool SetString(CelValue::StringHolder value) const {
    GetReflection()->SetString(msg_, field_desc_, std::string(value.value()));
    return true;
  }

  bool SetBytes(CelValue::BytesHolder value) const {
    GetReflection()->SetString(msg_, field_desc_, std::string(value.value()));
    return true;
  }

  bool SetMessage(const Message* value) const {
    if (!value) {
      GOOGLE_LOG(ERROR) << "Message is NULL";
      return true;
    }

    if (value->GetDescriptor()->full_name() ==
        field_desc_->message_type()->full_name()) {
      GetReflection()->MutableMessage(msg_, field_desc_)->MergeFrom(*value);
      return true;

    } else if (field_desc_->message_type()->full_name() == kProtobufAny) {
      auto any_msg = google::protobuf::DynamicCastToGenerated<google::protobuf::Any>(
          GetReflection()->MutableMessage(msg_, field_desc_));
      if (any_msg == nullptr) {
        // TODO(issues/68): This is probably a dynamic message. We should
        // implement this once we add support for dynamic protobuf types.
        return false;
      }
      any_msg->set_type_url(absl::StrCat(kTypeGoogleApisComPrefix,
                                         value->GetDescriptor()->full_name()));
      return value->SerializeToString(any_msg->mutable_value());
    }
    return false;
  }

  bool SetEnum(const int64_t value) const {
    GetReflection()->SetEnumValue(msg_, field_desc_, value);
    return true;
  }

  const Reflection* GetReflection() const { return msg_->GetReflection(); }
};

// Appender class, to work with repeated fields
class RepeatedFieldSetter : public FieldSetter<RepeatedFieldSetter> {
 public:
  RepeatedFieldSetter(Message* msg, const FieldDescriptor* field_desc,
                      Arena* arena)
      : FieldSetter(msg, field_desc, arena) {}

  bool SetBool(bool value) const {
    GetReflection()->AddBool(msg_, field_desc_, value);
    return true;
  }

  bool SetInt32(int32_t value) const {
    GetReflection()->AddInt32(msg_, field_desc_, value);
    return true;
  }

  bool SetUInt32(uint32_t value) const {
    GetReflection()->AddUInt32(msg_, field_desc_, value);
    return true;
  }

  bool SetInt64(int64_t value) const {
    GetReflection()->AddInt64(msg_, field_desc_, value);
    return true;
  }

  bool SetUInt64(uint64_t value) const {
    GetReflection()->AddUInt64(msg_, field_desc_, value);
    return true;
  }

  bool SetFloat(float value) const {
    GetReflection()->AddFloat(msg_, field_desc_, value);
    return true;
  }

  bool SetDouble(double value) const {
    GetReflection()->AddDouble(msg_, field_desc_, value);
    return true;
  }

  bool SetString(CelValue::StringHolder value) const {
    GetReflection()->AddString(msg_, field_desc_, std::string(value.value()));
    return true;
  }

  bool SetBytes(CelValue::BytesHolder value) const {
    GetReflection()->AddString(msg_, field_desc_, std::string(value.value()));
    return true;
  }

  bool SetMessage(const Message* value) const {
    if (!value) return true;
    if (value->GetDescriptor()->full_name() !=
        field_desc_->message_type()->full_name()) {
      return false;
    }

    GetReflection()->AddMessage(msg_, field_desc_)->MergeFrom(*value);
    return true;
  }

  bool SetEnum(const int64_t value) const {
    GetReflection()->AddEnumValue(msg_, field_desc_, value);
    return true;
  }

 private:
  const Reflection* GetReflection() const { return msg_->GetReflection(); }
};

// This method sets message field
// If value provided successfully, returns Ok.
// arena Arena to use for allocations if needed.
// result pointer to object to store value in.
absl::Status SetValueToSingleField(const CelValue& value,
                                   const FieldDescriptor* desc, Message* msg,
                                   Arena* arena) {
  ScalarFieldSetter setter(msg, desc, arena);
  return (setter.SetFieldFromCelValue(value))
             ? absl::OkStatus()
             : absl::InvalidArgumentError(absl::Substitute(
                   "Could not assign supplied argument to message \"$0\" field "
                   "\"$1\" of type $2: value was \"$3\"",
                   msg->GetDescriptor()->name(), desc->name(),
                   desc->type_name(), value.DebugString()));
}

absl::Status AddValueToRepeatedField(const CelValue& value,
                                     const FieldDescriptor* desc, Message* msg,
                                     Arena* arena) {
  RepeatedFieldSetter setter(msg, desc, arena);
  return (setter.SetFieldFromCelValue(value))
             ? absl::OkStatus()
             : absl::InvalidArgumentError(absl::Substitute(
                   "Could not add supplied argument \"$2\" to message \"$0\" "
                   "field \"$1\".",
                   msg->GetDescriptor()->name(), desc->name(),
                   value.DebugString()));
}

}  // namespace google::api::expr::runtime

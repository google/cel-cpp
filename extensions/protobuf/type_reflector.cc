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

#include "extensions/protobuf/type_reflector.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/values/piecewise_value_manager.h"
#include "extensions/protobuf/internal/any.h"
#include "extensions/protobuf/internal/duration.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "extensions/protobuf/internal/message.h"
#include "extensions/protobuf/internal/struct.h"
#include "extensions/protobuf/internal/timestamp.h"
#include "extensions/protobuf/internal/wrappers.h"
#include "extensions/protobuf/json.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

namespace {

using ProtoRepeatedFieldFromValueMutator = absl::Status (*)(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*>, absl::Nonnull<google::protobuf::Message*>,
    absl::Nonnull<const google::protobuf::FieldDescriptor*>, const Value&);

absl::Status ProtoBoolRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto bool_value = As<BoolValue>(value); bool_value) {
    reflection->AddBool(message, field, bool_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
}

absl::Status ProtoInt32RepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto int_value = As<IntValue>(value); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    reflection->AddInt32(message, field,
                         static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoInt64RepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto int_value = As<IntValue>(value); int_value) {
    reflection->AddInt64(message, field, int_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoUInt32RepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return absl::OutOfRangeError("uint64 to uint32_t overflow");
    }
    reflection->AddUInt32(message, field,
                          static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoUInt64RepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    reflection->AddUInt64(message, field, uint_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoFloatRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto double_value = As<DoubleValue>(value); double_value) {
    reflection->AddFloat(message, field,
                         static_cast<float>(double_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "double").NativeValue();
}

absl::Status ProtoDoubleRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto double_value = As<DoubleValue>(value); double_value) {
    reflection->AddDouble(message, field, double_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "double").NativeValue();
}

absl::Status ProtoBytesRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto bytes_value = As<BytesValue>(value); bytes_value) {
    reflection->AddString(message, field, bytes_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bytes").NativeValue();
}

absl::Status ProtoStringRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (auto string_value = As<StringValue>(value); string_value) {
    reflection->AddString(message, field, string_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "string").NativeValue();
}

absl::Status ProtoNullRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  if (InstanceOf<NullValue>(value) || InstanceOf<IntValue>(value)) {
    reflection->AddEnumValue(message, field, 0);
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "null_type").NativeValue();
}

absl::Status ProtoEnumRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*>,
    absl::Nonnull<google::protobuf::MessageFactory*>,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  const auto* enum_descriptor = field->enum_type();
  if (auto int_value = As<IntValue>(value); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int>::min() ||
        int_value->NativeValue() > std::numeric_limits<int>::max()) {
      return TypeConversionError(value.GetTypeName(),
                                 enum_descriptor->full_name())
          .NativeValue();
    }
    reflection->AddEnumValue(message, field,
                             static_cast<int>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), enum_descriptor->full_name())
      .NativeValue();
}

absl::Status ProtoMessageRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, const Value& value) {
  auto* element = reflection->AddMessage(message, field, factory);
  auto status = protobuf_internal::ProtoMessageFromValueImpl(value, pool,
                                                             factory, element);
  if (!status.ok()) {
    reflection->RemoveLast(message, field);
  }
  return status;
}

absl::StatusOr<ProtoRepeatedFieldFromValueMutator>
GetProtoRepeatedFieldFromValueMutator(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  ABSL_DCHECK(!field->is_map());
  ABSL_DCHECK(field->is_repeated());
  switch (field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolRepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32RepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64RepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32RepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64RepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return ProtoFloatRepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return ProtoDoubleRepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        return ProtoBytesRepeatedFieldFromValueMutator;
      }
      return ProtoStringRepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
        return ProtoNullRepeatedFieldFromValueMutator;
      }
      return ProtoEnumRepeatedFieldFromValueMutator;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoMessageRepeatedFieldFromValueMutator;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer repeated field type: ",
          google::protobuf::FieldDescriptor::CppTypeName(field->cpp_type())));
  }
}

class ProtoStructValueBuilder final : public StructValueBuilder {
 public:
  ProtoStructValueBuilder(
      const ProtoTypeReflector& type_reflector, ValueFactory& value_factory,
      absl::Nullable<google::protobuf::Arena*> arena,
      absl::Nonnull<google::protobuf::Message*> message,
      absl::Nonnull<const google::protobuf::Reflection*> const reflection,
      absl::Nonnull<const google::protobuf::Descriptor*> const descriptor)
      : type_reflector_(type_reflector),
        value_factory_(value_factory),
        arena_(arena),
        message_(message),
        reflection_(reflection),
        descriptor_(descriptor) {}

  ~ProtoStructValueBuilder() override {
    if (message_ != nullptr && arena_ == nullptr) {
      delete message_;
    }
  }

  absl::Status SetFieldByName(absl::string_view name, Value value) override {
    const auto* field = descriptor_->FindFieldByName(name);
    if (field == nullptr) {
      field = reflection_->FindKnownExtensionByName(name);
      if (field == nullptr) {
        return NoSuchFieldError(name).NativeValue();
      }
    }
    return SetField(field, std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Value value) override {
    if (number < std::numeric_limits<int>::min() ||
        number > std::numeric_limits<int>::max()) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    const auto* field = descriptor_->FindFieldByNumber(number);
    if (field == nullptr) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return SetField(field, std::move(value));
  }

  absl::StatusOr<StructValue> Build() && override {
    ABSL_ASSERT(message_ != nullptr);
    auto* message = message_;
    message_ = nullptr;
    return protobuf_internal::ProtoMessageAsStructValueImpl(value_factory_,
                                                            message);
  }

 private:
  absl::Status SetField(absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                        Value value) {
    if (field->is_map()) {
      return SetMapField(field, std::move(value));
    }
    if (field->is_repeated()) {
      return SetRepeatedField(field, std::move(value));
    }
    return SetSingularField(field, std::move(value));
  }

  absl::Status SetMapField(absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                           Value value) {
    auto map_value = As<MapValue>(value);
    if (!map_value) {
      return TypeConversionError(value.GetTypeName(), "map").NativeValue();
    }
    CEL_ASSIGN_OR_RETURN(auto key_converter,
                         protobuf_internal::GetProtoMapKeyFromValueConverter(
                             field->message_type()->map_key()->cpp_type()));
    CEL_ASSIGN_OR_RETURN(
        auto value_converter,
        protobuf_internal::GetProtoMapValueFromValueConverter(field));
    reflection_->ClearField(message_, field);
    common_internal::PiecewiseValueManager value_manager(type_reflector_,
                                                         value_factory_);
    const auto* map_value_field = field->message_type()->map_value();
    CEL_RETURN_IF_ERROR(map_value->ForEach(
        value_manager,
        [this, field, key_converter, map_value_field, value_converter](
            const Value& entry_key,
            const Value& entry_value) -> absl::StatusOr<bool> {
          google::protobuf::MapKey proto_key;
          CEL_RETURN_IF_ERROR((*key_converter)(entry_key, proto_key));
          google::protobuf::MapValueRef proto_value;
          protobuf_internal::InsertOrLookupMapValue(
              *reflection_, message_, *field, proto_key, &proto_value);
          CEL_RETURN_IF_ERROR(
              (*value_converter)(entry_value, map_value_field, proto_value));
          return true;
        }));
    return absl::OkStatus();
  }

  absl::Status SetRepeatedField(
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, Value value) {
    auto list_value = As<ListValue>(value);
    if (!list_value) {
      return TypeConversionError(value.GetTypeName(), "list").NativeValue();
    }
    CEL_ASSIGN_OR_RETURN(auto accessor,
                         GetProtoRepeatedFieldFromValueMutator(field));
    reflection_->ClearField(message_, field);
    common_internal::PiecewiseValueManager value_manager(type_reflector_,
                                                         value_factory_);
    CEL_RETURN_IF_ERROR(list_value->ForEach(
        value_manager,
        [this, field, accessor](const Value& element) -> absl::StatusOr<bool> {
          CEL_RETURN_IF_ERROR((*accessor)(type_reflector_.descriptor_pool(),
                                          type_reflector_.message_factory(),
                                          reflection_, message_, field,
                                          element));
          return true;
        }));
    return absl::OkStatus();
  }

  absl::Status SetSingularField(
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, Value value) {
    switch (field->cpp_type()) {
      case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
        if (auto bool_value = As<BoolValue>(value); bool_value) {
          reflection_->SetBool(message_, field, bool_value->NativeValue());
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
        if (auto int_value = As<IntValue>(value); int_value) {
          if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
              int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
            return absl::OutOfRangeError("int64 to int32_t overflow");
          }
          reflection_->SetInt32(message_, field,
                                static_cast<int32_t>(int_value->NativeValue()));
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "int").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
        if (auto int_value = As<IntValue>(value); int_value) {
          reflection_->SetInt64(message_, field, int_value->NativeValue());
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "int").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
        if (auto uint_value = As<UintValue>(value); uint_value) {
          if (uint_value->NativeValue() >
              std::numeric_limits<uint32_t>::max()) {
            return absl::OutOfRangeError("uint64 to uint32_t overflow");
          }
          reflection_->SetUInt32(
              message_, field,
              static_cast<uint32_t>(uint_value->NativeValue()));
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
        if (auto uint_value = As<UintValue>(value); uint_value) {
          reflection_->SetUInt64(message_, field, uint_value->NativeValue());
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT: {
        if (auto double_value = As<DoubleValue>(value); double_value) {
          reflection_->SetFloat(message_, field, double_value->NativeValue());
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "double").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE: {
        if (auto double_value = As<DoubleValue>(value); double_value) {
          reflection_->SetDouble(message_, field, double_value->NativeValue());
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "double").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
        if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
          if (auto bytes_value = As<BytesValue>(value); bytes_value) {
            bytes_value->NativeValue(absl::Overload(
                [this, field](absl::string_view string) {
                  reflection_->SetString(message_, field, std::string(string));
                },
                [this, field](const absl::Cord& cord) {
                  reflection_->SetString(message_, field, cord);
                }));
            return absl::OkStatus();
          }
          return TypeConversionError(value.GetTypeName(), "bytes")
              .NativeValue();
        }
        if (auto string_value = As<StringValue>(value); string_value) {
          string_value->NativeValue(absl::Overload(
              [this, field](absl::string_view string) {
                reflection_->SetString(message_, field, std::string(string));
              },
              [this, field](const absl::Cord& cord) {
                reflection_->SetString(message_, field, cord);
              }));
          return absl::OkStatus();
        }
        return TypeConversionError(value.GetTypeName(), "string").NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
        if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
          if (InstanceOf<NullValue>(value) || InstanceOf<IntValue>(value)) {
            reflection_->SetEnumValue(message_, field, 0);
            return absl::OkStatus();
          }
          return TypeConversionError(value.GetTypeName(), "null_type")
              .NativeValue();
        }
        if (auto int_value = As<IntValue>(value); int_value) {
          if (int_value->NativeValue() >= std::numeric_limits<int32_t>::min() &&
              int_value->NativeValue() <= std::numeric_limits<int32_t>::max()) {
            reflection_->SetEnumValue(
                message_, field, static_cast<int>(int_value->NativeValue()));
            return absl::OkStatus();
          }
        }
        return TypeConversionError(value.GetTypeName(),
                                   field->enum_type()->full_name())
            .NativeValue();
      }
      case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
        switch (field->message_type()->well_known_type()) {
          case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
            if (auto bool_value = As<BoolValue>(value); bool_value) {
              return protobuf_internal::WrapDynamicBoolValueProto(
                  bool_value->NativeValue(),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE: {
            if (auto int_value = As<IntValue>(value); int_value) {
              if (int_value->NativeValue() <
                      std::numeric_limits<int32_t>::min() ||
                  int_value->NativeValue() >
                      std::numeric_limits<int32_t>::max()) {
                return absl::OutOfRangeError("int64 to int32_t overflow");
              }
              return protobuf_internal::WrapDynamicInt32ValueProto(
                  static_cast<int32_t>(int_value->NativeValue()),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE: {
            if (auto int_value = As<IntValue>(value); int_value) {
              return protobuf_internal::WrapDynamicInt64ValueProto(
                  int_value->NativeValue(),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
            if (auto uint_value = As<UintValue>(value); uint_value) {
              if (uint_value->NativeValue() >
                  std::numeric_limits<uint32_t>::max()) {
                return absl::OutOfRangeError("uint64 to uint32_t overflow");
              }
              return protobuf_internal::WrapDynamicUInt32ValueProto(
                  static_cast<uint32_t>(uint_value->NativeValue()),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
            if (auto uint_value = As<UintValue>(value); uint_value) {
              return protobuf_internal::WrapDynamicUInt64ValueProto(
                  uint_value->NativeValue(),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
            if (auto double_value = As<DoubleValue>(value); double_value) {
              return protobuf_internal::WrapDynamicFloatValueProto(
                  static_cast<float>(double_value->NativeValue()),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
            if (auto double_value = As<DoubleValue>(value); double_value) {
              return protobuf_internal::WrapDynamicDoubleValueProto(
                  double_value->NativeValue(),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
            if (auto bytes_value = As<BytesValue>(value); bytes_value) {
              return protobuf_internal::WrapDynamicBytesValueProto(
                  bytes_value->NativeCord(),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
            if (auto string_value = As<StringValue>(value); string_value) {
              return protobuf_internal::WrapDynamicStringValueProto(
                  string_value->NativeCord(),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION: {
            if (auto duration_value = As<DurationValue>(value);
                duration_value) {
              return protobuf_internal::WrapDynamicDurationProto(
                  duration_value->NativeValue(),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
            if (auto timestamp_value = As<TimestampValue>(value);
                timestamp_value) {
              return protobuf_internal::WrapDynamicTimestampProto(
                  timestamp_value->NativeValue(),
                  *reflection_->MutableMessage(
                      message_, field, type_reflector_.message_factory()));
            }
            return TypeConversionError(value.GetTypeName(),
                                       field->message_type()->full_name())
                .NativeValue();
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE: {
            // Probably not correct, need to use the parent/common one.
            ProtoAnyToJsonConverter converter(
                type_reflector_.descriptor_pool(),
                type_reflector_.message_factory());
            CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
            return protobuf_internal::DynamicValueProtoFromJson(
                json, *reflection_->MutableMessage(
                          message_, field, type_reflector_.message_factory()));
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE: {
            // Probably not correct, need to use the parent/common one.
            ProtoAnyToJsonConverter converter(
                type_reflector_.descriptor_pool(),
                type_reflector_.message_factory());
            CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
            if (!absl::holds_alternative<JsonArray>(json)) {
              return TypeConversionError(value.GetTypeName(),
                                         field->message_type()->full_name())
                  .NativeValue();
            }
            return protobuf_internal::DynamicListValueProtoFromJson(
                absl::get<JsonArray>(json),
                *reflection_->MutableMessage(
                    message_, field, type_reflector_.message_factory()));
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT: {
            // Probably not correct, need to use the parent/common one.
            ProtoAnyToJsonConverter converter(
                type_reflector_.descriptor_pool(),
                type_reflector_.message_factory());
            CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
            if (!absl::holds_alternative<JsonObject>(json)) {
              return TypeConversionError(value.GetTypeName(),
                                         field->message_type()->full_name())
                  .NativeValue();
            }
            return protobuf_internal::DynamicStructProtoFromJson(
                absl::get<JsonObject>(json),
                *reflection_->MutableMessage(
                    message_, field, type_reflector_.message_factory()));
          }
          case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY: {
            // Probably not correct, need to use the parent/common one.
            ProtoAnyToJsonConverter converter(
                type_reflector_.descriptor_pool(),
                type_reflector_.message_factory());
            absl::Cord serialized;
            CEL_RETURN_IF_ERROR(value.SerializeTo(converter, serialized));
            std::string type_url;
            switch (value.kind()) {
              case ValueKind::kNull:
                type_url = MakeTypeUrl("google.protobuf.Value");
                break;
              case ValueKind::kBool:
                type_url = MakeTypeUrl("google.protobuf.BoolValue");
                break;
              case ValueKind::kInt:
                type_url = MakeTypeUrl("google.protobuf.Int64Value");
                break;
              case ValueKind::kUint:
                type_url = MakeTypeUrl("google.protobuf.UInt64Value");
                break;
              case ValueKind::kDouble:
                type_url = MakeTypeUrl("google.protobuf.DoubleValue");
                break;
              case ValueKind::kBytes:
                type_url = MakeTypeUrl("google.protobuf.BytesValue");
                break;
              case ValueKind::kString:
                type_url = MakeTypeUrl("google.protobuf.StringValue");
                break;
              case ValueKind::kList:
                type_url = MakeTypeUrl("google.protobuf.ListValue");
                break;
              case ValueKind::kMap:
                type_url = MakeTypeUrl("google.protobuf.Struct");
                break;
              case ValueKind::kDuration:
                type_url = MakeTypeUrl("google.protobuf.Duration");
                break;
              case ValueKind::kTimestamp:
                type_url = MakeTypeUrl("google.protobuf.Timestamp");
                break;
              default:
                type_url = MakeTypeUrl(value.GetTypeName());
                break;
            }
            return protobuf_internal::WrapDynamicAnyProto(
                type_url, serialized,
                *reflection_->MutableMessage(
                    message_, field, type_reflector_.message_factory()));
          }
          default:
            break;
        }
        return protobuf_internal::ProtoMessageFromValueImpl(
            value, reflection_->MutableMessage(
                       message_, field, type_reflector_.message_factory()));
      }
      default:
        return absl::InternalError(
            absl::StrCat("unexpected protocol buffer message field type: ",
                         field->cpp_type_name()));
    }
  }

  const ProtoTypeReflector& type_reflector_;
  ValueFactory& value_factory_;
  absl::Nullable<google::protobuf::Arena*> const arena_;
  absl::Nonnull<google::protobuf::Message*> message_;
  absl::Nonnull<const google::protobuf::Reflection*> const reflection_;
  absl::Nonnull<const google::protobuf::Descriptor*> const descriptor_;
};

}  // namespace

absl::StatusOr<absl::optional<Unique<StructValueBuilder>>>
ProtoTypeReflector::NewStructValueBuilder(ValueFactory& value_factory,
                                          const StructType& type) const {
  // Well known types are handled via `NewValueBuilder`. If we are requested to
  // create a well known type here, we pretend we do not support it.
  const auto* descriptor =
      descriptor_pool()->FindMessageTypeByName(type.name());
  if (descriptor == nullptr) {
    return absl::nullopt;
  }
  switch (descriptor->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY:
      return absl::nullopt;
    default:
      break;
  }
  const auto* prototype = message_factory()->GetPrototype(descriptor);
  if (prototype == nullptr) {
    return absl::nullopt;
  }
  auto memory_manager = value_factory.GetMemoryManager();
  auto* arena = ProtoMemoryManagerArena(memory_manager);
  auto message = protobuf_internal::ArenaUniquePtr<google::protobuf::Message>{
      prototype->New(arena), protobuf_internal::DefaultArenaDeleter{arena}};
  CEL_ASSIGN_OR_RETURN(const auto* reflection,
                       protobuf_internal::GetReflection(*message));
  CEL_ASSIGN_OR_RETURN(descriptor, protobuf_internal::GetDescriptor(*message));
  return memory_manager.MakeUnique<ProtoStructValueBuilder>(
      *this, value_factory, arena, message.release(), reflection, descriptor);
}

absl::StatusOr<absl::optional<Value>> ProtoTypeReflector::DeserializeValueImpl(
    ValueFactory& value_factory, absl::string_view type_url,
    const absl::Cord& value) const {
  absl::string_view type_name;
  if (!ParseTypeUrl(type_url, &type_name)) {
    return absl::InvalidArgumentError("invalid type URL");
  }
  const auto* descriptor = descriptor_pool()->FindMessageTypeByName(type_name);
  if (descriptor == nullptr) {
    return absl::nullopt;
  }
  const auto* prototype = message_factory()->GetPrototype(descriptor);
  if (prototype == nullptr) {
    return absl::nullopt;
  }
  return protobuf_internal::ProtoMessageToValueImpl(value_factory, *this,
                                                    prototype, value);
}

}  // namespace cel::extensions

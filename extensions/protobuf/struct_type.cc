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

#include "extensions/protobuf/struct_type.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/type_manager.h"
#include "base/value_factory.h"
#include "base/values/opaque_value.h"
#include "base/values/struct_value_builder.h"
#include "eval/internal/errors.h"
#include "extensions/protobuf/enum_type.h"
#include "extensions/protobuf/internal/any.h"
#include "extensions/protobuf/internal/duration.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "extensions/protobuf/internal/reflection.h"
#include "extensions/protobuf/internal/struct.h"
#include "extensions/protobuf/internal/timestamp.h"
#include "extensions/protobuf/internal/wrappers.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/struct_value.h"
#include "extensions/protobuf/type.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/reflection.h"

namespace cel::extensions {

absl::StatusOr<Handle<ProtoStructType>> ProtoStructType::Resolve(
    TypeManager& type_manager, const google::protobuf::Descriptor& descriptor) {
  CEL_ASSIGN_OR_RETURN(auto type,
                       type_manager.ResolveType(descriptor.full_name()));
  if (ABSL_PREDICT_FALSE(!type.has_value())) {
    return absl::NotFoundError(absl::StrCat(
        "Missing protocol buffer message type implementation for \"",
        descriptor.full_name(), "\""));
  }
  if (ABSL_PREDICT_FALSE(!(*type)->Is<ProtoStructType>())) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Unexpected protocol buffer message type implementation for \"",
        descriptor.full_name(), "\": ", (*type)->DebugString()));
  }
  return std::move(type).value().As<ProtoStructType>();
}

namespace {

absl::StatusOr<Handle<Type>> FieldDescriptorToTypeSingular(
    TypeManager& type_manager, const google::protobuf::FieldDescriptor* field_desc) {
  switch (field_desc->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return type_manager.type_factory().GetDoubleType();
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      return type_manager.type_factory().GetIntType();
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return type_manager.type_factory().GetUintType();
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return type_manager.type_factory().GetBoolType();
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      return type_manager.type_factory().GetStringType();
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      return ProtoType::Resolve(type_manager, *field_desc->message_type());
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      return type_manager.type_factory().GetBytesType();
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      return ProtoType::Resolve(type_manager, *field_desc->enum_type());
  }
}

absl::StatusOr<Handle<Type>> FieldDescriptorToTypeRepeated(
    TypeManager& type_manager, const google::protobuf::FieldDescriptor* field_desc) {
  CEL_ASSIGN_OR_RETURN(auto type,
                       FieldDescriptorToTypeSingular(type_manager, field_desc));
  // The wrapper types make zero sense as a list element, list elements of
  // wrapper types can never be null.
  return type_manager.type_factory().CreateListType(
      UnwrapType(std::move(type)));
}

absl::StatusOr<Handle<Type>> FieldDescriptorToType(
    TypeManager& type_manager, const google::protobuf::FieldDescriptor* field_desc) {
  if (field_desc->is_map()) {
    const auto* key_desc = field_desc->message_type()->map_key();
    CEL_ASSIGN_OR_RETURN(auto key_type,
                         FieldDescriptorToTypeSingular(type_manager, key_desc));
    const auto* value_desc = field_desc->message_type()->map_value();
    CEL_ASSIGN_OR_RETURN(auto value_type, FieldDescriptorToTypeSingular(
                                              type_manager, value_desc));
    // The wrapper types make zero sense as a map value, map values of
    // wrapper types can never be null.
    return type_manager.type_factory().CreateMapType(
        std::move(key_type), UnwrapType(std::move(value_type)));
  }
  if (field_desc->is_repeated()) {
    return FieldDescriptorToTypeRepeated(type_manager, field_desc);
  }
  return FieldDescriptorToTypeSingular(type_manager, field_desc);
}

template <typename F, typename T>
struct CheckedCast;

template <typename T>
struct CheckedCast<T, T> {
  static absl::StatusOr<T> Cast(T value) { return value; }
};

template <>
struct CheckedCast<double, float> {
  static absl::StatusOr<float> Cast(double value) {
    if (ABSL_PREDICT_FALSE(static_cast<double>(static_cast<float>(value)) !=
                           value)) {
      return absl::OutOfRangeError("double to float overflow");
    }
    return static_cast<float>(value);
  }
};

template <>
struct CheckedCast<int64_t, int32_t> {
  static absl::StatusOr<int32_t> Cast(int64_t value) {
    if (ABSL_PREDICT_FALSE(value < std::numeric_limits<int32_t>::min() ||
                           value > std::numeric_limits<int32_t>::max())) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    return static_cast<int32_t>(value);
  }
};

template <>
struct CheckedCast<uint64_t, uint32_t> {
  static absl::StatusOr<uint32_t> Cast(uint64_t value) {
    if (ABSL_PREDICT_FALSE(value > std::numeric_limits<int32_t>::max())) {
      return absl::OutOfRangeError("uint64 to uint32_t overflow");
    }
    return static_cast<uint32_t>(value);
  }
};

std::string MakeAnyTypeUrl(absl::string_view type_name) {
  return absl::StrCat("type.googleapis.com/", type_name);
}

absl::Status TypeConversionError(const Type& from, const Type& to) {
  return absl::InvalidArgumentError(absl::StrCat("type conversion error from ",
                                                 from.DebugString(), " to ",
                                                 to.DebugString()));
}

}  // namespace

class ProtoStructTypeFieldIterator final : public StructType::FieldIterator {
 public:
  ProtoStructTypeFieldIterator(TypeManager& type_manager,
                               const google::protobuf::Descriptor& descriptor)
      : type_manager_(type_manager), descriptor_(descriptor) {}

  bool HasNext() override { return index_ < descriptor_.field_count(); }

  absl::StatusOr<StructType::Field> Next() override {
    if (ABSL_PREDICT_FALSE(index_ >= descriptor_.field_count())) {
      return absl::FailedPreconditionError(
          "StructType::FieldIterator::Next() called when "
          "StructType::FieldIterator::HasNext() returns false");
    }
    const auto* field = descriptor_.field(index_);
    CEL_ASSIGN_OR_RETURN(auto type,
                         FieldDescriptorToType(type_manager_, field));
    ++index_;
    return StructType::Field(ProtoStructType::MakeFieldId(field->number()),
                             field->name(), field->number(), std::move(type),
                             field);
  }

  absl::StatusOr<FieldId> NextId() override {
    if (ABSL_PREDICT_FALSE(index_ >= descriptor_.field_count())) {
      return absl::FailedPreconditionError(
          "StructType::FieldIterator::Next() called when "
          "StructType::FieldIterator::HasNext() returns false");
    }
    return ProtoStructType::MakeFieldId(descriptor_.field(index_++)->number());
  }

  absl::StatusOr<absl::string_view> NextName() override {
    if (ABSL_PREDICT_FALSE(index_ >= descriptor_.field_count())) {
      return absl::FailedPreconditionError(
          "StructType::FieldIterator::Next() called when "
          "StructType::FieldIterator::HasNext() returns false");
    }
    return descriptor_.field(index_++)->name();
  }

  absl::StatusOr<int64_t> NextNumber() override {
    if (ABSL_PREDICT_FALSE(index_ >= descriptor_.field_count())) {
      return absl::FailedPreconditionError(
          "StructType::FieldIterator::Next() called when "
          "StructType::FieldIterator::HasNext() returns false");
    }
    return descriptor_.field(index_++)->number();
  }

 private:
  TypeManager& type_manager_;
  const google::protobuf::Descriptor& descriptor_;
  int index_ = 0;
};

size_t ProtoStructType::field_count() const {
  return descriptor().field_count();
}

absl::StatusOr<UniqueRef<StructType::FieldIterator>>
ProtoStructType::NewFieldIterator(TypeManager& type_manager) const {
  return MakeUnique<ProtoStructTypeFieldIterator>(type_manager.memory_manager(),
                                                  type_manager, descriptor());
}

absl::StatusOr<absl::optional<ProtoStructType::Field>>
ProtoStructType::FindFieldByName(TypeManager& type_manager,
                                 absl::string_view name) const {
  const auto* field_desc = descriptor().FindFieldByName(name);
  if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
    return absl::nullopt;
  }
  CEL_ASSIGN_OR_RETURN(auto type,
                       FieldDescriptorToType(type_manager, field_desc));
  return Field(MakeFieldId(field_desc->number()), field_desc->name(),
               field_desc->number(), std::move(type), field_desc);
}

absl::StatusOr<absl::optional<ProtoStructType::Field>>
ProtoStructType::FindFieldByNumber(TypeManager& type_manager,
                                   int64_t number) const {
  if (ABSL_PREDICT_FALSE(number < std::numeric_limits<int>::min() ||
                         number > std::numeric_limits<int>::max())) {
    // Treat it as not found.
    return absl::nullopt;
  }
  const auto* field_desc =
      descriptor().FindFieldByNumber(static_cast<int>(number));
  if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
    return absl::nullopt;
  }
  CEL_ASSIGN_OR_RETURN(auto type,
                       FieldDescriptorToType(type_manager, field_desc));
  return Field(MakeFieldId(field_desc->number()), field_desc->name(),
               field_desc->number(), std::move(type), field_desc);
}

// TODO(uncreated-issue/47): handle subtle implicit conversions around mixed numeric
class ProtoStructValueBuilder final : public StructValueBuilderInterface {
 public:
  ProtoStructValueBuilder(ValueFactory& value_factory,
                          Handle<ProtoStructType> type,
                          google::protobuf::MessageFactory* factory,
                          google::protobuf::Message* message)
      : value_factory_(value_factory),
        type_(std::move(type)),
        factory_(factory),
        message_(ABSL_DIE_IF_NULL(message)) {}  // Crash OK

  ~ProtoStructValueBuilder() override {
    if (message_ != nullptr && message_->GetArena() == nullptr) {
      delete message_;
    }
  }

  absl::Status SetFieldByName(absl::string_view name,
                              Handle<Value> value) override {
    CEL_ASSIGN_OR_RETURN(
        auto field_type,
        type_->FindFieldByName(value_factory_.type_manager(), name));
    if (ABSL_PREDICT_FALSE(!field_type)) {
      return runtime_internal::CreateNoSuchFieldError(name);
    }
    return SetField(*field_type, std::move(value));
  }

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    CEL_ASSIGN_OR_RETURN(
        auto field_type,
        type_->FindFieldByNumber(value_factory_.type_manager(), number));
    if (ABSL_PREDICT_FALSE(!field_type)) {
      return runtime_internal::CreateNoSuchFieldError(absl::StrCat(number));
    }
    return SetField(*field_type, std::move(value));
  }

  absl::StatusOr<Handle<StructValue>> Build() && override {
    return protobuf_internal::ParsedProtoStructValue::Create(
        value_factory_, std::move(*message_));
  }

 private:
  absl::Status SetField(const StructType::Field& field, Handle<Value>&& value) {
    const auto* field_desc =
        static_cast<const google::protobuf::FieldDescriptor*>(field.hint);
    const auto* reflect = message_->GetReflection();
    if (field_desc->is_map()) {
      if (ABSL_PREDICT_FALSE(!value->Is<MapValue>())) {
        return TypeConversionError(*field.type, *value->type());
      }
      return SetMapField(field, *reflect, *field_desc,
                         std::move(value).As<MapValue>());
    }
    if (field_desc->is_repeated()) {
      if (ABSL_PREDICT_FALSE(!value->Is<ListValue>())) {
        return TypeConversionError(*field.type, *value->type());
      }
      return SetRepeatedField(field, *reflect, *field_desc,
                              std::move(value).As<ListValue>());
    }
    return SetSingularField(field, *reflect, *field_desc, std::move(value));
  }

  static absl::StatusOr<
      absl::AnyInvocable<absl::Status(const Value&, google::protobuf::MapKey&)>>
  GetMapFieldKeyConverter(
      const google::protobuf::FieldDescriptor& field_desc, const Type& from_key_type,
      const Type& to_key_type ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    switch (field_desc.type()) {
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
        if (ABSL_PREDICT_FALSE(!from_key_type.Is<BoolType>() &&
                               !from_key_type.Is<DynType>())) {
          return TypeConversionError(from_key_type, to_key_type);
        }
        return [&to_key_type](const Value& value,
                              google::protobuf::MapKey& key) -> absl::Status {
          if (ABSL_PREDICT_FALSE(!value.Is<BoolValue>())) {
            return TypeConversionError(*value.type(), to_key_type);
          }
          key.SetBoolValue(value.As<BoolValue>().value());
          return absl::OkStatus();
        };
      case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT32:
        if (ABSL_PREDICT_FALSE(!from_key_type.Is<IntType>() &&
                               !from_key_type.Is<DynType>())) {
          return TypeConversionError(from_key_type, to_key_type);
        }
        return [&to_key_type](const Value& value,
                              google::protobuf::MapKey& key) -> absl::Status {
          if (ABSL_PREDICT_FALSE(!value.Is<IntValue>())) {
            return TypeConversionError(*value.type(), to_key_type);
          }
          CEL_ASSIGN_OR_RETURN(auto raw_value,
                               (CheckedCast<int64_t, int32_t>::Cast(
                                   value.As<IntValue>().value())));
          key.SetInt32Value(raw_value);
          return absl::OkStatus();
        };
      case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT64:
        if (ABSL_PREDICT_FALSE(!from_key_type.Is<IntType>() &&
                               !from_key_type.Is<DynType>())) {
          return TypeConversionError(from_key_type, to_key_type);
        }
        return [&to_key_type](const Value& value,
                              google::protobuf::MapKey& key) -> absl::Status {
          if (ABSL_PREDICT_FALSE(!value.Is<IntValue>())) {
            return TypeConversionError(*value.type(), to_key_type);
          }
          key.SetInt64Value(value.As<IntValue>().value());
          return absl::OkStatus();
        };
      case google::protobuf::FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
        if (ABSL_PREDICT_FALSE(!from_key_type.Is<UintType>() &&
                               !from_key_type.Is<DynType>())) {
          return TypeConversionError(from_key_type, to_key_type);
        }
        return [&to_key_type](const Value& value,
                              google::protobuf::MapKey& key) -> absl::Status {
          if (ABSL_PREDICT_FALSE(!value.Is<UintValue>())) {
            return TypeConversionError(*value.type(), to_key_type);
          }
          CEL_ASSIGN_OR_RETURN(auto raw_value,
                               (CheckedCast<uint64_t, uint32_t>::Cast(
                                   value.As<UintValue>().value())));
          key.SetUInt32Value(raw_value);
          return absl::OkStatus();
        };
      case google::protobuf::FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
        if (ABSL_PREDICT_FALSE(!from_key_type.Is<UintType>() &&
                               !from_key_type.Is<DynType>())) {
          return TypeConversionError(from_key_type, to_key_type);
        }
        return [&to_key_type](const Value& value,
                              google::protobuf::MapKey& key) -> absl::Status {
          if (ABSL_PREDICT_FALSE(!value.Is<UintValue>())) {
            return TypeConversionError(*value.type(), to_key_type);
          }
          key.SetUInt64Value(value.As<UintValue>().value());
          return absl::OkStatus();
        };
      case google::protobuf::FieldDescriptor::TYPE_STRING:
        if (ABSL_PREDICT_FALSE(!from_key_type.Is<StringType>() &&
                               !from_key_type.Is<DynType>())) {
          return TypeConversionError(from_key_type, to_key_type);
        }
        return [&to_key_type](const Value& value,
                              google::protobuf::MapKey& key) -> absl::Status {
          if (ABSL_PREDICT_FALSE(!value.Is<StringValue>())) {
            return TypeConversionError(*value.type(), to_key_type);
          }
          key.SetStringValue(value.As<StringValue>().ToString());
          return absl::OkStatus();
        };
      default:
        return absl::InternalError(
            absl::StrCat("unexpected protocol buffer map field key type: ",
                         google::protobuf::FieldDescriptor::TypeName(field_desc.type())));
    }
  }

  static absl::StatusOr<absl::AnyInvocable<
      absl::Status(ValueFactory&, const Value&, google::protobuf::MapValueRef&)>>
  GetMapFieldValueConverter(
      const google::protobuf::FieldDescriptor& field_desc, const Type& from_value_type,
      const Type& to_value_type ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    switch (field_desc.type()) {
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
        if (ABSL_PREDICT_FALSE(!from_value_type.Is<BoolType>() &&
                               !from_value_type.Is<DynType>())) {
          return TypeConversionError(from_value_type, to_value_type);
        }
        return
            [&to_value_type](ValueFactory&, const Value& value,
                             google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<BoolValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              value_ref.SetBoolValue(value.As<BoolValue>().value());
              return absl::OkStatus();
            };
      case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT32:
        if (ABSL_PREDICT_FALSE(!from_value_type.Is<IntType>() &&
                               !from_value_type.Is<DynType>())) {
          return TypeConversionError(from_value_type, to_value_type);
        }
        return
            [&to_value_type](ValueFactory&, const Value& value,
                             google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<IntValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              CEL_ASSIGN_OR_RETURN(auto raw_value,
                                   (CheckedCast<int64_t, int32_t>::Cast(
                                       value.As<IntValue>().value())));
              value_ref.SetInt32Value(raw_value);
              return absl::OkStatus();
            };
      case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT64:
        if (ABSL_PREDICT_FALSE(!from_value_type.Is<IntType>() &&
                               !from_value_type.Is<DynType>())) {
          return TypeConversionError(from_value_type, to_value_type);
        }
        return
            [&to_value_type](ValueFactory&, const Value& value,
                             google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<IntValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              value_ref.SetInt64Value(value.As<IntValue>().value());
              return absl::OkStatus();
            };
      case google::protobuf::FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
        if (ABSL_PREDICT_FALSE(!from_value_type.Is<UintType>() &&
                               !from_value_type.Is<DynType>())) {
          return TypeConversionError(from_value_type, to_value_type);
        }
        return
            [&to_value_type](ValueFactory&, const Value& value,
                             google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<UintValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              CEL_ASSIGN_OR_RETURN(auto raw_value,
                                   (CheckedCast<uint64_t, uint32_t>::Cast(
                                       value.As<UintValue>().value())));
              value_ref.SetUInt32Value(raw_value);
              return absl::OkStatus();
            };
      case google::protobuf::FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
        if (ABSL_PREDICT_FALSE(!from_value_type.Is<UintType>() &&
                               !from_value_type.Is<DynType>())) {
          return TypeConversionError(from_value_type, to_value_type);
        }
        return
            [&to_value_type](ValueFactory&, const Value& value,
                             google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<UintValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              value_ref.SetUInt64Value(value.As<UintValue>().value());
              return absl::OkStatus();
            };
      case google::protobuf::FieldDescriptor::TYPE_STRING:
        if (ABSL_PREDICT_FALSE(!from_value_type.Is<StringType>() &&
                               !from_value_type.Is<DynType>())) {
          return TypeConversionError(from_value_type, to_value_type);
        }
        return
            [&to_value_type](ValueFactory&, const Value& value,
                             google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<StringValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              value_ref.SetStringValue(value.As<StringValue>().ToString());
              return absl::OkStatus();
            };
      case google::protobuf::FieldDescriptor::TYPE_BYTES:
        if (ABSL_PREDICT_FALSE(!from_value_type.Is<BytesType>() &&
                               !from_value_type.Is<DynType>())) {
          return TypeConversionError(from_value_type, to_value_type);
        }
        return
            [&to_value_type](ValueFactory&, const Value& value,
                             google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<BytesValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              value_ref.SetStringValue(value.As<BytesValue>().ToString());
              return absl::OkStatus();
            };
      case google::protobuf::FieldDescriptor::TYPE_FLOAT:
        if (ABSL_PREDICT_FALSE(!from_value_type.Is<DoubleType>() &&
                               !from_value_type.Is<DynType>())) {
          return TypeConversionError(from_value_type, to_value_type);
        }
        return
            [&to_value_type](ValueFactory&, const Value& value,
                             google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<DoubleValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              CEL_ASSIGN_OR_RETURN(auto raw_value,
                                   (CheckedCast<double, float>::Cast(
                                       value.As<DoubleValue>().value())));
              value_ref.SetFloatValue(raw_value);
              return absl::OkStatus();
            };
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
        if (ABSL_PREDICT_FALSE(!from_value_type.Is<DoubleType>() &&
                               !from_value_type.Is<DynType>())) {
          return TypeConversionError(from_value_type, to_value_type);
        }
        return
            [&to_value_type](ValueFactory&, const Value& value,
                             google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<DoubleValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              value_ref.SetDoubleValue(value.As<DoubleValue>().value());
              return absl::OkStatus();
            };
      case google::protobuf::FieldDescriptor::TYPE_ENUM:
        if (to_value_type.Is<NullType>()) {
          if (ABSL_PREDICT_FALSE(!from_value_type.Is<NullType>() &&
                                 !from_value_type.Is<DynType>())) {
            return TypeConversionError(from_value_type, to_value_type);
          }
          return
              [&to_value_type](ValueFactory&, const Value& value,
                               google::protobuf::MapValueRef& value_ref) -> absl::Status {
                if (ABSL_PREDICT_FALSE(!value.Is<NullValue>())) {
                  return TypeConversionError(*value.type(), to_value_type);
                }
                value_ref.SetEnumValue(google::protobuf::NULL_VALUE);
                return absl::OkStatus();
              };
        }
        if (ABSL_PREDICT_FALSE(!from_value_type.Is<EnumType>() &&
                               !from_value_type.Is<IntType>() &&
                               !from_value_type.Is<DynType>())) {
          return TypeConversionError(from_value_type, to_value_type);
        }
        return
            [&to_value_type](ValueFactory&, const Value& value,
                             google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (value.Is<IntValue>()) {
                CEL_ASSIGN_OR_RETURN(auto raw_value,
                                     (CheckedCast<int64_t, int32_t>::Cast(
                                         value.As<IntValue>().value())));
                value_ref.SetEnumValue(static_cast<int>(raw_value));
              } else if (value.Is<EnumValue>()) {
                CEL_ASSIGN_OR_RETURN(auto raw_value,
                                     (CheckedCast<int64_t, int32_t>::Cast(
                                         value.As<EnumValue>().number())));
                value_ref.SetEnumValue(static_cast<int>(raw_value));
              } else {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return absl::OkStatus();
            };
      case google::protobuf::FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
        switch (to_value_type.kind()) {
          case TypeKind::kAny:
            // google.protobuf.Any
            return [](ValueFactory& value_factory, const Value& value,
                      google::protobuf::MapValueRef& value_ref) -> absl::Status {
              CEL_ASSIGN_OR_RETURN(auto any, value.ConvertToAny(value_factory));
              return protobuf_internal::WrapDynamicAnyProto(
                  any.type_url(), any.value(),
                  *value_ref.MutableMessageValue());
            };
          case TypeKind::kDyn:
            // google.protobuf.Value
            return [&to_value_type](
                       ValueFactory& value_factory, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              CEL_ASSIGN_OR_RETURN(auto json,
                                   value.ConvertToJson(value_factory));
              return protobuf_internal::DynamicValueProtoFromJson(
                  json, *value_ref.MutableMessageValue());
            };
          case TypeKind::kMap:
            // google.protobuf.Struct
            return [&to_value_type](
                       ValueFactory& value_factory, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (!value.Is<MapValue>() && !value.Is<StructValue>() &&
                  !value.Is<OpaqueValue>()) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              CEL_ASSIGN_OR_RETURN(auto json,
                                   value.ConvertToJson(value_factory));
              if (!absl::holds_alternative<JsonObject>(json)) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return protobuf_internal::DynamicStructProtoFromJson(
                  absl::get<JsonObject>(json),
                  *value_ref.MutableMessageValue());
            };
          case TypeKind::kList:
            // google.protobuf.ListValue
            return [&to_value_type](
                       ValueFactory& value_factory, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (!value.Is<ListValue>() && !value.Is<OpaqueValue>()) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              CEL_ASSIGN_OR_RETURN(auto json,
                                   value.ConvertToJson(value_factory));
              if (!absl::holds_alternative<JsonArray>(json)) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return protobuf_internal::DynamicListValueProtoFromJson(
                  absl::get<JsonArray>(json), *value_ref.MutableMessageValue());
            };
          case TypeKind::kDuration:
            // google.protobuf.Duration
            if (ABSL_PREDICT_FALSE(!from_value_type.Is<DurationType>() &&
                                   !from_value_type.Is<DynType>())) {
              return TypeConversionError(from_value_type, to_value_type);
            }
            return [&to_value_type](
                       ValueFactory&, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<DurationValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return protobuf_internal::WrapDynamicDurationProto(
                  value.As<DurationValue>().value(),
                  *value_ref.MutableMessageValue());
            };
          case TypeKind::kTimestamp:
            // google.protobuf.Timestamp
            if (ABSL_PREDICT_FALSE(!from_value_type.Is<TimestampType>() &&
                                   !from_value_type.Is<DynType>())) {
              return TypeConversionError(from_value_type, to_value_type);
            }
            return [&to_value_type](
                       ValueFactory&, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<TimestampValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return protobuf_internal::WrapDynamicTimestampProto(
                  value.As<TimestampValue>().value(),
                  *value_ref.MutableMessageValue());
            };
          case TypeKind::kBool:
            // google.protobuf.BoolValue
            if (ABSL_PREDICT_FALSE(!from_value_type.Is<BoolType>() &&
                                   !from_value_type.Is<DynType>())) {
              return TypeConversionError(from_value_type, to_value_type);
            }
            return [&to_value_type](
                       ValueFactory&, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<BoolValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return protobuf_internal::WrapDynamicBoolValueProto(
                  value.As<BoolValue>().value(),
                  *value_ref.MutableMessageValue());
            };
          case TypeKind::kInt:
            // google.protobuf.{Int32,Int64}Value
            if (ABSL_PREDICT_FALSE(!from_value_type.Is<IntType>() &&
                                   !from_value_type.Is<DynType>())) {
              return TypeConversionError(from_value_type, to_value_type);
            }
            return [&to_value_type](
                       ValueFactory&, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<IntValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return protobuf_internal::WrapDynamicSignedIntegralValueProto(
                  value.As<IntValue>().value(),
                  *value_ref.MutableMessageValue());
            };
          case TypeKind::kUint:
            // google.protobuf.{UInt32,UInt64}Value
            if (ABSL_PREDICT_FALSE(!from_value_type.Is<UintType>() &&
                                   !from_value_type.Is<DynType>())) {
              return TypeConversionError(from_value_type, to_value_type);
            }
            return [&to_value_type](
                       ValueFactory&, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<UintValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return protobuf_internal::WrapDynamicUnsignedIntegralValueProto(
                  value.As<UintValue>().value(),
                  *value_ref.MutableMessageValue());
            };
          case TypeKind::kDouble:
            // google.protobuf.{Float,Double}Value
            if (ABSL_PREDICT_FALSE(!from_value_type.Is<DoubleType>() &&
                                   !from_value_type.Is<DynType>())) {
              return TypeConversionError(from_value_type, to_value_type);
            }
            return [&to_value_type](
                       ValueFactory&, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<DoubleValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return protobuf_internal::WrapDynamicFloatingPointValueProto(
                  value.As<DoubleValue>().value(),
                  *value_ref.MutableMessageValue());
            };
          case TypeKind::kBytes:
            // google.protobuf.BytesValue
            if (ABSL_PREDICT_FALSE(!from_value_type.Is<BytesType>() &&
                                   !from_value_type.Is<DynType>())) {
              return TypeConversionError(from_value_type, to_value_type);
            }
            return [&to_value_type](
                       ValueFactory&, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<BytesValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return protobuf_internal::WrapDynamicBytesValueProto(
                  value.As<BytesValue>().ToCord(),
                  *value_ref.MutableMessageValue());
            };
          case TypeKind::kString:
            // google.protobuf.StringValue
            if (ABSL_PREDICT_FALSE(!from_value_type.Is<StringType>() &&
                                   !from_value_type.Is<DynType>())) {
              return TypeConversionError(from_value_type, to_value_type);
            }
            return [&to_value_type](
                       ValueFactory&, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<StringValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return protobuf_internal::WrapDynamicStringValueProto(
                  value.As<StringValue>().ToCord(),
                  *value_ref.MutableMessageValue());
            };
          case TypeKind::kStruct:
            if (ABSL_PREDICT_FALSE(!from_value_type.Is<StructType>() &&
                                   !from_value_type.Is<DynType>())) {
              return TypeConversionError(from_value_type, to_value_type);
            }
            return [&to_value_type](
                       ValueFactory&, const Value& value,
                       google::protobuf::MapValueRef& value_ref) -> absl::Status {
              if (ABSL_PREDICT_FALSE(!value.Is<ProtoStructValue>())) {
                return TypeConversionError(*value.type(), to_value_type);
              }
              return value.As<ProtoStructValue>().CopyTo(
                  *value_ref.MutableMessageValue());
            };
          default:
            return absl::InternalError(
                absl::StrCat("unexpected map field value type: ",
                             to_value_type.DebugString()));
        }
      default:
        return absl::InternalError(
            absl::StrCat("unexpected protocol buffer map field value type: ",
                         google::protobuf::FieldDescriptor::TypeName(field_desc.type())));
    }
  }

  absl::Status SetMapField(const StructType::Field& field,
                           const google::protobuf::Reflection& reflect,
                           const google::protobuf::FieldDescriptor& field_desc,
                           Handle<MapValue>&& value) {
    const auto* key_field_desc = field_desc.message_type()->map_key();
    const auto* value_field_desc = field_desc.message_type()->map_value();
    auto from_map_type_handle = value->type();
    const auto& from_map_type = *from_map_type_handle;
    const auto& from_key_type = *from_map_type.key();
    const auto& from_value_type = *from_map_type.value();
    const auto& to_map_type = field.type->As<MapType>();
    const auto& to_key_type = *to_map_type.key();
    const auto& to_value_type = *to_map_type.value();

    CEL_ASSIGN_OR_RETURN(
        auto key_converter,
        GetMapFieldKeyConverter(*key_field_desc, from_key_type, to_key_type));
    CEL_ASSIGN_OR_RETURN(
        auto value_converter,
        GetMapFieldValueConverter(*value_field_desc, from_value_type,
                                  to_value_type));

    CEL_ASSIGN_OR_RETURN(auto iterator, value->NewIterator(value_factory_));
    while (iterator->HasNext()) {
      CEL_ASSIGN_OR_RETURN(auto entry, iterator->Next());
      google::protobuf::MapKey map_key;
      CEL_RETURN_IF_ERROR(key_converter(*entry.key, map_key));
      google::protobuf::MapValueRef map_value;
      protobuf_internal::InsertOrLookupMapValue(reflect, message_, field_desc,
                                                map_key, &map_value);
      CEL_RETURN_IF_ERROR(
          value_converter(value_factory_, *entry.value, map_value));
    }
    return absl::OkStatus();
  }

  // Sets a repeated scalar field. `T` is a subclass of `cel::Type`,
  // `V` is a subclass of `cel::Value`, and `P` is the primitive C++ type.
  template <typename T, typename V, typename P>
  absl::Status SetRepeatedScalarField(
      const ListType& from_list_type, const ListType& to_list_type,
      const ListValue& value, const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc) {
    const auto& to_element_type = *to_list_type.element();
    const auto& from_element_type = *from_list_type.element();
    ABSL_DCHECK(to_element_type.Is<T>());
    if (ABSL_PREDICT_FALSE(!from_element_type.Is<T>() &&
                           !from_element_type.Is<DynType>())) {
      return TypeConversionError(from_list_type, to_list_type);
    }
    auto repeated_field_ref =
        reflect.GetMutableRepeatedFieldRef<P>(message_, &field_desc);
    repeated_field_ref.Clear();
    CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory_));
    while (iterator->HasNext()) {
      CEL_ASSIGN_OR_RETURN(auto element, iterator->NextValue());
      if (ABSL_PREDICT_FALSE(!element->Is<V>())) {
        return TypeConversionError(*element->type(), to_element_type);
      }
      CEL_ASSIGN_OR_RETURN(
          auto casted_element,
          (CheckedCast<typename base_internal::ValueTraits<V>::underlying_type,
                       P>::Cast(element->As<V>().value())));
      repeated_field_ref.Add(std::move(casted_element));
    }
    return absl::OkStatus();
  }

  // Sets a repeated string field. `T` is a subclass of `cel::Type` and
  // `V` is a subclass of `cel::Value`. This is used for bytes and strings.
  template <typename T, typename V>
  absl::Status SetRepeatedStringField(
      const ListType& from_list_type, const ListType& to_list_type,
      const ListValue& value, const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc) {
    const auto& to_element_type = *to_list_type.element();
    const auto& from_element_type = *from_list_type.element();
    ABSL_DCHECK(to_element_type.Is<T>());
    if (ABSL_PREDICT_FALSE(!from_element_type.Is<T>() &&
                           !from_element_type.Is<DynType>())) {
      return TypeConversionError(from_list_type, to_list_type);
    }
    auto repeated_field_ref =
        reflect.GetMutableRepeatedFieldRef<std::string>(message_, &field_desc);
    repeated_field_ref.Clear();
    CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory_));
    while (iterator->HasNext()) {
      CEL_ASSIGN_OR_RETURN(auto element, iterator->NextValue());
      if (ABSL_PREDICT_FALSE(!element->Is<V>())) {
        return TypeConversionError(*element->type(), to_element_type);
      }
      repeated_field_ref.Add(element->As<V>().ToString());
    }
    return absl::OkStatus();
  }

  // Sets an enum repeated field.
  absl::Status SetRepeatedEnumField(const ListType& from_list_type,
                                    const ListType& to_list_type,
                                    const ListValue& value,
                                    const google::protobuf::Reflection& reflect,
                                    const google::protobuf::FieldDescriptor& field_desc) {
    const auto& to_element_type = *to_list_type.element();
    const auto& from_element_type = *from_list_type.element();
    if (to_element_type.Is<NullType>()) {
      // google.protobuf.NullValue
      if (ABSL_PREDICT_FALSE(!from_element_type.Is<NullType>() &&
                             !from_element_type.Is<DynType>())) {
        return TypeConversionError(from_list_type, to_list_type);
      }
      auto repeated_field_ref =
          reflect.GetMutableRepeatedFieldRef<int32_t>(message_, &field_desc);
      repeated_field_ref.Clear();
      CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory_));
      while (iterator->HasNext()) {
        CEL_ASSIGN_OR_RETURN(auto element, iterator->NextValue());
        if (ABSL_PREDICT_FALSE(!element->Is<NullValue>())) {
          return TypeConversionError(*element->type(), to_element_type);
        }
        repeated_field_ref.Add(static_cast<int>(google::protobuf::NULL_VALUE));
      }
      return absl::OkStatus();
    }
    ABSL_DCHECK(to_element_type.Is<EnumType>());
    if (ABSL_PREDICT_FALSE(!from_element_type.Is<EnumType>() &&
                           !from_element_type.Is<IntType>() &&
                           !from_element_type.Is<DynType>())) {
      return TypeConversionError(from_list_type, to_list_type);
    }
    auto repeated_field_ref =
        reflect.GetMutableRepeatedFieldRef<int32_t>(message_, &field_desc);
    repeated_field_ref.Clear();
    CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory_));
    while (iterator->HasNext()) {
      CEL_ASSIGN_OR_RETURN(auto element, iterator->NextValue());
      if (element->Is<EnumValue>()) {
        if (ABSL_PREDICT_FALSE(element->As<EnumValue>().type()->name() !=
                               field_desc.enum_type()->full_name())) {
          return TypeConversionError(*element->type(), to_element_type);
        }
        int64_t raw_value = element->As<EnumValue>().number();
        if (ABSL_PREDICT_FALSE(raw_value < std::numeric_limits<int>::min() ||
                               raw_value > std::numeric_limits<int>::max())) {
          return absl::OutOfRangeError("int64 to int32_t overflow");
        }
        repeated_field_ref.Add(static_cast<int>(raw_value));
      } else if (element->Is<IntValue>()) {
        int64_t raw_value = element->As<IntValue>().value();
        if (ABSL_PREDICT_FALSE(raw_value < std::numeric_limits<int>::min() ||
                               raw_value > std::numeric_limits<int>::max())) {
          return absl::OutOfRangeError("int64 to int32_t overflow");
        }
        repeated_field_ref.Add(static_cast<int>(raw_value));
      } else {
        return TypeConversionError(*element->type(), to_element_type);
      }
    }
    return absl::OkStatus();
  }

  // Sets a `google::protobuf::Message` repeated field. `T` is a subclass of `cel::Type`,
  // `V` is a subclass of `cel::Value`, and `P` is the primitive C++ type.
  // `valuer` extracts the primitive C++ type from an instance of `V` and
  // `wrapper` converts the C++ primitive type to the protobuf wrapper type.
  template <typename T, typename V, typename P>
  absl::Status SetRepeatedWrapperMessageField(
      const ListType& from_list_type, const ListType& to_list_type,
      const Type& to_element_type, const ListValue& value,
      const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc,
      absl::FunctionRef<std::decay_t<P>(const V&)> valuer,
      absl::FunctionRef<absl::Status(P, google::protobuf::Message&)> wrapper) {
    const auto& from_element_type = *from_list_type.element();
    ABSL_DCHECK(to_element_type.Is<T>());
    if (ABSL_PREDICT_FALSE(!from_element_type.Is<T>() &&
                           !from_element_type.Is<DynType>())) {
      return TypeConversionError(from_list_type, to_list_type);
    }
    auto repeated_field_ref =
        reflect.GetMutableRepeatedFieldRef<google::protobuf::Message>(message_,
                                                            &field_desc);
    repeated_field_ref.Clear();
    CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory_));
    auto scratch = absl::WrapUnique(repeated_field_ref.NewMessage());
    while (iterator->HasNext()) {
      CEL_ASSIGN_OR_RETURN(auto element, iterator->NextValue());
      if (ABSL_PREDICT_FALSE(!element->Is<V>())) {
        return TypeConversionError(*element->type(), to_element_type);
      }
      scratch->Clear();
      CEL_RETURN_IF_ERROR(wrapper(valuer(element->As<V>()), *scratch));
      repeated_field_ref.Add(*scratch);
    }
    return absl::OkStatus();
  }

  absl::Status SetRepeatedMessageField(
      const ListType& from_list_type, const ListType& to_list_type,
      const ListValue& value, const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc) {
    const auto& to_element_type = *to_list_type.element();
    switch (to_element_type.kind()) {
      case TypeKind::kAny: {
        // google.protobuf.Any
        auto repeated_field_ref =
            reflect.GetMutableRepeatedFieldRef<google::protobuf::Message>(message_,
                                                                &field_desc);
        repeated_field_ref.Clear();
        CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory_));
        auto scratch = absl::WrapUnique(repeated_field_ref.NewMessage());
        while (iterator->HasNext()) {
          CEL_ASSIGN_OR_RETURN(auto element, iterator->NextValue());
          CEL_ASSIGN_OR_RETURN(auto any, element->ConvertToAny(value_factory_));
          scratch->Clear();
          CEL_RETURN_IF_ERROR(protobuf_internal::WrapDynamicAnyProto(
              any.type_url(), any.value(), *scratch));
          repeated_field_ref.Add(*scratch);
        }
        return absl::OkStatus();
      }
      case TypeKind::kDyn: {
        // google.protobuf.Value
        auto repeated_field_ref =
            reflect.GetMutableRepeatedFieldRef<google::protobuf::Message>(message_,
                                                                &field_desc);
        repeated_field_ref.Clear();
        CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory_));
        auto scratch = absl::WrapUnique(repeated_field_ref.NewMessage());
        while (iterator->HasNext()) {
          CEL_ASSIGN_OR_RETURN(auto element, iterator->NextValue());
          scratch->Clear();
          CEL_ASSIGN_OR_RETURN(auto json,
                               element->ConvertToJson(value_factory_));
          CEL_RETURN_IF_ERROR(
              protobuf_internal::DynamicValueProtoFromJson(json, *scratch));
          repeated_field_ref.Add(*scratch);
        }
        return absl::OkStatus();
      }
      case TypeKind::kList: {
        // google.protobuf.ListValue
        auto repeated_field_ref =
            reflect.GetMutableRepeatedFieldRef<google::protobuf::Message>(message_,
                                                                &field_desc);
        repeated_field_ref.Clear();
        CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory_));
        auto scratch = absl::WrapUnique(repeated_field_ref.NewMessage());
        while (iterator->HasNext()) {
          CEL_ASSIGN_OR_RETURN(auto element, iterator->NextValue());
          scratch->Clear();
          if (!element->Is<ListValue>() && !element->Is<OpaqueValue>()) {
            return TypeConversionError(*element->type(), to_element_type);
          }
          CEL_ASSIGN_OR_RETURN(auto json,
                               element->ConvertToJson(value_factory_));
          if (!absl::holds_alternative<JsonArray>(json)) {
            return TypeConversionError(*element->type(), to_element_type);
          }
          CEL_RETURN_IF_ERROR(protobuf_internal::DynamicListValueProtoFromJson(
              absl::get<JsonArray>(json), *scratch));
          repeated_field_ref.Add(*scratch);
        }
        return absl::OkStatus();
      }
      case TypeKind::kMap: {
        // google.protobuf.Struct
        auto repeated_field_ref =
            reflect.GetMutableRepeatedFieldRef<google::protobuf::Message>(message_,
                                                                &field_desc);
        repeated_field_ref.Clear();
        CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory_));
        auto scratch = absl::WrapUnique(repeated_field_ref.NewMessage());
        while (iterator->HasNext()) {
          CEL_ASSIGN_OR_RETURN(auto element, iterator->NextValue());
          scratch->Clear();
          if (!element->Is<MapValue>() && !element->Is<StructValue>() &&
              !element->Is<OpaqueValue>()) {
            return TypeConversionError(*element->type(), to_element_type);
          }
          CEL_ASSIGN_OR_RETURN(auto json,
                               element->ConvertToJson(value_factory_));
          if (!absl::holds_alternative<JsonObject>(json)) {
            return TypeConversionError(*element->type(), to_element_type);
          }
          CEL_RETURN_IF_ERROR(protobuf_internal::DynamicStructProtoFromJson(
              absl::get<JsonObject>(json), *scratch));
          repeated_field_ref.Add(*scratch);
        }
        return absl::OkStatus();
      }
      case TypeKind::kDuration:
        // google.protobuf.Duration
        return SetRepeatedWrapperMessageField<DurationType, DurationValue,
                                              absl::Duration>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc,
            [](const DurationValue& value) { return value.value(); },
            [](absl::Duration value, google::protobuf::Message& message) {
              return protobuf_internal::WrapDynamicDurationProto(value,
                                                                 message);
            });
      case TypeKind::kTimestamp:
        // google.protobuf.Timestamp
        return SetRepeatedWrapperMessageField<TimestampType, TimestampValue,
                                              absl::Time>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc,
            [](const TimestampValue& value) { return value.value(); },
            [](absl::Time value, google::protobuf::Message& message) {
              return protobuf_internal::WrapDynamicTimestampProto(value,
                                                                  message);
            });
      case TypeKind::kBool:
        // google.protobuf.BoolValue
        return SetRepeatedWrapperMessageField<BoolType, BoolValue, bool>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const BoolValue& value) { return value.value(); },
            protobuf_internal::WrapDynamicBoolValueProto);
      case TypeKind::kInt:
        // google.protobuf.{Int32,Int64}Value
        return SetRepeatedWrapperMessageField<IntType, IntValue, int64_t>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const IntValue& value) { return value.value(); },
            protobuf_internal::WrapDynamicSignedIntegralValueProto);
      case TypeKind::kUint:
        // google.protobuf.{UInt32,UInt64}Value
        return SetRepeatedWrapperMessageField<UintType, UintValue, uint64_t>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const UintValue& value) { return value.value(); },
            protobuf_internal::WrapDynamicUnsignedIntegralValueProto);
      case TypeKind::kDouble:
        // google.protobuf.{Float,Double}Value
        return SetRepeatedWrapperMessageField<DoubleType, DoubleValue, double>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const DoubleValue& value) { return value.value(); },
            protobuf_internal::WrapDynamicFloatingPointValueProto);
      case TypeKind::kBytes:
        // google.protobuf.BytesValue
        return SetRepeatedWrapperMessageField<BytesType, BytesValue,
                                              const absl::Cord&>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const BytesValue& value) { return value.ToCord(); },
            protobuf_internal::WrapDynamicBytesValueProto);
      case TypeKind::kString:
        // google.protobuf.StringValue
        return SetRepeatedWrapperMessageField<StringType, StringValue,
                                              const absl::Cord&>(
            from_list_type, to_list_type, to_element_type, value, reflect,
            field_desc, [](const StringValue& value) { return value.ToCord(); },
            protobuf_internal::WrapDynamicStringValueProto);
      case TypeKind::kStruct: {
        auto repeated_field_ref =
            reflect.GetMutableRepeatedFieldRef<google::protobuf::Message>(message_,
                                                                &field_desc);
        repeated_field_ref.Clear();
        CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory_));
        auto scratch = absl::WrapUnique(repeated_field_ref.NewMessage());
        while (iterator->HasNext()) {
          CEL_ASSIGN_OR_RETURN(auto element, iterator->NextValue());
          if (ABSL_PREDICT_FALSE(!element->Is<ProtoStructValue>())) {
            return TypeConversionError(*element->type(), to_element_type);
          }
          scratch->Clear();
          CEL_RETURN_IF_ERROR(element->As<ProtoStructValue>().CopyTo(*scratch));
          repeated_field_ref.Add(*scratch);
        }
        return absl::OkStatus();
      }
      default:
        return TypeConversionError(from_list_type, to_list_type);
    }
  }

  absl::Status SetRepeatedField(const StructType::Field& field,
                                const google::protobuf::Reflection& reflect,
                                const google::protobuf::FieldDescriptor& field_desc,
                                Handle<ListValue>&& value) {
    const auto& to_list_type = field.type->As<ListType>();
    auto from_list_type = value->type();
    switch (field_desc.type()) {
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
        return SetRepeatedScalarField<DoubleType, DoubleValue, double>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_FLOAT:
        return SetRepeatedScalarField<DoubleType, DoubleValue, float>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT64:
        return SetRepeatedScalarField<UintType, UintValue, uint64_t>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_BOOL:
        return SetRepeatedScalarField<BoolType, BoolValue, bool>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_STRING:
        return SetRepeatedStringField<StringType, StringValue>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
        return SetRepeatedMessageField(*from_list_type, to_list_type, *value,
                                       reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_BYTES:
        return SetRepeatedStringField<BytesType, BytesValue>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT32:
        return SetRepeatedScalarField<UintType, UintValue, uint32_t>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_ENUM:
        return SetRepeatedEnumField(*from_list_type, to_list_type, *value,
                                    reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT32:
        return SetRepeatedScalarField<IntType, IntValue, int32_t>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
      case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT64:
        return SetRepeatedScalarField<IntType, IntValue, int64_t>(
            *from_list_type, to_list_type, *value, reflect, field_desc);
    }
  }

  absl::Status SetSingularAnyField(const StructType::Field& field,
                                   const google::protobuf::Reflection& reflect,
                                   const google::protobuf::FieldDescriptor& field_desc,
                                   Handle<Value>&& value) {
    CEL_ASSIGN_OR_RETURN(auto any, value->ConvertToAny(value_factory_));
    return protobuf_internal::WrapDynamicAnyProto(
        any.type_url(), any.value(),
        *reflect.MutableMessage(message_, &field_desc, factory_));
  }

  absl::Status SetSingularMessageField(
      const StructType::Field& field, const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc, Handle<Value>&& value) {
    switch (field.type->kind()) {
      case TypeKind::kAny:
        return SetSingularAnyField(field, reflect, field_desc,
                                   std::move(value));
      case TypeKind::kDyn: {
        // google.protobuf.Value
        CEL_ASSIGN_OR_RETURN(auto json, value->ConvertToJson(value_factory_));
        return protobuf_internal::DynamicValueProtoFromJson(
            json, *reflect.MutableMessage(message_, &field_desc, factory_));
      }
      case TypeKind::kList: {
        // google.protobuf.ListValue
        // Only ListValue and OpaqueValue could feasibly serialize as a JSON
        // list.
        if (!value->Is<ListValue>() && !value->Is<OpaqueValue>()) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        CEL_ASSIGN_OR_RETURN(auto json, value->ConvertToJson(value_factory_));
        if (!absl::holds_alternative<JsonArray>(json)) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        return protobuf_internal::DynamicListValueProtoFromJson(
            absl::get<JsonArray>(json),
            *reflect.MutableMessage(message_, &field_desc, factory_));
      }
      case TypeKind::kMap: {
        // google.protobuf.Struct
        // Only MapValue, StructValue, and OpaqueValue could feasibly
        // serialize as a JSON object.
        if (!value->Is<MapValue>() && !value->Is<StructValue>() &&
            !value->Is<OpaqueValue>()) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        CEL_ASSIGN_OR_RETURN(auto json, value->ConvertToJson(value_factory_));
        if (!absl::holds_alternative<JsonObject>(json)) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        return protobuf_internal::DynamicStructProtoFromJson(
            absl::get<JsonObject>(json),
            *reflect.MutableMessage(message_, &field_desc, factory_));
      }
      case TypeKind::kDuration: {
        // google.protobuf.Duration
        if (ABSL_PREDICT_FALSE(!value->Is<DurationValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        return protobuf_internal::WrapDynamicDurationProto(
            value->As<DurationValue>().value(),
            *reflect.MutableMessage(message_, &field_desc, factory_));
      }
      case TypeKind::kTimestamp: {
        // google.protobuf.Timestamp
        if (ABSL_PREDICT_FALSE(!value->Is<TimestampValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        return protobuf_internal::WrapDynamicTimestampProto(
            value->As<TimestampValue>().value(),
            *reflect.MutableMessage(message_, &field_desc, factory_));
      }
      case TypeKind::kWrapper: {
        if (value->Is<NullValue>()) {
          reflect.ClearField(message_, &field_desc);
          return absl::OkStatus();
        }
        switch (field.type->As<WrapperType>().wrapped()->kind()) {
          case TypeKind::kBool: {
            // google.protobuf.BoolValue
            if (ABSL_PREDICT_FALSE(!value->Is<BoolValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapDynamicBoolValueProto(
                value->As<BoolValue>().value(),
                *reflect.MutableMessage(message_, &field_desc, factory_));
          }
          case TypeKind::kInt: {
            // google.protobuf.{Int32,Int64}Value
            if (ABSL_PREDICT_FALSE(!value->Is<IntValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapDynamicSignedIntegralValueProto(
                value->As<IntValue>().value(),
                *reflect.MutableMessage(message_, &field_desc, factory_));
          }
          case TypeKind::kUint: {
            // google.protobuf.{UInt32,UInt64}Value
            if (ABSL_PREDICT_FALSE(!value->Is<UintValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapDynamicUnsignedIntegralValueProto(
                value->As<UintValue>().value(),
                *reflect.MutableMessage(message_, &field_desc, factory_));
          }
          case TypeKind::kDouble: {
            // google.protobuf.{Float,Double}Value
            if (ABSL_PREDICT_FALSE(!value->Is<DoubleValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapDynamicFloatingPointValueProto(
                value->As<DoubleValue>().value(),
                *reflect.MutableMessage(message_, &field_desc, factory_));
          }
          case TypeKind::kBytes: {
            // google.protobuf.BytesValue
            if (ABSL_PREDICT_FALSE(!value->Is<BytesValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapDynamicBytesValueProto(
                value->As<BytesValue>().ToCord(),
                *reflect.MutableMessage(message_, &field_desc, factory_));
          }
          case TypeKind::kString: {
            // google.protobuf.StringValue
            if (ABSL_PREDICT_FALSE(!value->Is<StringValue>())) {
              return absl::InvalidArgumentError(absl::StrCat(
                  "type conversion error from ", field.type->DebugString(),
                  " to ", value->type()->DebugString()));
            }
            return protobuf_internal::WrapDynamicStringValueProto(
                value->As<StringValue>().ToCord(),
                *reflect.MutableMessage(message_, &field_desc, factory_));
          }
          default:
            // There are only 6 wrapper types.
            ABSL_UNREACHABLE();
        }
      }
      case TypeKind::kStruct: {
        if (ABSL_PREDICT_FALSE(!value->Is<ProtoStructValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        return value->As<ProtoStructValue>().CopyTo(
            *reflect.MutableMessage(message_, &field_desc, factory_));
      }
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "type conversion error from ", field.type->DebugString(), " to ",
            value->type()->DebugString()));
    }
  }

  absl::Status SetSingularField(const StructType::Field& field,
                                const google::protobuf::Reflection& reflect,
                                const google::protobuf::FieldDescriptor& field_desc,
                                Handle<Value>&& value) {
    switch (field_desc.type()) {
      case google::protobuf::FieldDescriptor::TYPE_DOUBLE: {
        if (ABSL_PREDICT_FALSE(!value->Is<DoubleValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        reflect.SetDouble(message_, &field_desc,
                          value->As<DoubleValue>().value());
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_FLOAT: {
        if (ABSL_PREDICT_FALSE(!value->Is<DoubleValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        auto raw_value = value->As<DoubleValue>().value();
        if (ABSL_PREDICT_FALSE(static_cast<double>(static_cast<float>(
                                   raw_value)) != raw_value)) {
          return absl::InvalidArgumentError("double to float overflow");
        }
        reflect.SetFloat(message_, &field_desc, static_cast<float>(raw_value));
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT64: {
        if (ABSL_PREDICT_FALSE(!value->Is<UintValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        reflect.SetUInt64(message_, &field_desc,
                          value->As<UintValue>().value());
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_BOOL: {
        if (ABSL_PREDICT_FALSE(!value->Is<BoolValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        reflect.SetBool(message_, &field_desc, value->As<BoolValue>().value());
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_STRING: {
        if (ABSL_PREDICT_FALSE(!value->Is<StringValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        if (protobuf_internal::IsCordField(field_desc)) {
          reflect.SetString(message_, &field_desc,
                            value->As<StringValue>().ToCord());
        } else {
          reflect.SetString(message_, &field_desc,
                            value->As<StringValue>().ToString());
        }
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
        return SetSingularMessageField(field, reflect, field_desc,
                                       std::move(value));
      case google::protobuf::FieldDescriptor::TYPE_BYTES: {
        if (ABSL_PREDICT_FALSE(!value->Is<BytesValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        if (protobuf_internal::IsCordField(field_desc)) {
          reflect.SetString(message_, &field_desc,
                            value->As<BytesValue>().ToCord());
        } else {
          reflect.SetString(message_, &field_desc,
                            value->As<BytesValue>().ToString());
        }
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_UINT32: {
        if (ABSL_PREDICT_FALSE(!value->Is<UintValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        auto raw_value = value->As<UintValue>().value();
        if (ABSL_PREDICT_FALSE(raw_value >
                               std::numeric_limits<uint32_t>::max())) {
          return absl::InvalidArgumentError("uint64 to uint32_t overflow");
        }
        reflect.SetUInt32(message_, &field_desc,
                          static_cast<uint32_t>(raw_value));
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_ENUM: {
        if (value->Is<NullValue>()) {
          // google.protobuf.NullValue
          reflect.SetEnumValue(message_, &field_desc,
                               google::protobuf::NULL_VALUE);
          return absl::OkStatus();
        }
        int64_t raw_value;
        if (value->Is<IntValue>()) {
          raw_value = value->As<IntValue>().value();
        } else if (value->Is<EnumValue>()) {
          raw_value = value->As<EnumValue>().number();
        } else {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        if (ABSL_PREDICT_FALSE(raw_value < std::numeric_limits<int>::min() ||
                               raw_value > std::numeric_limits<int>::max())) {
          return absl::InvalidArgumentError("int64 to int32_t overflow");
        }
        reflect.SetEnumValue(message_, &field_desc,
                             static_cast<int>(raw_value));
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT32: {
        if (ABSL_PREDICT_FALSE(!value->Is<IntValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        auto raw_value = value->As<IntValue>().value();
        if (ABSL_PREDICT_FALSE(
                raw_value < std::numeric_limits<int32_t>::min() ||
                raw_value > std::numeric_limits<int32_t>::max())) {
          return absl::InvalidArgumentError("int64 to int32_t overflow");
        }
        reflect.SetInt32(message_, &field_desc,
                         static_cast<int32_t>(raw_value));
        return absl::OkStatus();
      }
      case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::FieldDescriptor::TYPE_INT64: {
        if (ABSL_PREDICT_FALSE(!value->Is<IntValue>())) {
          return absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from ", field.type->DebugString(), " to ",
              value->type()->DebugString()));
        }
        reflect.SetInt64(message_, &field_desc, value->As<IntValue>().value());
        return absl::OkStatus();
      }
    }
  }

  ValueFactory& value_factory_;
  Handle<ProtoStructType> type_;
  google::protobuf::MessageFactory* const factory_;
  google::protobuf::Message* message_;
};

absl::StatusOr<UniqueRef<StructValueBuilderInterface>>
ProtoStructType::NewValueBuilder(ValueFactory& value_factory) const {
  const auto* prototype = factory_->GetPrototype(&descriptor());
  if (prototype == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Unable to retrieve prototype from protocol buffer "
                     "message factory for type ",
                     descriptor().full_name()));
  }
  google::protobuf::Message* message;
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    message = prototype->New(
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager()));
  } else {
    message = prototype->New();
  }
  return MakeUnique<ProtoStructValueBuilder>(
      value_factory.memory_manager(), value_factory,
      handle_from_this().As<ProtoStructType>(), factory_, message);
}

absl::StatusOr<Handle<StructValue>> ProtoStructType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  const auto* prototype = factory_->GetPrototype(&descriptor());
  if (prototype == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Unable to retrieve prototype from protocol buffer "
                     "message factory for type ",
                     descriptor().full_name()));
  }
  google::protobuf::Message* message;
  bool arena_based = ProtoMemoryManager::Is(value_factory.memory_manager());
  if (arena_based) {
    message = prototype->New(
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager()));
  } else {
    message = prototype->New();
  }
  if (!message->ParsePartialFromCord(value)) {
    return absl::InvalidArgumentError(absl::StrCat("Failed to parse ", name()));
  }
  auto status_or_value = protobuf_internal::ParsedProtoStructValue::Create(
      value_factory, std::move(*message));
  if (!arena_based) {
    delete message;
  }
  return status_or_value;
}

}  // namespace cel::extensions

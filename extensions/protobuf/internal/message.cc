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

#include "extensions/protobuf/internal/message.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/numeric/bits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "base/internal/message_wrapper.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/internal/reference_count.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "extensions/protobuf/internal/any.h"
#include "extensions/protobuf/internal/duration.h"
#include "extensions/protobuf/internal/json.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "extensions/protobuf/internal/qualify.h"
#include "extensions/protobuf/internal/struct.h"
#include "extensions/protobuf/internal/timestamp.h"
#include "extensions/protobuf/internal/wrappers.h"
#include "extensions/protobuf/json.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/align.h"
#include "internal/casts.h"
#include "internal/new.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"

namespace cel {

// Forward declare Value interfaces for implementing type traits.
namespace extensions::protobuf_internal {
namespace {
class PooledParsedProtoStructValueInterface;
class ParsedProtoListValueInterface;
class ParsedProtoMapValueInterface;
}  // namespace
}  // namespace extensions::protobuf_internal

template <>
struct NativeTypeTraits<
    extensions::protobuf_internal::PooledParsedProtoStructValueInterface> {
  static bool SkipDestructor(const extensions::protobuf_internal::
                                 PooledParsedProtoStructValueInterface&) {
    return true;
  }
};

template <>
struct NativeTypeTraits<
    extensions::protobuf_internal::ParsedProtoListValueInterface> {
  static bool SkipDestructor(
      const extensions::protobuf_internal::ParsedProtoListValueInterface&) {
    return true;
  }
};

template <>
struct NativeTypeTraits<
    extensions::protobuf_internal::ParsedProtoMapValueInterface> {
  static bool SkipDestructor(
      const extensions::protobuf_internal::ParsedProtoMapValueInterface&) {
    return true;
  }
};

namespace extensions::protobuf_internal {

namespace {

absl::StatusOr<absl::Nonnull<ArenaUniquePtr<google::protobuf::Message>>> NewProtoMessage(
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory, absl::string_view name,
    google::protobuf::Arena* arena) {
  const auto* desc = pool->FindMessageTypeByName(name);
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::NotFoundError(
        absl::StrCat("descriptor missing: `", name, "`"));
  }
  const auto* proto = factory->GetPrototype(desc);
  if (ABSL_PREDICT_FALSE(proto == nullptr)) {
    return absl::NotFoundError(absl::StrCat("prototype missing: `", name, "`"));
  }
  return ArenaUniquePtr<google::protobuf::Message>(proto->New(arena),
                                         DefaultArenaDeleter{arena});
}

absl::Status ProtoMapKeyTypeMismatch(google::protobuf::FieldDescriptor::CppType expected,
                                     google::protobuf::FieldDescriptor::CppType got) {
  if (ABSL_PREDICT_FALSE(got != expected)) {
    return absl::InternalError(
        absl::StrCat("protocol buffer map key type mismatch: ",
                     google::protobuf::FieldDescriptor::CppTypeName(expected), " vs ",
                     google::protobuf::FieldDescriptor::CppTypeName(got)));
  }
  return absl::OkStatus();
}

template <typename T>
class AliasingValue : public T {
 public:
  template <typename U, typename... Args>
  explicit AliasingValue(Shared<U> alias, Args&&... args)
      : T(std::forward<Args>(args)...), alias_(std::move(alias)) {}

 private:
  Shared<const void> alias_;
};

// -----------------------------------------------------------------------------
// cel::Value -> google::protobuf::MapKey

absl::Status ProtoBoolMapKeyFromValueConverter(const Value& value,
                                               google::protobuf::MapKey& key) {
  if (auto bool_value = As<BoolValue>(value); bool_value) {
    key.SetBoolValue(bool_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
}

absl::Status ProtoInt32MapKeyFromValueConverter(const Value& value,
                                                google::protobuf::MapKey& key) {
  if (auto int_value = As<IntValue>(value); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    key.SetInt32Value(static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoInt64MapKeyFromValueConverter(const Value& value,
                                                google::protobuf::MapKey& key) {
  if (auto int_value = As<IntValue>(value); int_value) {
    key.SetInt64Value(int_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoUInt32MapKeyFromValueConverter(const Value& value,
                                                 google::protobuf::MapKey& key) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return absl::OutOfRangeError("uint64 to uint32_t overflow");
    }
    key.SetUInt32Value(static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoUInt64MapKeyFromValueConverter(const Value& value,
                                                 google::protobuf::MapKey& key) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    key.SetUInt64Value(uint_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoStringMapKeyFromValueConverter(const Value& value,
                                                 google::protobuf::MapKey& key) {
  if (auto string_value = As<StringValue>(value); string_value) {
    key.SetStringValue(string_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "string").NativeValue();
}

}  // namespace

absl::StatusOr<ProtoMapKeyFromValueConverter> GetProtoMapKeyFromValueConverter(
    google::protobuf::FieldDescriptor::CppType cpp_type) {
  switch (cpp_type) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolMapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return ProtoStringMapKeyFromValueConverter;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected protocol buffer map key type: ",
                       google::protobuf::FieldDescriptor::CppTypeName(cpp_type)));
  }
}

namespace {

// -----------------------------------------------------------------------------
// google::protobuf::MapKey -> cel::Value

using ProtoMapKeyToValueConverter =
    absl::Status (*)(const google::protobuf::MapKeyConstRef&, ValueManager&, Value&);

absl::Status ProtoBoolMapKeyToValueConverter(const google::protobuf::MapKeyConstRef& key,
                                             ValueManager&, Value& result) {
  CEL_RETURN_IF_ERROR(ProtoMapKeyTypeMismatch(
      google::protobuf::FieldDescriptor::CPPTYPE_BOOL, key.type()));
  result = BoolValue{key.GetBoolValue()};
  return absl::OkStatus();
}

absl::Status ProtoInt32MapKeyToValueConverter(const google::protobuf::MapKeyConstRef& key,
                                              ValueManager&, Value& result) {
  CEL_RETURN_IF_ERROR(ProtoMapKeyTypeMismatch(
      google::protobuf::FieldDescriptor::CPPTYPE_INT32, key.type()));
  result = IntValue{key.GetInt32Value()};
  return absl::OkStatus();
}

absl::Status ProtoInt64MapKeyToValueConverter(const google::protobuf::MapKeyConstRef& key,
                                              ValueManager&, Value& result) {
  CEL_RETURN_IF_ERROR(ProtoMapKeyTypeMismatch(
      google::protobuf::FieldDescriptor::CPPTYPE_INT64, key.type()));
  result = IntValue{key.GetInt64Value()};
  return absl::OkStatus();
}

absl::Status ProtoUInt32MapKeyToValueConverter(
    const google::protobuf::MapKeyConstRef& key, ValueManager&, Value& result) {
  CEL_RETURN_IF_ERROR(ProtoMapKeyTypeMismatch(
      google::protobuf::FieldDescriptor::CPPTYPE_UINT32, key.type()));
  result = UintValue{key.GetUInt32Value()};
  return absl::OkStatus();
}

absl::Status ProtoUInt64MapKeyToValueConverter(
    const google::protobuf::MapKeyConstRef& key, ValueManager&, Value& result) {
  CEL_RETURN_IF_ERROR(ProtoMapKeyTypeMismatch(
      google::protobuf::FieldDescriptor::CPPTYPE_UINT64, key.type()));
  result = UintValue{key.GetUInt64Value()};
  return absl::OkStatus();
}

absl::Status ProtoStringMapKeyToValueConverter(
    const google::protobuf::MapKeyConstRef& key, ValueManager& value_manager,
    Value& result) {
  CEL_RETURN_IF_ERROR(ProtoMapKeyTypeMismatch(
      google::protobuf::FieldDescriptor::CPPTYPE_STRING, key.type()));
  result = StringValue{key.GetStringValue()};
  return absl::OkStatus();
}

absl::StatusOr<ProtoMapKeyToValueConverter> GetProtoMapKeyToValueConverter(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  ABSL_DCHECK(field->is_map());
  const auto* key_field = field->message_type()->map_key();
  switch (key_field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolMapKeyToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32MapKeyToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64MapKeyToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32MapKeyToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64MapKeyToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return ProtoStringMapKeyToValueConverter;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer map key type: ",
          google::protobuf::FieldDescriptor::CppTypeName(key_field->cpp_type())));
  }
}

// -----------------------------------------------------------------------------
// cel::Value -> google::protobuf::MapValueRef

absl::Status ProtoBoolMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto bool_value = As<BoolValue>(value); bool_value) {
    value_ref.SetBoolValue(bool_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
}

absl::Status ProtoInt32MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = As<IntValue>(value); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    value_ref.SetInt32Value(static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoInt64MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = As<IntValue>(value); int_value) {
    value_ref.SetInt64Value(int_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoUInt32MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return absl::OutOfRangeError("uint64 to uint32_t overflow");
    }
    value_ref.SetUInt32Value(static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoUInt64MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    value_ref.SetUInt64Value(uint_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoFloatMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto double_value = As<DoubleValue>(value); double_value) {
    value_ref.SetFloatValue(double_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "double").NativeValue();
}

absl::Status ProtoDoubleMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto double_value = As<DoubleValue>(value); double_value) {
    value_ref.SetDoubleValue(double_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "double").NativeValue();
}

absl::Status ProtoBytesMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto bytes_value = As<BytesValue>(value); bytes_value) {
    value_ref.SetStringValue(bytes_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bytes").NativeValue();
}

absl::Status ProtoStringMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto string_value = As<StringValue>(value); string_value) {
    value_ref.SetStringValue(string_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "string").NativeValue();
}

absl::Status ProtoNullMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (InstanceOf<NullValue>(value) || InstanceOf<IntValue>(value)) {
    value_ref.SetEnumValue(0);
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "google.protobuf.NullValue")
      .NativeValue();
}

absl::Status ProtoEnumMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = As<IntValue>(value); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    value_ref.SetEnumValue(static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "enum").NativeValue();
}

absl::Status ProtoMessageMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  return ProtoMessageFromValueImpl(value, value_ref.MutableMessageValue());
}

}  // namespace

absl::StatusOr<ProtoMapValueFromValueConverter>
GetProtoMapValueFromValueConverter(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  ABSL_DCHECK(field->is_map());
  const auto* value_field = field->message_type()->map_value();
  switch (value_field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return ProtoFloatMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return ProtoDoubleMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      if (value_field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        return ProtoBytesMapValueFromValueConverter;
      }
      return ProtoStringMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      if (value_field->enum_type()->full_name() ==
          "google.protobuf.NullValue") {
        return ProtoNullMapValueFromValueConverter;
      }
      return ProtoEnumMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoMessageMapValueFromValueConverter;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer map value type: ",
          google::protobuf::FieldDescriptor::CppTypeName(value_field->cpp_type())));
  }
}

namespace {

// -----------------------------------------------------------------------------
// google::protobuf::MapValueConstRef -> cel::Value

using ProtoMapValueToValueConverter = absl::Status (*)(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    const google::protobuf::MapValueConstRef&, ValueManager&, Value&);

absl::Status ProtoBoolMapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  result = BoolValue{value_ref.GetBoolValue()};
  return absl::OkStatus();
}

absl::Status ProtoInt32MapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  result = IntValue{value_ref.GetInt32Value()};
  return absl::OkStatus();
}

absl::Status ProtoInt64MapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.

  result = IntValue{value_ref.GetInt64Value()};
  return absl::OkStatus();
}

absl::Status ProtoUInt32MapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  result = UintValue{value_ref.GetUInt32Value()};
  return absl::OkStatus();
}

absl::Status ProtoUInt64MapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  result = UintValue{value_ref.GetUInt64Value()};
  return absl::OkStatus();
}

absl::Status ProtoFloatMapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  result = DoubleValue{value_ref.GetFloatValue()};
  return absl::OkStatus();
}

absl::Status ProtoDoubleMapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  result = DoubleValue{value_ref.GetDoubleValue()};
  return absl::OkStatus();
}

absl::Status ProtoBytesMapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  result = BytesValue{value_ref.GetStringValue()};
  return absl::OkStatus();
}

absl::Status ProtoStringMapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  result = StringValue{value_ref.GetStringValue()};
  return absl::OkStatus();
}

absl::Status ProtoNullMapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  result = NullValue{};
  return absl::OkStatus();
}

absl::Status ProtoEnumMapValueToValueConverter(
    SharedView<const void>, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  result = IntValue{value_ref.GetEnumValue()};
  return absl::OkStatus();
}

absl::Status ProtoMessageMapValueToValueConverter(
    SharedView<const void> alias,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueConstRef& value_ref, ValueManager& value_manager,
    Value& result) {
  // Caller validates that the field type is correct.
  CEL_ASSIGN_OR_RETURN(
      result, ProtoMessageToValueImpl(value_manager, Shared<const void>(alias),
                                      &value_ref.GetMessageValue()));
  return absl::OkStatus();
}

absl::StatusOr<ProtoMapValueToValueConverter> GetProtoMapValueToValueConverter(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  ABSL_DCHECK(field->is_map());
  const auto* value_field = field->message_type()->map_value();
  switch (value_field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolMapValueToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32MapValueToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64MapValueToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32MapValueToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64MapValueToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return ProtoFloatMapValueToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return ProtoDoubleMapValueToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      if (value_field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        return ProtoBytesMapValueToValueConverter;
      }
      return ProtoStringMapValueToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      if (value_field->enum_type()->full_name() ==
          "google.protobuf.NullValue") {
        return ProtoNullMapValueToValueConverter;
      }
      return ProtoEnumMapValueToValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoMessageMapValueToValueConverter;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer map value type: ",
          google::protobuf::FieldDescriptor::CppTypeName(value_field->cpp_type())));
  }
}

// -----------------------------------------------------------------------------
// repeated field -> Value

using ProtoRepeatedFieldToValueAccessor = absl::Status (*)(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*>,
    absl::Nonnull<const google::protobuf::Reflection*>,
    absl::Nonnull<const google::protobuf::FieldDescriptor*>, int, ValueManager&, Value&);

absl::Status ProtoBoolRepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager&, Value& result) {
  result = BoolValue{reflection->GetRepeatedBool(*message, field, index)};
  return absl::OkStatus();
}

absl::Status ProtoInt32RepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager&, Value& result) {
  result = IntValue{reflection->GetRepeatedInt32(*message, field, index)};
  return absl::OkStatus();
}

absl::Status ProtoInt64RepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager&, Value& result) {
  result = IntValue{reflection->GetRepeatedInt64(*message, field, index)};
  return absl::OkStatus();
}

absl::Status ProtoUInt32RepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager&, Value& result) {
  result = UintValue{reflection->GetRepeatedUInt32(*message, field, index)};
  return absl::OkStatus();
}

absl::Status ProtoUInt64RepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager&, Value& result) {
  result = UintValue{reflection->GetRepeatedUInt64(*message, field, index)};
  return absl::OkStatus();
}

absl::Status ProtoFloatRepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager&, Value& result) {
  result = DoubleValue{reflection->GetRepeatedFloat(*message, field, index)};
  return absl::OkStatus();
}

absl::Status ProtoDoubleRepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager&, Value& result) {
  result = DoubleValue{reflection->GetRepeatedDouble(*message, field, index)};
  return absl::OkStatus();
}

absl::Status ProtoBytesRepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager& value_manager, Value& result) {
  result = BytesValue{reflection->GetRepeatedString(*message, field, index)};
  return absl::OkStatus();
}

absl::Status ProtoStringRepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager& value_manager, Value& result) {
  result = value_manager.CreateUncheckedStringValue(
      reflection->GetRepeatedString(*message, field, index));
  return absl::OkStatus();
}

absl::Status ProtoNullRepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*>,
    absl::Nonnull<const google::protobuf::Reflection*>,
    absl::Nonnull<const google::protobuf::FieldDescriptor*>, int, ValueManager&,
    Value& result) {
  result = NullValue{};
  return absl::OkStatus();
}

absl::Status ProtoEnumRepeatedFieldToValueAccessor(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager& value_manager, Value& result) {
  result = IntValue{reflection->GetRepeatedEnumValue(*message, field, index)};
  return absl::OkStatus();
}

absl::Status ProtoMessageRepeatedFieldToValueAccessor(
    SharedView<const void> aliased,
    absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    ValueManager& value_manager, Value& result) {
  const auto& field_value =
      reflection->GetRepeatedMessage(*message, field, index);
  CEL_ASSIGN_OR_RETURN(
      result, ProtoMessageToValueImpl(
                  value_manager, Shared<const void>(aliased), &field_value));
  return absl::OkStatus();
}

absl::StatusOr<ProtoRepeatedFieldToValueAccessor>
GetProtoRepeatedFieldToValueAccessor(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  ABSL_DCHECK(!field->is_map());
  ABSL_DCHECK(field->is_repeated());
  switch (field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolRepeatedFieldToValueAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32RepeatedFieldToValueAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64RepeatedFieldToValueAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32RepeatedFieldToValueAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64RepeatedFieldToValueAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return ProtoFloatRepeatedFieldToValueAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return ProtoDoubleRepeatedFieldToValueAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        return ProtoBytesRepeatedFieldToValueAccessor;
      }
      return ProtoStringRepeatedFieldToValueAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
        return ProtoNullRepeatedFieldToValueAccessor;
      }
      return ProtoEnumRepeatedFieldToValueAccessor;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoMessageRepeatedFieldToValueAccessor;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer repeated field type: ",
          google::protobuf::FieldDescriptor::CppTypeName(field->cpp_type())));
  }
}

// -----------------------------------------------------------------------------
// field -> Value

absl::Status ProtoBoolFieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, ValueManager&,
    Value& result) {
  result = BoolValue{reflection->GetBool(*message, field)};
  return absl::OkStatus();
}

absl::Status ProtoInt32FieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, ValueManager&,
    Value& result) {
  result = IntValue{reflection->GetInt32(*message, field)};
  return absl::OkStatus();
}

absl::Status ProtoInt64FieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, ValueManager&,
    Value& result) {
  result = IntValue{reflection->GetInt64(*message, field)};
  return absl::OkStatus();
}

absl::Status ProtoUInt32FieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, ValueManager&,
    Value& result) {
  result = UintValue{reflection->GetUInt32(*message, field)};
  return absl::OkStatus();
}

absl::Status ProtoUInt64FieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, ValueManager&,
    Value& result) {
  result = UintValue{reflection->GetUInt64(*message, field)};
  return absl::OkStatus();
}

absl::Status ProtoFloatFieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, ValueManager&,
    Value& result) {
  result = DoubleValue{reflection->GetFloat(*message, field)};
  return absl::OkStatus();
}

absl::Status ProtoDoubleFieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, ValueManager&,
    Value& result) {
  result = DoubleValue{reflection->GetDouble(*message, field)};
  return absl::OkStatus();
}

absl::Status ProtoBytesFieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    ValueManager& value_manager, Value& result) {
  result = BytesValue{reflection->GetString(*message, field)};
  return absl::OkStatus();
}

absl::Status ProtoStringFieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    ValueManager& value_manager, Value& result) {
  result = StringValue{reflection->GetString(*message, field)};
  return absl::OkStatus();
}

absl::Status ProtoNullFieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*>,
    absl::Nonnull<const google::protobuf::Reflection*>,
    absl::Nonnull<const google::protobuf::FieldDescriptor*>, ValueManager&,
    Value& result) {
  result = NullValue{};
  return absl::OkStatus();
}

absl::Status ProtoEnumFieldToValue(
    SharedView<const void>, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, ValueManager&,
    Value& result) {
  result = IntValue{reflection->GetEnumValue(*message, field)};
  return absl::OkStatus();
}

bool IsWrapperType(absl::Nonnull<const google::protobuf::Descriptor*> descriptor) {
  switch (descriptor->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE:
      return true;
    default:
      return false;
  }
}

absl::Status ProtoMessageFieldToValue(
    SharedView<const void> aliased,
    absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    ValueManager& value_manager, Value& result,
    ProtoWrapperTypeOptions wrapper_type_options) {
  if (wrapper_type_options == ProtoWrapperTypeOptions::kUnsetNull &&
      IsWrapperType(field->message_type()) &&
      !reflection->HasField(*message, field)) {
    result = NullValue{};
    return absl::OkStatus();
  }
  const auto& field_value = reflection->GetMessage(*message, field);
  CEL_ASSIGN_OR_RETURN(
      result, ProtoMessageToValueImpl(
                  value_manager, Shared<const void>(aliased), &field_value));
  return absl::OkStatus();
}

absl::Status ProtoMapFieldToValue(
    SharedView<const void> aliased,
    absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    ValueManager& value_manager, Value& value);

absl::Status ProtoRepeatedFieldToValue(
    SharedView<const void> aliased,
    absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    ValueManager& value_manager, Value& value);

absl::Status ProtoFieldToValue(
    SharedView<const void> aliased,
    absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    ValueManager& value_manager, Value& result,
    ProtoWrapperTypeOptions wrapper_type_options) {
  if (field->is_map()) {
    return ProtoMapFieldToValue(aliased, message, reflection, field,
                                value_manager, result);
  }
  if (field->is_repeated()) {
    return ProtoRepeatedFieldToValue(aliased, message, reflection, field,
                                     value_manager, result);
  }
  switch (field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolFieldToValue(aliased, message, reflection, field,
                                   value_manager, result);
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32FieldToValue(aliased, message, reflection, field,
                                    value_manager, result);
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64FieldToValue(aliased, message, reflection, field,
                                    value_manager, result);
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32FieldToValue(aliased, message, reflection, field,
                                     value_manager, result);
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64FieldToValue(aliased, message, reflection, field,
                                     value_manager, result);
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return ProtoFloatFieldToValue(aliased, message, reflection, field,
                                    value_manager, result);
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return ProtoDoubleFieldToValue(aliased, message, reflection, field,
                                     value_manager, result);
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        return ProtoBytesFieldToValue(aliased, message, reflection, field,
                                      value_manager, result);
      }
      return ProtoStringFieldToValue(aliased, message, reflection, field,
                                     value_manager, result);
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      if (field->enum_type()->full_name() == "google.protobuf.NullValue") {
        return ProtoNullFieldToValue(aliased, message, reflection, field,
                                     value_manager, result);
      }
      return ProtoEnumFieldToValue(aliased, message, reflection, field,
                                   value_manager, result);
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoMessageFieldToValue(aliased, message, reflection, field,
                                      value_manager, result,
                                      wrapper_type_options);
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer repeated field type: ",
          google::protobuf::FieldDescriptor::CppTypeName(field->cpp_type())));
  }
}

bool IsValidFieldNumber(int64_t number) {
  return ABSL_PREDICT_TRUE(number > 0 &&
                           number < std::numeric_limits<int32_t>::max());
}

class ParsedProtoListElementIterator final : public ValueIterator {
 public:
  ParsedProtoListElementIterator(
      Shared<const void> aliasing, const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      ProtoRepeatedFieldToValueAccessor field_to_value_accessor)
      : aliasing_(std::move(aliasing)),
        message_(message),
        field_(field),
        field_to_value_accessor_(field_to_value_accessor),
        size_(GetReflectionOrDie(message_)->FieldSize(message, field_)) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(ValueManager& value_manager, Value& result) override {
    Value scratch;
    CEL_RETURN_IF_ERROR(field_to_value_accessor_(
        aliasing_, &message_, GetReflectionOrDie(message_), field_, index_,
        value_manager, result));
    ++index_;
    return absl::OkStatus();
  }

 private:
  Shared<const void> aliasing_;
  const google::protobuf::Message& message_;
  absl::Nonnull<const google::protobuf::FieldDescriptor*> field_;
  ProtoRepeatedFieldToValueAccessor field_to_value_accessor_;
  const int size_;
  int index_ = 0;
};

class ParsedProtoListValueInterface
    : public ParsedListValueInterface,
      public EnableSharedFromThis<ParsedProtoListValueInterface> {
 public:
  ParsedProtoListValueInterface(
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      ProtoRepeatedFieldToValueAccessor field_to_value_accessor)
      : message_(message),
        field_(field),
        field_to_value_accessor_(field_to_value_accessor) {}

  std::string DebugString() const final {
    google::protobuf::TextFormat::Printer printer;
    printer.SetSingleLineMode(true);
    printer.SetUseUtf8StringEscaping(true);
    std::string buffer;
    buffer.push_back('[');
    std::string output;
    const int count = GetReflectionOrDie(message_)->FieldSize(message_, field_);
    for (int index = 0; index < count; ++index) {
      if (index != 0) {
        buffer.append(", ");
      }
      printer.PrintFieldValueToString(message_, field_, index, &output);
      buffer.append(output);
    }
    buffer.push_back(']');
    return buffer;
  }

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter& converter) const final {
    return ProtoRepeatedFieldToJsonArray(
        converter, GetReflectionOrDie(message_), message_, field_);
  }

  size_t Size() const final {
    return static_cast<size_t>(
        GetReflectionOrDie(message_)->FieldSize(message_, field_));
  }

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const final {
    const auto size = Size();
    Value element;
    for (size_t index = 0; index < size; ++index) {
      CEL_RETURN_IF_ERROR(field_to_value_accessor_(
          shared_from_this(), &message_, GetReflectionOrDie(message_), field_,
          static_cast<int>(index), value_manager, element));
      CEL_ASSIGN_OR_RETURN(auto ok, callback(element));
      if (!ok) {
        break;
      }
    }
    return absl::OkStatus();
  }

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachWithIndexCallback callback) const final {
    const auto size = Size();
    Value element;
    for (size_t index = 0; index < size; ++index) {
      CEL_RETURN_IF_ERROR(field_to_value_accessor_(
          shared_from_this(), &message_, GetReflectionOrDie(message_), field_,
          static_cast<int>(index), value_manager, element));
      CEL_ASSIGN_OR_RETURN(auto ok, callback(index, element));
      if (!ok) {
        break;
      }
    }
    return absl::OkStatus();
  }

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const final {
    return std::make_unique<ParsedProtoListElementIterator>(
        shared_from_this(), message_, field_, field_to_value_accessor_);
  }

 private:
  absl::Status GetImpl(ValueManager& value_manager, size_t index,
                       Value& result) const final {
    Value scratch;
    CEL_RETURN_IF_ERROR(field_to_value_accessor_(
        shared_from_this(), &message_, GetReflectionOrDie(message_), field_,
        static_cast<int>(index), value_manager, result));
    return absl::OkStatus();
  }

  NativeTypeId GetNativeTypeId() const final {
    return NativeTypeId::For<ParsedProtoListValueInterface>();
  }

  const google::protobuf::Message& message_;
  absl::Nonnull<const google::protobuf::FieldDescriptor*> field_;
  ProtoRepeatedFieldToValueAccessor field_to_value_accessor_;
};

class ParsedProtoMapKeyIterator final : public ValueIterator {
 public:
  ParsedProtoMapKeyIterator(const google::protobuf::Message& message,
                            absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                            ProtoMapKeyToValueConverter map_key_to_value)
      : begin_(MapBegin(*GetReflectionOrDie(message), message, *field)),
        end_(MapEnd(*GetReflectionOrDie(message), message, *field)),
        map_key_to_value_(map_key_to_value) {}

  bool HasNext() override { return begin_ != end_; }

  absl::Status Next(ValueManager& value_manager, Value& result) override {
    Value scratch;
    CEL_RETURN_IF_ERROR(
        map_key_to_value_(begin_.GetKeyRef(), value_manager, result));
    ++begin_;
    return absl::OkStatus();
  }

 private:
  google::protobuf::MapIterator begin_;
  google::protobuf::MapIterator end_;
  ProtoMapKeyToValueConverter map_key_to_value_;
};

class ParsedProtoMapValueInterface
    : public ParsedMapValueInterface,
      public EnableSharedFromThis<ParsedProtoMapValueInterface> {
 public:
  ParsedProtoMapValueInterface(
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      ProtoMapKeyFromValueConverter map_key_from_value,
      ProtoMapKeyToValueConverter map_key_to_value,
      ProtoMapValueToValueConverter map_value_to_value)
      : message_(message),
        field_(field),
        map_key_from_value_(map_key_from_value),
        map_key_to_value_(map_key_to_value),
        map_value_to_value_(map_value_to_value) {}

  std::string DebugString() const final {
    google::protobuf::TextFormat::Printer printer;
    printer.SetSingleLineMode(true);
    printer.SetUseUtf8StringEscaping(true);
    std::string buffer;
    buffer.push_back('{');
    std::string output;
    const auto* reflection = GetReflectionOrDie(message_);
    const auto* map_key = field_->message_type()->map_key();
    const auto* map_value = field_->message_type()->map_value();
    const int count = reflection->FieldSize(message_, field_);
    for (int index = 0; index < count; ++index) {
      if (index != 0) {
        buffer.append(", ");
      }
      const auto& entry =
          reflection->GetRepeatedMessage(message_, field_, index);
      printer.PrintFieldValueToString(entry, map_key, -1, &output);
      buffer.append(output);
      buffer.append(": ");
      printer.PrintFieldValueToString(entry, map_value, -1, &output);
      buffer.append(output);
    }
    buffer.push_back('}');
    return buffer;
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter& converter) const final {
    return ProtoMapFieldToJsonObject(converter, GetReflectionOrDie(message_),
                                     message_, field_);
  }

  size_t Size() const final {
    return static_cast<size_t>(protobuf_internal::MapSize(
        *GetReflectionOrDie(message_), message_, *field_));
  }

  absl::Status ListKeys(ValueManager& value_manager,
                        ListValue& result) const final {
    CEL_ASSIGN_OR_RETURN(auto builder,
                         value_manager.NewListValueBuilder(ListType{}));
    builder->Reserve(Size());
    auto begin = MapBegin(*GetReflectionOrDie(message_), message_, *field_);
    auto end = MapEnd(*GetReflectionOrDie(message_), message_, *field_);
    Value key;
    while (begin != end) {
      CEL_RETURN_IF_ERROR(
          map_key_to_value_(begin.GetKeyRef(), value_manager, key));
      CEL_RETURN_IF_ERROR(builder->Add(std::move(key)));
      ++begin;
    }
    result = std::move(*builder).Build();
    return absl::OkStatus();
  }

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const final {
    auto begin = MapBegin(*GetReflectionOrDie(message_), message_, *field_);
    auto end = MapEnd(*GetReflectionOrDie(message_), message_, *field_);
    Value key;
    Value value;
    while (begin != end) {
      CEL_RETURN_IF_ERROR(
          map_key_to_value_(begin.GetKeyRef(), value_manager, key));
      CEL_RETURN_IF_ERROR(map_value_to_value_(shared_from_this(), field_,
                                              begin.GetValueRef(),
                                              value_manager, value));
      CEL_ASSIGN_OR_RETURN(auto ok, callback(key, value));
      if (!ok) {
        break;
      }
      ++begin;
    }
    return absl::OkStatus();
  }

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const final {
    return std::make_unique<ParsedProtoMapKeyIterator>(message_, field_,
                                                       map_key_to_value_);
  }

 private:
  absl::StatusOr<bool> FindImpl(ValueManager& value_manager, const Value& key,
                                Value& result) const final {
    google::protobuf::MapKey map_key;
    CEL_RETURN_IF_ERROR(map_key_from_value_(Value(key), map_key));
    google::protobuf::MapValueConstRef map_value;
    if (!LookupMapValue(*GetReflectionOrDie(message_), message_, *field_,
                        map_key, &map_value)) {
      return false;
    }
    Value scratch;
    CEL_RETURN_IF_ERROR(map_value_to_value_(shared_from_this(),
                                            field_->message_type()->map_value(),
                                            map_value, value_manager, result));
    return true;
  }

  absl::StatusOr<bool> HasImpl(ValueManager& value_manager,
                               const Value& key) const final {
    google::protobuf::MapKey map_key;
    CEL_RETURN_IF_ERROR(map_key_from_value_(Value(key), map_key));
    return ContainsMapKey(*GetReflectionOrDie(message_), message_, *field_,
                          map_key);
  }

  NativeTypeId GetNativeTypeId() const final {
    return NativeTypeId::For<ParsedProtoMapValueInterface>();
  }

  const google::protobuf::Message& message_;
  absl::Nonnull<const google::protobuf::FieldDescriptor*> field_;
  ProtoMapKeyFromValueConverter map_key_from_value_;
  ProtoMapKeyToValueConverter map_key_to_value_;
  ProtoMapValueToValueConverter map_value_to_value_;
};

class ParsedProtoQualifyState final : public ProtoQualifyState {
 public:
  ParsedProtoQualifyState(const google::protobuf::Message* message,
                          const google::protobuf::Descriptor* descriptor,
                          const google::protobuf::Reflection* reflection,
                          Shared<const void> alias, ValueManager& value_manager)
      : ProtoQualifyState(message, descriptor, reflection),
        alias_(std::move(alias)),
        value_manager_(value_manager) {}

  absl::optional<Value>& result() { return result_; }

 private:
  void SetResultFromError(absl::Status status,
                          cel::MemoryManagerRef memory_manager) override {
    result_ = ErrorValue{std::move(status)};
  }

  void SetResultFromBool(bool value) override { result_ = BoolValue{value}; }

  absl::Status SetResultFromField(
      const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManagerRef memory_manager) override {
    Value result;
    CEL_RETURN_IF_ERROR(
        ProtoFieldToValue(alias_, message, message->GetReflection(), field,
                          value_manager_, result, unboxing_option));
    result_ = std::move(result);
    return absl::OkStatus();
  }

  absl::Status SetResultFromRepeatedField(
      const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field,
      int index, cel::MemoryManagerRef memory_manager) override {
    Value result;
    CEL_ASSIGN_OR_RETURN(auto accessor,
                         GetProtoRepeatedFieldToValueAccessor(field));
    CEL_RETURN_IF_ERROR((*accessor)(alias_, message, message->GetReflection(),
                                    field, index, value_manager_, result));
    result_ = std::move(result);
    return absl::OkStatus();
  }

  absl::Status SetResultFromMapField(
      const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field,
      const google::protobuf::MapValueConstRef& value,
      cel::MemoryManagerRef memory_manager) override {
    CEL_ASSIGN_OR_RETURN(auto converter,
                         GetProtoMapValueToValueConverter(field));
    Value result;
    CEL_RETURN_IF_ERROR(
        (*converter)(alias_, field, value, value_manager_, result));
    result_ = std::move(result);
    return absl::OkStatus();
  }

  Shared<const void> alias_;
  ValueManager& value_manager_;
  absl::optional<Value> result_;
};

class ParsedProtoStructValueInterface;

const ParsedProtoStructValueInterface* AsParsedProtoStructValue(
    const ParsedStructValue& value);

const ParsedProtoStructValueInterface* AsParsedProtoStructValue(
    const Value& value) {
  if (auto parsed_struct_value = As<ParsedStructValue>(value);
      parsed_struct_value) {
    return AsParsedProtoStructValue(*parsed_struct_value);
  }
  return nullptr;
}

class ParsedProtoStructValueInterface
    : public ParsedStructValueInterface,
      public EnableSharedFromThis<ParsedProtoStructValueInterface> {
 public:
  absl::string_view GetTypeName() const final {
    return message().GetDescriptor()->full_name();
  }

  std::string DebugString() const final { return message().DebugString(); }

  // `SerializeTo` serializes this value and appends it to `value`. If this
  // value does not support serialization, `FAILED_PRECONDITION` is returned.
  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const final {
    if (!message().SerializePartialToCord(&value)) {
      return absl::InternalError(
          absl::StrCat("failed to serialize ", GetTypeName()));
    }
    return absl::OkStatus();
  }

  absl::StatusOr<Json> ConvertToJson(
      AnyToJsonConverter& value_manager) const final {
    return ProtoMessageToJson(value_manager, message());
  }

  bool IsZeroValue() const final { return message().ByteSizeLong() == 0; }

  absl::Status GetFieldByName(
      ValueManager& value_manager, absl::string_view name, Value& result,
      ProtoWrapperTypeOptions unboxing_options) const final {
    const auto* desc = message().GetDescriptor();
    const auto* field_desc = desc->FindFieldByName(name);
    if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
      field_desc = message().GetReflection()->FindKnownExtensionByName(name);
      if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
        result = NoSuchFieldError(name);
        return absl::OkStatus();
      }
    }
    return GetField(value_manager, field_desc, result, unboxing_options);
  }

  absl::Status GetFieldByNumber(
      ValueManager& value_manager, int64_t number, Value& result,
      ProtoWrapperTypeOptions unboxing_options) const final {
    if (!IsValidFieldNumber(number)) {
      result = NoSuchFieldError(absl::StrCat(number));
      return absl::OkStatus();
    }
    const auto* desc = message().GetDescriptor();
    const auto* field_desc = desc->FindFieldByNumber(static_cast<int>(number));
    if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
      result = NoSuchFieldError(absl::StrCat(number));
      return absl::OkStatus();
    }
    return GetField(value_manager, field_desc, result, unboxing_options);
  }

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const final {
    const auto* desc = message().GetDescriptor();
    const auto* field_desc = desc->FindFieldByName(name);
    if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
      field_desc = message().GetReflection()->FindKnownExtensionByName(name);
      if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
        return NoSuchFieldError(name).NativeValue();
      }
    }
    return HasField(field_desc);
  }

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const final {
    if (!IsValidFieldNumber(number)) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    const auto* desc = message().GetDescriptor();
    const auto* field_desc = desc->FindFieldByNumber(static_cast<int>(number));
    if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
      return NoSuchFieldError(absl::StrCat(number)).NativeValue();
    }
    return HasField(field_desc);
  }

  absl::Status ForEachField(ValueManager& value_manager,
                            ForEachFieldCallback callback) const final {
    std::vector<const google::protobuf::FieldDescriptor*> fields;
    const auto* reflection = message().GetReflection();
    reflection->ListFields(message(), &fields);
    Value value;
    for (const auto* field : fields) {
      CEL_RETURN_IF_ERROR(ProtoFieldToValue(
          shared_from_this(), &message(), reflection, field, value_manager,
          value, ProtoWrapperTypeOptions::kUnsetProtoDefault));
      CEL_ASSIGN_OR_RETURN(auto ok, callback(field->name(), value));
      if (!ok) {
        break;
      }
    }
    return absl::OkStatus();
  }

  absl::StatusOr<int> Qualify(ValueManager& value_manager,
                              absl::Span<const SelectQualifier> qualifiers,
                              bool presence_test, Value& result) const final {
    if (ABSL_PREDICT_FALSE(qualifiers.empty())) {
      return absl::InvalidArgumentError("invalid select qualifier path.");
    }
    auto memory_manager = value_manager.GetMemoryManager();
    ParsedProtoQualifyState qualify_state(&message(), message().GetDescriptor(),
                                          message().GetReflection(),
                                          shared_from_this(), value_manager);
    for (int i = 0; i < qualifiers.size() - 1; i++) {
      const auto& qualifier = qualifiers[i];
      CEL_RETURN_IF_ERROR(
          qualify_state.ApplySelectQualifier(qualifier, memory_manager));
      if (qualify_state.result().has_value()) {
        result = std::move(qualify_state.result()).value();
        return result.Is<ErrorValue>() ? -1 : i + 1;
      }
    }
    const auto& last_qualifier = qualifiers.back();
    if (presence_test) {
      CEL_RETURN_IF_ERROR(
          qualify_state.ApplyLastQualifierHas(last_qualifier, memory_manager));
    } else {
      CEL_RETURN_IF_ERROR(
          qualify_state.ApplyLastQualifierGet(last_qualifier, memory_manager));
    }
    result = std::move(qualify_state.result()).value();
    return -1;
  }

  virtual const google::protobuf::Message& message() const = 0;

  StructType GetRuntimeType() const final {
    return MessageType(message().GetDescriptor());
  }

 private:
  absl::Status EqualImpl(ValueManager& value_manager,
                         const ParsedStructValue& other,
                         Value& result) const final {
    if (const auto* parsed_proto_struct_value = AsParsedProtoStructValue(other);
        parsed_proto_struct_value) {
      const auto& lhs_message = message();
      const auto& rhs_message = parsed_proto_struct_value->message();
      if (lhs_message.GetDescriptor() == rhs_message.GetDescriptor()) {
        result = BoolValue{
            google::protobuf::util::MessageDifferencer::Equals(lhs_message, rhs_message)};
        return absl::OkStatus();
      }
    }
    return ParsedStructValueInterface::EqualImpl(value_manager, other, result);
  }

  NativeTypeId GetNativeTypeId() const final {
    return NativeTypeId::For<ParsedProtoStructValueInterface>();
  }

  absl::StatusOr<bool> HasField(
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field_desc) const {
    const auto* reflect = message().GetReflection();
    if (field_desc->is_map() || field_desc->is_repeated()) {
      return reflect->FieldSize(message(), field_desc) > 0;
    }
    return reflect->HasField(message(), field_desc);
  }

  absl::Status GetField(
      ValueManager& value_manager,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field_desc, Value& result,
      ProtoWrapperTypeOptions unboxing_options) const {
    CEL_RETURN_IF_ERROR(ProtoFieldToValue(
        shared_from_this(), &message(), message().GetReflection(), field_desc,
        value_manager, result, unboxing_options));
    return absl::OkStatus();
  }
};

const ParsedProtoStructValueInterface* AsParsedProtoStructValue(
    const ParsedStructValue& value) {
  return NativeTypeId::Of(value) ==
                 NativeTypeId::For<ParsedProtoStructValueInterface>()
             ? cel::internal::down_cast<const ParsedProtoStructValueInterface*>(
                   value.operator->())
             : nullptr;
}

class PooledParsedProtoStructValueInterface final
    : public ParsedProtoStructValueInterface {
 public:
  explicit PooledParsedProtoStructValueInterface(
      absl::Nonnull<const google::protobuf::Message*> message)
      : message_(message) {}

  const google::protobuf::Message& message() const override { return *message_; }

 private:
  absl::Nonnull<const google::protobuf::Message*> message_;
};

class AliasingParsedProtoStructValueInterface final
    : public ParsedProtoStructValueInterface {
 public:
  explicit AliasingParsedProtoStructValueInterface(
      absl::Nonnull<const google::protobuf::Message*> message, Shared<const void> alias)
      : message_(message), alias_(std::move(alias)) {}

  const google::protobuf::Message& message() const override { return *message_; }

 private:
  absl::Nonnull<const google::protobuf::Message*> message_;
  Shared<const void> alias_;
};

// Reference counted `ParsedProtoStructValueInterface`. Used when we know the
// concrete message type.
class ReffedStaticParsedProtoStructValueInterface final
    : public ParsedProtoStructValueInterface,
      public common_internal::ReferenceCount {
 public:
  explicit ReffedStaticParsedProtoStructValueInterface(size_t size)
      : size_(size) {}

  const google::protobuf::Message& message() const override {
    return *static_cast<const google::protobuf::Message*>(
        reinterpret_cast<const google::protobuf::MessageLite*>(
            reinterpret_cast<const char*>(this) + MessageOffset()));
  }

  static size_t MessageOffset() {
    return internal::AlignUp(
        sizeof(ReffedStaticParsedProtoStructValueInterface),
        __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  }

 private:
  void Finalize() noexcept override {
    reinterpret_cast<google::protobuf::MessageLite*>(reinterpret_cast<char*>(this) +
                                           MessageOffset())
        ->~MessageLite();
  }

  void Delete() noexcept override {
    void* address = this;
    const auto size = MessageOffset() + size_;
    this->~ReffedStaticParsedProtoStructValueInterface();
    internal::SizedDelete(address, size);
  }

  const size_t size_;
};

// Reference counted `ParsedProtoStructValueInterface`. Used when we do not know
// the concrete message type.
class ReffedDynamicParsedProtoStructValueInterface final
    : public ParsedProtoStructValueInterface,
      public common_internal::ReferenceCount {
 public:
  explicit ReffedDynamicParsedProtoStructValueInterface(
      absl::Nonnull<const google::protobuf::Message*> message)
      : message_(message) {}

  const google::protobuf::Message& message() const override { return *message_; }

 private:
  void Finalize() noexcept override { delete message_; }

  void Delete() noexcept override { delete this; }

  absl::Nonnull<const google::protobuf::Message*> message_;
};

void ProtoMessageDestruct(void* object) {
  static_cast<google::protobuf::Message*>(object)->~Message();
}

absl::Status ProtoMapFieldToValue(
    SharedView<const void> aliased,
    absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    ValueManager& value_manager, Value& value) {
  ABSL_DCHECK(field->is_map());
  CEL_ASSIGN_OR_RETURN(auto map_key_from_value,
                       GetProtoMapKeyFromValueConverter(
                           field->message_type()->map_key()->cpp_type()));
  CEL_ASSIGN_OR_RETURN(auto map_key_to_value,
                       GetProtoMapKeyToValueConverter(field));
  CEL_ASSIGN_OR_RETURN(auto map_value_to_value,
                       GetProtoMapValueToValueConverter(field));
  if (!aliased) {
    value = ParsedMapValue{value_manager.GetMemoryManager()
                               .MakeShared<ParsedProtoMapValueInterface>(
                                   *message, field, map_key_from_value,
                                   map_key_to_value, map_value_to_value)};
  } else {
    value = ParsedMapValue{
        value_manager.GetMemoryManager()
            .MakeShared<AliasingValue<ParsedProtoMapValueInterface>>(
                Shared<const void>(aliased), *message, field,
                map_key_from_value, map_key_to_value, map_value_to_value)};
  }
  return absl::OkStatus();
}

absl::Status ProtoRepeatedFieldToValue(
    SharedView<const void> aliased,
    absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    ValueManager& value_manager, Value& value) {
  ABSL_DCHECK(!field->is_map());
  ABSL_DCHECK(field->is_repeated());
  CEL_ASSIGN_OR_RETURN(auto repeated_field_to_value,
                       GetProtoRepeatedFieldToValueAccessor(field));
  if (!aliased) {
    value = ParsedListValue{value_manager.GetMemoryManager()
                                .MakeShared<ParsedProtoListValueInterface>(
                                    *message, field, repeated_field_to_value)};
  } else {
    value = ParsedListValue{
        value_manager.GetMemoryManager()
            .MakeShared<AliasingValue<ParsedProtoListValueInterface>>(
                Shared<const void>(aliased), *message, field,
                repeated_field_to_value)};
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::optional<Value>> WellKnownProtoMessageToValue(
    ValueFactory& value_factory, const TypeReflector& type_reflector,
    absl::Nonnull<const google::protobuf::Message*> message) {
  const auto* desc = message->GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::nullopt;
  }
  switch (desc->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicFloatValueProto(*message));
      return DoubleValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicDoubleValueProto(*message));
      return DoubleValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicInt32ValueProto(*message));
      return IntValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicInt64ValueProto(*message));
      return IntValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicUInt32ValueProto(*message));
      return UintValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicUInt64ValueProto(*message));
      return UintValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicStringValueProto(*message));
      return StringValue{std::move(value)};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicBytesValueProto(*message));
      return BytesValue{std::move(value)};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicBoolValueProto(*message));
      return BoolValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY: {
      CEL_ASSIGN_OR_RETURN(auto any, UnwrapDynamicAnyProto(*message));
      CEL_ASSIGN_OR_RETURN(auto value, type_reflector.DeserializeValue(
                                           value_factory, any.type_url(),
                                           GetAnyValueAsCord(any)));
      if (!value) {
        return absl::NotFoundError(
            absl::StrCat("unable to find deserializer for ", any.type_url()));
      }
      return std::move(value).value();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicDurationProto(*message));
      return DurationValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicTimestampProto(*message));
      return TimestampValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, DynamicValueProtoToJson(*message));
      return value_factory.CreateValueFromJson(std::move(value));
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, DynamicListValueProtoToJson(*message));
      return value_factory.CreateValueFromJson(std::move(value));
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT: {
      CEL_ASSIGN_OR_RETURN(auto value, DynamicStructProtoToJson(*message));
      return value_factory.CreateValueFromJson(std::move(value));
    }
    default:
      return absl::nullopt;
  }
}

absl::StatusOr<absl::optional<Value>> WellKnownProtoMessageToValue(
    ValueManager& value_manager,
    absl::Nonnull<const google::protobuf::Message*> message) {
  return WellKnownProtoMessageToValue(value_manager,
                                      value_manager.type_provider(), message);
}

absl::Status ProtoMessageCopyUsingSerialization(
    google::protobuf::MessageLite* to, const google::protobuf::MessageLite* from) {
  ABSL_DCHECK_EQ(to->GetTypeName(), from->GetTypeName());
  absl::Cord serialized;
  if (!from->SerializePartialToCord(&serialized)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize `", from->GetTypeName(), "`"));
  }
  if (!to->ParsePartialFromCord(serialized)) {
    return absl::UnknownError(
        absl::StrCat("failed to parse `", to->GetTypeName(), "`"));
  }
  return absl::OkStatus();
}

absl::Status ProtoMessageCopy(
    absl::Nonnull<google::protobuf::Message*> to_message,
    absl::Nonnull<const google::protobuf::Descriptor*> to_descriptor,
    absl::Nonnull<const google::protobuf::Message*> from_message) {
  CEL_ASSIGN_OR_RETURN(const auto* from_descriptor,
                       GetDescriptor(*from_message));
  if (to_descriptor == from_descriptor) {
    // Same.
    to_message->CopyFrom(*from_message);
    return absl::OkStatus();
  }
  if (to_descriptor->full_name() == from_descriptor->full_name()) {
    // Same type, different descriptors.
    return ProtoMessageCopyUsingSerialization(to_message, from_message);
  }
  return TypeConversionError(from_descriptor->full_name(),
                             to_descriptor->full_name())
      .NativeValue();
}

absl::Status ProtoMessageCopy(
    absl::Nonnull<google::protobuf::Message*> to_message,
    absl::Nonnull<const google::protobuf::Descriptor*> to_descriptor,
    absl::Nonnull<const google::protobuf::MessageLite*> from_message) {
  const auto& from_type_name = from_message->GetTypeName();
  if (from_type_name == to_descriptor->full_name()) {
    return ProtoMessageCopyUsingSerialization(to_message, from_message);
  }
  return TypeConversionError(from_type_name, to_descriptor->full_name())
      .NativeValue();
}

}  // namespace

absl::StatusOr<absl::Nonnull<const google::protobuf::Descriptor*>> GetDescriptor(
    const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat(message.GetTypeName(), " is missing descriptor"));
  }
  return desc;
}

absl::StatusOr<absl::Nonnull<const google::protobuf::Reflection*>> GetReflection(
    const google::protobuf::Message& message) {
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat(message.GetTypeName(), " is missing reflection"));
  }
  return reflect;
}

absl::Nonnull<const google::protobuf::Reflection*> GetReflectionOrDie(
    const google::protobuf::Message& message) {
  const auto* reflection = message.GetReflection();
  ABSL_CHECK(reflection != nullptr)  // Crash OK
      << message.GetTypeName() << " is missing reflection";
  return reflection;
}

absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueManager& value_manager, absl::Nonnull<const google::protobuf::Message*> message,
    size_t size, size_t align,
    absl::Nonnull<ProtoMessageArenaCopyConstructor> arena_copy_construct,
    absl::Nonnull<ProtoMessageCopyConstructor> copy_construct) {
  ABSL_DCHECK_GT(size, 0);
  ABSL_DCHECK(absl::has_single_bit(align));
  {
    CEL_ASSIGN_OR_RETURN(auto well_known,
                         WellKnownProtoMessageToValue(value_manager, message));
    if (well_known) {
      return std::move(well_known).value();
    }
  }
  auto memory_manager = value_manager.GetMemoryManager();
  if (auto* arena = ProtoMemoryManagerArena(memory_manager); arena != nullptr) {
    auto* copied_message = (*arena_copy_construct)(arena, message);
    return ParsedStructValue{
        memory_manager.MakeShared<PooledParsedProtoStructValueInterface>(
            copied_message)};
  }
  switch (memory_manager.memory_management()) {
    case MemoryManagement::kPooling: {
      auto* copied_message =
          (*copy_construct)(memory_manager.Allocate(size, align), message);
      memory_manager.OwnCustomDestructor(copied_message, &ProtoMessageDestruct);
      return ParsedStructValue{
          memory_manager.MakeShared<PooledParsedProtoStructValueInterface>(
              copied_message)};
    }
    case MemoryManagement::kReferenceCounting: {
      auto* block = static_cast<char*>(memory_manager.Allocate(
          ReffedStaticParsedProtoStructValueInterface::MessageOffset() + size,
          __STDCPP_DEFAULT_NEW_ALIGNMENT__));
      auto* message_address =
          block + ReffedStaticParsedProtoStructValueInterface::MessageOffset();
      auto* copied_message = (*copy_construct)(message_address, message);
      ABSL_DCHECK_EQ(reinterpret_cast<uintptr_t>(message_address),
                     reinterpret_cast<uintptr_t>(copied_message));
      auto* message_value = ::new (static_cast<void*>(block))
          ReffedStaticParsedProtoStructValueInterface(size);
      common_internal::SetReferenceCountForThat(*message_value, message_value);
      return ParsedStructValue{common_internal::MakeShared(
          common_internal::kAdoptRef, message_value, message_value)};
    }
  }
}

absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueManager& value_manager, absl::Nonnull<google::protobuf::Message*> message,
    size_t size, size_t align,
    absl::Nonnull<ProtoMessageArenaMoveConstructor> arena_move_construct,
    absl::Nonnull<ProtoMessageMoveConstructor> move_construct) {
  ABSL_DCHECK_GT(size, 0);
  ABSL_DCHECK(absl::has_single_bit(align));
  {
    CEL_ASSIGN_OR_RETURN(auto well_known,
                         WellKnownProtoMessageToValue(value_manager, message));
    if (well_known) {
      return std::move(well_known).value();
    }
  }
  auto memory_manager = value_manager.GetMemoryManager();
  if (auto* arena = ProtoMemoryManagerArena(memory_manager); arena != nullptr) {
    auto* moved_message = (*arena_move_construct)(arena, message);
    return ParsedStructValue{
        memory_manager.MakeShared<PooledParsedProtoStructValueInterface>(
            moved_message)};
  }
  switch (memory_manager.memory_management()) {
    case MemoryManagement::kPooling: {
      auto* moved_message =
          (*move_construct)(memory_manager.Allocate(size, align), message);
      memory_manager.OwnCustomDestructor(moved_message, &ProtoMessageDestruct);
      return ParsedStructValue{
          memory_manager.MakeShared<PooledParsedProtoStructValueInterface>(
              moved_message)};
    }
    case MemoryManagement::kReferenceCounting: {
      auto* block = static_cast<char*>(memory_manager.Allocate(
          ReffedStaticParsedProtoStructValueInterface::MessageOffset() + size,
          __STDCPP_DEFAULT_NEW_ALIGNMENT__));
      auto* message_address =
          block + ReffedStaticParsedProtoStructValueInterface::MessageOffset();
      auto* moved_message = (*move_construct)(message_address, message);
      ABSL_DCHECK_EQ(reinterpret_cast<uintptr_t>(message_address),
                     reinterpret_cast<uintptr_t>(moved_message));
      auto* message_value = ::new (static_cast<void*>(block))
          ReffedStaticParsedProtoStructValueInterface(size);
      common_internal::SetReferenceCountForThat(*message_value, message_value);
      return ParsedStructValue{common_internal::MakeShared(
          common_internal::kAdoptRef, message_value, message_value)};
    }
  }
}

absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueManager& value_manager, Shared<const void> aliased,
    absl::Nonnull<const google::protobuf::Message*> message) {
  {
    CEL_ASSIGN_OR_RETURN(auto well_known,
                         WellKnownProtoMessageToValue(value_manager, message));
    if (well_known) {
      return std::move(well_known).value();
    }
  }
  auto memory_manager = value_manager.GetMemoryManager();
  switch (memory_manager.memory_management()) {
    case MemoryManagement::kPooling: {
      if (!aliased) {
        // `message` is indirectly owned by something on an arena. The user is
        // responsible for ensuring they are the same arena or that `message`
        // outlives the resulting value.
        return ParsedStructValue{
            memory_manager.MakeShared<PooledParsedProtoStructValueInterface>(
                message)};
      }
      // `message` is indirectly owned by something reference counted. The
      // destructor of the implementation will decrement the reference count.
      return ParsedStructValue{
          memory_manager.MakeShared<AliasingParsedProtoStructValueInterface>(
              message, std::move(aliased))};
    }
    case MemoryManagement::kReferenceCounting: {
      if (!aliased) {
        // `message` is indirectly owned by something on an arena, and we want
        // to create a reference counted value. Unfortunately we have no way of
        // ensuring the arena outlives the resulting reference counted value. So
        // we need to perform a copy.
        auto* copied_message = message->New();
        copied_message->CopyFrom(*message);
        return ParsedStructValue{
            memory_manager
                .MakeShared<ReffedDynamicParsedProtoStructValueInterface>(
                    copied_message)};
      }
      return ParsedStructValue{
          memory_manager.MakeShared<AliasingParsedProtoStructValueInterface>(
              message, std::move(aliased))};
    }
  }
}

absl::StatusOr<absl::Nonnull<google::protobuf::Message*>> ProtoMessageFromValueImpl(
    const Value& value, absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory, google::protobuf::Arena* arena) {
  switch (value.kind()) {
    case ValueKind::kNull: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.Value", arena));
      CEL_RETURN_IF_ERROR(DynamicValueProtoFromJson(kJsonNull, *message));
      return message.release();
    }
    case ValueKind::kBool: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.BoolValue", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicBoolValueProto(
          Cast<BoolValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kInt: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.Int64Value", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicInt64ValueProto(
          Cast<IntValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kUint: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.UInt64Value", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicUInt64ValueProto(
          Cast<UintValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kDouble: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.DoubleValue", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicDoubleValueProto(
          Cast<DoubleValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kString: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.StringValue", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicStringValueProto(
          Cast<StringValue>(value).NativeCord(), *message));
      return message.release();
    }
    case ValueKind::kBytes: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.BytesValue", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicBytesValueProto(
          Cast<BytesValue>(value).NativeCord(), *message));
      return message.release();
    }
    case ValueKind::kStruct: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, value.GetTypeName(), arena));
      ProtoAnyToJsonConverter converter(pool, factory);
      absl::Cord serialized;
      CEL_RETURN_IF_ERROR(value.SerializeTo(converter, serialized));
      if (!message->ParsePartialFromCord(serialized)) {
        return absl::UnknownError(
            absl::StrCat("failed to parse `", message->GetTypeName(), "`"));
      }
      return message.release();
    }
    case ValueKind::kDuration: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.Duration", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicDurationProto(
          Cast<DurationValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kTimestamp: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.Timestamp", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicTimestampProto(
          Cast<TimestampValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kList: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.ListValue", arena));
      ProtoAnyToJsonConverter converter(pool, factory);
      CEL_ASSIGN_OR_RETURN(
          auto json, Cast<ListValue>(value).ConvertToJsonArray(converter));
      CEL_RETURN_IF_ERROR(DynamicListValueProtoFromJson(json, *message));
      return message.release();
    }
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.Struct", arena));
      ProtoAnyToJsonConverter converter(pool, factory);
      CEL_ASSIGN_OR_RETURN(
          auto json, Cast<MapValue>(value).ConvertToJsonObject(converter));
      CEL_RETURN_IF_ERROR(DynamicStructProtoFromJson(json, *message));
      return message.release();
    }
    default:
      break;
  }
  return TypeConversionError(value.GetTypeName(), "*message*").NativeValue();
}

absl::Status ProtoMessageFromValueImpl(
    const Value& value, absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory,
    absl::Nonnull<google::protobuf::Message*> message) {
  CEL_ASSIGN_OR_RETURN(const auto* to_desc, GetDescriptor(*message));
  switch (to_desc->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
      if (auto double_value = As<DoubleValue>(value); double_value) {
        return WrapDynamicFloatValueProto(
            static_cast<float>(double_value->NativeValue()), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
      if (auto double_value = As<DoubleValue>(value); double_value) {
        return WrapDynamicDoubleValueProto(
            static_cast<float>(double_value->NativeValue()), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE: {
      if (auto int_value = As<IntValue>(value); int_value) {
        if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
            int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
          return absl::OutOfRangeError("int64 to int32_t overflow");
        }
        return WrapDynamicInt32ValueProto(
            static_cast<int32_t>(int_value->NativeValue()), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE: {
      if (auto int_value = As<IntValue>(value); int_value) {
        return WrapDynamicInt64ValueProto(int_value->NativeValue(), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
      if (auto uint_value = As<UintValue>(value); uint_value) {
        if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
          return absl::OutOfRangeError("uint64 to uint32_t overflow");
        }
        return WrapDynamicUInt32ValueProto(
            static_cast<uint32_t>(uint_value->NativeValue()), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
      if (auto uint_value = As<UintValue>(value); uint_value) {
        return WrapDynamicUInt64ValueProto(uint_value->NativeValue(), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
      if (auto string_value = As<StringValue>(value); string_value) {
        return WrapDynamicStringValueProto(string_value->NativeCord(),
                                           *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
      if (auto bytes_value = As<BytesValue>(value); bytes_value) {
        return WrapDynamicBytesValueProto(bytes_value->NativeCord(), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
      if (auto bool_value = As<BoolValue>(value); bool_value) {
        return WrapDynamicBoolValueProto(bool_value->NativeValue(), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY: {
      ProtoAnyToJsonConverter converter(pool, factory);
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
      return WrapDynamicAnyProto(type_url, serialized, *message);
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION: {
      if (auto duration_value = As<DurationValue>(value); duration_value) {
        return WrapDynamicDurationProto(duration_value->NativeValue(),
                                        *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
      if (auto timestamp_value = As<TimestampValue>(value); timestamp_value) {
        return WrapDynamicTimestampProto(timestamp_value->NativeValue(),
                                         *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE: {
      ProtoAnyToJsonConverter converter(pool, factory);
      CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
      return DynamicValueProtoFromJson(json, *message);
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE: {
      ProtoAnyToJsonConverter converter(pool, factory);
      CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
      if (absl::holds_alternative<JsonArray>(json)) {
        return DynamicListValueProtoFromJson(absl::get<JsonArray>(json),
                                             *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT: {
      ProtoAnyToJsonConverter converter(pool, factory);
      CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
      if (absl::holds_alternative<JsonObject>(json)) {
        return DynamicStructProtoFromJson(absl::get<JsonObject>(json),
                                          *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    default:
      break;
  }

  // Not a well known type.

  // Deal with legacy values.
  if (auto legacy_value = As<common_internal::LegacyStructValue>(value);
      legacy_value) {
    if ((legacy_value->message_ptr() & base_internal::kMessageWrapperTagMask) ==
        base_internal::kMessageWrapperTagMessageValue) {
      // Full.
      const auto* from_message = reinterpret_cast<const google::protobuf::Message*>(
          legacy_value->message_ptr() & base_internal::kMessageWrapperPtrMask);
      return ProtoMessageCopy(message, to_desc, from_message);
    } else {
      // Lite.
      // Only thing we can do is check type names, which is gross because proto
      // returns `std::string`.
      const auto* from_message = reinterpret_cast<const google::protobuf::MessageLite*>(
          legacy_value->message_ptr() & base_internal::kMessageWrapperPtrMask);
      return ProtoMessageCopy(message, to_desc, from_message);
    }
  }

  // Deal with modern values.
  if (const auto* parsed_proto_struct_value = AsParsedProtoStructValue(value);
      parsed_proto_struct_value) {
    return ProtoMessageCopy(message, to_desc,
                            &parsed_proto_struct_value->message());
  }

  return TypeConversionError(value.GetTypeName(), message->GetTypeName())
      .NativeValue();
}

absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueFactory& value_factory, const TypeReflector& type_reflector,
    absl::Nonnull<const google::protobuf::Message*> prototype,
    const absl::Cord& serialized) {
  auto memory_manager = value_factory.GetMemoryManager();
  auto* arena = ProtoMemoryManagerArena(value_factory.GetMemoryManager());
  auto message = ArenaUniquePtr<google::protobuf::Message>(prototype->New(arena),
                                                 DefaultArenaDeleter{arena});
  if (!message->ParsePartialFromCord(serialized)) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to parse `", prototype->GetTypeName(), "`"));
  }
  {
    CEL_ASSIGN_OR_RETURN(auto well_known,
                         WellKnownProtoMessageToValue(
                             value_factory, type_reflector, message.get()));
    if (well_known) {
      return std::move(well_known).value();
    }
  }
  switch (memory_manager.memory_management()) {
    case MemoryManagement::kPooling:
      return ParsedStructValue{
          memory_manager.MakeShared<PooledParsedProtoStructValueInterface>(
              message.release())};
    case MemoryManagement::kReferenceCounting:
      return ParsedStructValue{
          memory_manager
              .MakeShared<ReffedDynamicParsedProtoStructValueInterface>(
                  message.release())};
  }
}

StructValue ProtoMessageAsStructValueImpl(
    ValueFactory& value_factory, absl::Nonnull<google::protobuf::Message*> message) {
  auto memory_manager = value_factory.GetMemoryManager();
  switch (memory_manager.memory_management()) {
    case MemoryManagement::kPooling:
      return ParsedStructValue{
          memory_manager.MakeShared<PooledParsedProtoStructValueInterface>(
              message)};
    case MemoryManagement::kReferenceCounting:
      return ParsedStructValue{
          memory_manager
              .MakeShared<ReffedDynamicParsedProtoStructValueInterface>(
                  message)};
  }
}

}  // namespace extensions::protobuf_internal

}  // namespace cel

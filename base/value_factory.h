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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUE_FACTORY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/attribute_set.h"
#include "base/function_result_set.h"
#include "base/handle.h"
#include "base/memory_manager.h"
#include "base/type_manager.h"
#include "base/value.h"
#include "base/values/bool_value.h"
#include "base/values/bytes_value.h"
#include "base/values/double_value.h"
#include "base/values/duration_value.h"
#include "base/values/enum_value.h"
#include "base/values/error_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "base/values/null_value.h"
#include "base/values/string_value.h"
#include "base/values/struct_value.h"
#include "base/values/timestamp_value.h"
#include "base/values/type_value.h"
#include "base/values/uint_value.h"
#include "base/values/unknown_value.h"
#include "internal/status_macros.h"

namespace cel {

namespace interop_internal {
absl::StatusOr<Handle<StringValue>> CreateStringValueFromView(
    cel::ValueFactory& value_factory, absl::string_view input);
absl::StatusOr<Handle<BytesValue>> CreateBytesValueFromView(
    cel::ValueFactory& value_factory, absl::string_view input);
}  // namespace interop_internal

class ValueFactory final {
 private:
  template <typename T, typename U, typename V>
  using EnableIfBaseOfT =
      std::enable_if_t<std::is_base_of_v<T, std::remove_const_t<U>>, V>;

 public:
  explicit ValueFactory(TypeManager& type_manager ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : type_manager_(type_manager) {}

  ValueFactory(const ValueFactory&) = delete;
  ValueFactory& operator=(const ValueFactory&) = delete;

  TypeFactory& type_factory() const { return type_manager().type_factory(); }

  TypeProvider& type_provider() const { return type_manager().type_provider(); }

  TypeManager& type_manager() const { return type_manager_; }

  Handle<NullValue> GetNullValue() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<ErrorValue> CreateErrorValue(absl::Status status)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<BoolValue> CreateBoolValue(bool value) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<BoolValue>::Make<BoolValue>(value);
  }

  Handle<IntValue> CreateIntValue(int64_t value) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<IntValue>::Make<IntValue>(value);
  }

  Handle<UintValue> CreateUintValue(uint64_t value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<UintValue>::Make<UintValue>(value);
  }

  Handle<DoubleValue> CreateDoubleValue(double value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<DoubleValue>::Make<DoubleValue>(value);
  }

  Handle<BytesValue> GetBytesValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetEmptyBytesValue();
  }

  absl::StatusOr<Handle<BytesValue>> CreateBytesValue(const char* value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateBytesValue(absl::string_view(value));
  }

  absl::StatusOr<Handle<BytesValue>> CreateBytesValue(absl::string_view value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateBytesValue(std::string(value));
  }

  absl::StatusOr<Handle<BytesValue>> CreateBytesValue(std::string value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Handle<BytesValue>> CreateBytesValue(absl::Cord value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  template <typename Releaser>
  absl::StatusOr<Handle<BytesValue>> CreateBytesValue(absl::string_view value,
                                                      Releaser&& releaser)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (value.empty()) {
      std::forward<Releaser>(releaser)();
      return GetEmptyBytesValue();
    }
    return CreateBytesValue(
        absl::MakeCordFromExternal(value, std::forward<Releaser>(releaser)));
  }

  Handle<StringValue> GetStringValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return GetEmptyStringValue();
  }

  absl::StatusOr<Handle<StringValue>> CreateStringValue(const char* value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateStringValue(absl::string_view(value));
  }

  absl::StatusOr<Handle<StringValue>> CreateStringValue(absl::string_view value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateStringValue(std::string(value));
  }

  absl::StatusOr<Handle<StringValue>> CreateStringValue(std::string value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // Create a string value from a caller validated utf-8 string.
  // This is appropriate for generating strings from other CEL strings that have
  // already been validated as utf-8.
  Handle<StringValue> CreateUncheckedStringValue(std::string value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Handle<StringValue>> CreateStringValue(absl::Cord value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  template <typename Releaser>
  absl::StatusOr<Handle<StringValue>> CreateStringValue(absl::string_view value,
                                                        Releaser&& releaser)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (value.empty()) {
      std::forward<Releaser>(releaser)();
      return GetEmptyStringValue();
    }
    return CreateStringValue(
        absl::MakeCordFromExternal(value, std::forward<Releaser>(releaser)));
  }

  absl::StatusOr<Handle<DurationValue>> CreateDurationValue(
      absl::Duration value) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<DurationValue> CreateUncheckedDurationValue(absl::Duration value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<DurationValue>::Make<DurationValue>(
        value);
  }

  absl::StatusOr<Handle<TimestampValue>> CreateTimestampValue(absl::Time value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<TimestampValue> CreateUncheckedTimestampValue(absl::Time value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<TimestampValue>::Make<TimestampValue>(
        value);
  }

  absl::StatusOr<Handle<EnumValue>> CreateEnumValue(
      const Handle<EnumType>& enum_type,
      int64_t number) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CEL_ASSIGN_OR_RETURN(auto constant,
                         enum_type->FindConstant(EnumType::ConstantId(number)));
    if (!constant.has_value()) {
      return absl::NotFoundError(absl::StrCat("no such enum number", number));
    }
    return base_internal::HandleFactory<EnumValue>::template Make<EnumValue>(
        enum_type, constant->number);
  }

  absl::StatusOr<Handle<EnumValue>> CreateEnumValue(
      const Handle<EnumType>& enum_type,
      absl::string_view name) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CEL_ASSIGN_OR_RETURN(auto constant,
                         enum_type->FindConstant(EnumType::ConstantId(name)));
    if (!constant.has_value()) {
      return absl::NotFoundError(absl::StrCat("no such enum value", name));
    }
    return base_internal::HandleFactory<EnumValue>::template Make<EnumValue>(
        enum_type, constant->number);
  }

  template <typename T>
  std::enable_if_t<std::is_enum_v<T>, absl::StatusOr<Handle<EnumValue>>>
  CreateEnumValue(const Handle<EnumType>& enum_type,
                  T value) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateEnumValue(enum_type, static_cast<int64_t>(value));
  }

  template <typename T, typename... Args>
  EnableIfBaseOfT<StructValue, T, absl::StatusOr<Handle<T>>> CreateStructValue(
      const Handle<StructType>& struct_type,
      Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<
        std::remove_const_t<T>>(memory_manager(), struct_type,
                                std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  EnableIfBaseOfT<ListValue, T, absl::StatusOr<Handle<T>>> CreateListValue(
      const Handle<ListType>& type,
      Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<
        std::remove_const_t<T>>(memory_manager(), type,
                                std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  EnableIfBaseOfT<MapValue, T, absl::StatusOr<Handle<T>>> CreateMapValue(
      const Handle<MapType>& type,
      Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<
        std::remove_const_t<T>>(memory_manager(), type,
                                std::forward<Args>(args)...);
  }

  Handle<TypeValue> CreateTypeValue(const Handle<Type>& value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<UnknownValue> CreateUnknownValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateUnknownValue(AttributeSet(), FunctionResultSet());
  }

  Handle<UnknownValue> CreateUnknownValue(AttributeSet attribute_set)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateUnknownValue(std::move(attribute_set), FunctionResultSet());
  }

  Handle<UnknownValue> CreateUnknownValue(FunctionResultSet function_result_set)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateUnknownValue(AttributeSet(), std::move(function_result_set));
  }

  Handle<UnknownValue> CreateUnknownValue(AttributeSet attribute_set,
                                          FunctionResultSet function_result_set)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  MemoryManager& memory_manager() const {
    return type_manager().memory_manager();
  }

 private:
  friend class BytesValue;
  friend class StringValue;
  friend absl::StatusOr<Handle<StringValue>>
  interop_internal::CreateStringValueFromView(cel::ValueFactory& value_factory,
                                              absl::string_view input);
  friend absl::StatusOr<Handle<BytesValue>>
  interop_internal::CreateBytesValueFromView(cel::ValueFactory& value_factory,
                                             absl::string_view input);

  Handle<BytesValue> GetEmptyBytesValue() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Handle<BytesValue>> CreateBytesValueFromView(
      absl::string_view value) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<StringValue> GetEmptyStringValue() ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Handle<StringValue>> CreateStringValueFromView(
      absl::string_view value) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  TypeManager& type_manager_;
};

// TypedEnumValueFactory creates EnumValue scoped to a specific EnumType. Used
// with EnumType::NewInstance.
class TypedEnumValueFactory final {
 private:
  template <typename T, typename U, typename V>
  using EnableIfBaseOfT =
      std::enable_if_t<std::is_base_of_v<T, std::remove_const_t<U>>, V>;

 public:
  TypedEnumValueFactory(
      ValueFactory& value_factory ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const Handle<EnumType>& enum_type ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : value_factory_(value_factory), enum_type_(enum_type) {}

  absl::StatusOr<Handle<EnumValue>> CreateEnumValue(int64_t number)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_factory_.CreateEnumValue(enum_type_, number);
  }

  template <typename T>
  std::enable_if_t<std::is_enum_v<T>, absl::StatusOr<Handle<EnumValue>>>
  CreateEnumValue(T value) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return CreateEnumValue(static_cast<int64_t>(value));
  }

 private:
  ValueFactory& value_factory_;
  const Handle<EnumType>& enum_type_;
};

// TypedStructValueFactory creates StructValue scoped to a specific StructType.
// Used with StructType::NewInstance.
class TypedStructValueFactory final {
 private:
  template <typename T, typename U, typename V>
  using EnableIfBaseOfT =
      std::enable_if_t<std::is_base_of_v<T, std::remove_const_t<U>>, V>;

 public:
  TypedStructValueFactory(
      ValueFactory& value_factory ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const Handle<StructType>& enum_type ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : value_factory_(value_factory), struct_type_(enum_type) {}

  template <typename T, typename... Args>
  EnableIfBaseOfT<StructValue, T, absl::StatusOr<Handle<T>>> CreateStructValue(
      Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_factory_.CreateStructValue<T>(struct_type_,
                                               std::forward<Args>(args)...);
  }

 private:
  ValueFactory& value_factory_;
  const Handle<StructType>& struct_type_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUE_FACTORY_H_

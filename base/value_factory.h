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
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/attribute_set.h"
#include "base/function_result_set.h"
#include "base/handle.h"
#include "base/memory.h"
#include "base/owner.h"
#include "base/type_manager.h"
#include "base/types/opaque_type.h"
#include "base/types/type_type.h"
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
#include "common/json.h"
#include "internal/status_macros.h"

namespace cel {

namespace base_internal {

template <typename T>
class BorrowedValue final : public T {
 public:
  template <typename... Args>
  explicit BorrowedValue(const cel::Value* owner, Args&&... args)
      : T(std::forward<Args>(args)...),
        owner_(ABSL_DIE_IF_NULL(owner))  // Crash OK
  {}

  ~BorrowedValue() override { ValueMetadata::Unref(*owner_); }

 private:
  const cel::Value* const owner_;
};

}  // namespace base_internal

class ValueFactory final {
 private:
  template <typename T, typename U>
  using BaseOf = std::is_base_of<T, std::remove_const_t<U>>;

  template <typename T, typename U, typename V>
  using EnableIfBaseOf =
      std::enable_if_t<std::is_base_of_v<T, std::remove_const_t<U>>, V>;

  template <typename R, typename V>
  using EnableIfReferent = std::enable_if_t<std::is_base_of_v<Value, R>, V>;

  template <typename T, typename U, typename R, typename V>
  using EnableIfBaseOfAndReferent = std::enable_if_t<
      std::conjunction_v<std::is_base_of<T, std::remove_const_t<U>>,
                         std::is_base_of<Value, std::remove_const_t<R>>>,
      V>;

 public:
  explicit ValueFactory(TypeManager& type_manager ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : type_manager_(type_manager) {}

  ValueFactory(const ValueFactory&) = delete;
  ValueFactory& operator=(const ValueFactory&) = delete;

  TypeFactory& type_factory() const { return type_manager().type_factory(); }

  const TypeProvider& type_provider() const {
    return type_manager().type_provider();
  }

  TypeManager& type_manager() const { return type_manager_; }

  Handle<NullValue> GetNullValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<NullValue>::Make<NullValue>();
  }

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

  template <typename R>
  EnableIfReferent<R, absl::StatusOr<Handle<BytesValue>>>
  CreateBorrowedBytesValue(Owner<R> owner, absl::string_view value) {
    if (value.empty()) {
      return GetEmptyBytesValue();
    }
    auto* pointer = owner.release();
    if (pointer == nullptr) {
      return base_internal::HandleFactory<BytesValue>::Make<
          base_internal::InlinedStringViewBytesValue>(value);
    }
    return CreateMemberBytesValue(value, pointer);
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
  Handle<StringValue> CreateUncheckedStringValue(absl::Cord value)
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

  template <typename R>
  EnableIfReferent<R, absl::StatusOr<Handle<StringValue>>>
  CreateBorrowedStringValue(Owner<R> owner, absl::string_view value) {
    if (value.empty()) {
      return GetEmptyStringValue();
    }
    auto* pointer = owner.release();
    if (pointer == nullptr) {
      return base_internal::HandleFactory<StringValue>::Make<
          base_internal::InlinedStringViewStringValue>(value);
    }
    return CreateMemberStringValue(value, pointer);
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
                         enum_type->FindConstantByNumber(number));
    if (ABSL_PREDICT_FALSE(!constant.has_value())) {
      return absl::NotFoundError(absl::StrCat("no such enum number", number));
    }
    return base_internal::HandleFactory<EnumValue>::template Make<EnumValue>(
        enum_type, constant->number);
  }

  absl::StatusOr<Handle<EnumValue>> CreateEnumValue(
      const Handle<EnumType>& enum_type,
      absl::string_view name) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    CEL_ASSIGN_OR_RETURN(auto constant, enum_type->FindConstantByName(name));
    if (ABSL_PREDICT_FALSE(!constant.has_value())) {
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
  EnableIfBaseOf<StructValue, T, absl::StatusOr<Handle<T>>> CreateStructValue(
      const Handle<StructType>& type,
      Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<
        std::remove_const_t<T>>(memory_manager(), type,
                                std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  EnableIfBaseOf<StructValue, T, absl::StatusOr<Handle<T>>> CreateStructValue(
      Handle<StructType>&& type, Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<
        std::remove_const_t<T>>(memory_manager(), std::move(type),
                                std::forward<Args>(args)...);
  }

  template <typename T, typename R, typename... Args>
  EnableIfBaseOfAndReferent<StructValue, T, R, absl::StatusOr<Handle<T>>>
  CreateBorrowedStructValue(Owner<R> owner, const Handle<StructType>& type,
                            Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto* pointer = owner.release();
    if (pointer == nullptr) {
      return CreateStructValue<T>(type, std::forward<Args>(args)...);
    }
    return base_internal::HandleFactory<T>::template Make<
        base_internal::BorrowedValue<T>>(memory_manager(), pointer, type,
                                         std::forward<Args>(args)...);
  }

  template <typename T, typename R, typename... Args>
  EnableIfBaseOfAndReferent<StructValue, T, R, absl::StatusOr<Handle<T>>>
  CreateBorrowedStructValue(Owner<R> owner, Handle<StructType>&& type,
                            Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto* pointer = owner.release();
    if (pointer == nullptr) {
      return CreateStructValue<T>(std::move(type), std::forward<Args>(args)...);
    }
    return base_internal::HandleFactory<T>::template Make<
        base_internal::BorrowedValue<T>>(memory_manager(), pointer,
                                         std::move(type),
                                         std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  EnableIfBaseOf<ListValue, T, absl::StatusOr<Handle<T>>> CreateListValue(
      const Handle<ListType>& type,
      Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<
        std::remove_const_t<T>>(memory_manager(), type,
                                std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  EnableIfBaseOf<ListValue, T, absl::StatusOr<Handle<T>>> CreateListValue(
      Handle<ListType>&& type, Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<
        std::remove_const_t<T>>(memory_manager(), std::move(type),
                                std::forward<Args>(args)...);
  }

  template <typename T, typename R, typename... Args>
  EnableIfBaseOfAndReferent<ListValue, T, R, absl::StatusOr<Handle<T>>>
  CreateBorrowedListValue(Owner<R> owner, const Handle<ListType>& type,
                          Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto* pointer = owner.release();
    if (pointer == nullptr) {
      return CreateListValue<T>(type, std::forward<Args>(args)...);
    }
    return base_internal::HandleFactory<T>::template Make<
        base_internal::BorrowedValue<T>>(memory_manager(), pointer, type,
                                         std::forward<Args>(args)...);
  }

  template <typename T, typename R, typename... Args>
  EnableIfBaseOfAndReferent<ListValue, T, R, absl::StatusOr<Handle<T>>>
  CreateBorrowedListValue(Owner<R> owner, Handle<ListType>&& type,
                          Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto* pointer = owner.release();
    if (pointer == nullptr) {
      return CreateListValue<T>(std::move(type), std::forward<Args>(args)...);
    }
    return base_internal::HandleFactory<T>::template Make<
        base_internal::BorrowedValue<T>>(memory_manager(), pointer,
                                         std::move(type),
                                         std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  EnableIfBaseOf<MapValue, T, absl::StatusOr<Handle<T>>> CreateMapValue(
      const Handle<MapType>& type,
      Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<
        std::remove_const_t<T>>(memory_manager(), type,
                                std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  EnableIfBaseOf<MapValue, T, absl::StatusOr<Handle<T>>> CreateMapValue(
      Handle<MapType>&& type, Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<T>::template Make<
        std::remove_const_t<T>>(memory_manager(), std::move(type),
                                std::forward<Args>(args)...);
  }

  template <typename T, typename R, typename... Args>
  EnableIfBaseOfAndReferent<MapValue, T, R, absl::StatusOr<Handle<T>>>
  CreateBorrowedMapValue(Owner<R> owner, const Handle<MapType>& type,
                         Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto* pointer = owner.release();
    if (pointer == nullptr) {
      return CreateMapValue<T>(type, std::forward<Args>(args)...);
    }
    return base_internal::HandleFactory<T>::template Make<
        base_internal::BorrowedValue<T>>(memory_manager(), pointer, type,
                                         std::forward<Args>(args)...);
  }

  template <typename T, typename R, typename... Args>
  EnableIfBaseOfAndReferent<MapValue, T, R, absl::StatusOr<Handle<T>>>
  CreateBorrowedMapValue(Owner<R> owner, Handle<MapType>&& type,
                         Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto* pointer = owner.release();
    if (pointer == nullptr) {
      return CreateMapValue<T>(std::move(type), std::forward<Args>(args)...);
    }
    return base_internal::HandleFactory<T>::template Make<
        base_internal::BorrowedValue<T>>(memory_manager(), pointer,
                                         std::move(type),
                                         std::forward<Args>(args)...);
  }

  Handle<TypeValue> CreateTypeValue(const Handle<Type>& value)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<TypeValue>::Make<
        base_internal::ModernTypeValue>(value);
  }

  template <typename ValueType, typename TypeType, typename... Args>
  std::enable_if_t<std::conjunction_v<BaseOf<OpaqueType, TypeType>,
                                      BaseOf<OpaqueValue, ValueType>>,
                   absl::StatusOr<Handle<ValueType>>>
  CreateOpaqueValue(const Handle<TypeType>& type,
                    Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<ValueType>::template Make<
        std::remove_const_t<ValueType>>(memory_manager(), type,
                                        std::forward<Args>(args)...);
  }

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

  Handle<Value> CreateValueFromJson(Json json) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<ListValue> CreateListValueFromJson(JsonArray array)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<MapValue> CreateMapValueFromJson(JsonObject object)
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  MemoryManager& memory_manager() const {
    return type_manager().memory_manager();
  }

 private:
  friend class BytesValue;
  friend class StringValue;

  Handle<BytesValue> GetEmptyBytesValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<BytesValue>::Make<
        base_internal::InlinedStringViewBytesValue>(absl::string_view());
  }

  absl::StatusOr<Handle<BytesValue>> CreateBytesValueFromView(
      absl::string_view value) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Handle<StringValue> GetEmptyStringValue() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<StringValue>::Make<
        base_internal::InlinedStringViewStringValue>(absl::string_view());
  }

  absl::StatusOr<Handle<StringValue>> CreateStringValueFromView(
      absl::string_view value) ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Handle<BytesValue>> CreateMemberBytesValue(
      absl::string_view value,
      const Value* owner) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<BytesValue>::template Make<
        base_internal::InlinedStringViewBytesValue>(value, owner);
  }

  absl::StatusOr<Handle<StringValue>> CreateMemberStringValue(
      absl::string_view value,
      const Value* owner) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base_internal::HandleFactory<StringValue>::template Make<
        base_internal::InlinedStringViewStringValue>(value, owner);
  }

  TypeManager& type_manager_;
};

// -----------------------------------------------------------------------------
// Implementation details

namespace base_internal {

inline Handle<BoolValue> ValueTraits<BoolValue>::Wrap(
    ValueFactory& value_factory, bool value) {
  return value_factory.CreateBoolValue(value);
}

inline Handle<IntValue> ValueTraits<IntValue>::Wrap(ValueFactory& value_factory,
                                                    int64_t value) {
  return value_factory.CreateIntValue(value);
}

inline Handle<UintValue> ValueTraits<UintValue>::Wrap(
    ValueFactory& value_factory, uint64_t value) {
  return value_factory.CreateUintValue(value);
}

inline Handle<DoubleValue> ValueTraits<DoubleValue>::Wrap(
    ValueFactory& value_factory, double value) {
  return value_factory.CreateDoubleValue(value);
}

inline Handle<DurationValue> ValueTraits<DurationValue>::Wrap(
    ValueFactory& value_factory, absl::Duration value) {
  return value_factory.CreateUncheckedDurationValue(value);
}

inline Handle<TimestampValue> ValueTraits<TimestampValue>::Wrap(
    ValueFactory& value_factory, absl::Time value) {
  return value_factory.CreateUncheckedTimestampValue(value);
}

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUE_FACTORY_H_

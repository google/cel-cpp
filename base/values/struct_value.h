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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_STRUCT_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/base/attributes.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "internal/rtti.h"

namespace cel {

namespace interop_internal {
struct LegacyStructValueAccess;
}

class ValueFactory;

// StructValue represents an instance of cel::StructType.
class StructValue : public Value {
 public:
  static constexpr Kind kKind = Kind::kStruct;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  using FieldId = StructType::FieldId;

  constexpr Kind kind() const { return kKind; }

  Handle<StructType> type() const;

  std::string DebugString() const;

  absl::StatusOr<Handle<Value>> GetField(ValueFactory& value_factory,
                                         FieldId field) const;

  absl::StatusOr<bool> HasField(TypeManager& type_manager, FieldId field) const;

  using Value::Is;

 protected:
  absl::StatusOr<Handle<Value>> GetFieldByName(ValueFactory& value_factory,
                                               absl::string_view name) const;

  absl::StatusOr<Handle<Value>> GetFieldByNumber(ValueFactory& value_factory,
                                                 int64_t number) const;

  absl::StatusOr<bool> HasFieldByName(TypeManager& type_manager,
                                      absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(TypeManager& type_manager,
                                        int64_t number) const;

 private:
  struct GetFieldVisitor;
  struct HasFieldVisitor;

  friend struct GetFieldVisitor;
  friend struct HasFieldVisitor;
  friend internal::TypeInfo base_internal::GetStructValueTypeId(
      const StructValue& struct_value);
  friend class base_internal::ValueHandle;
  friend class base_internal::LegacyStructValue;
  friend class base_internal::AbstractStructValue;

  StructValue() = default;

  // Called by CEL_IMPLEMENT_STRUCT_VALUE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const;
};

CEL_INTERNAL_VALUE_DECL(StructValue);

namespace base_internal {

// In an ideal world we would just make StructType a heap type. Unfortunately we
// have to deal with our legacy API and we do not want to unncessarily perform
// heap allocations during interop. So we have an inline variant and heap
// variant.

ABSL_ATTRIBUTE_WEAK void MessageValueHash(uintptr_t msg, uintptr_t type_info,
                                          absl::HashState state);
ABSL_ATTRIBUTE_WEAK bool MessageValueEquals(uintptr_t lhs_msg,
                                            uintptr_t lhs_type_info,
                                            const Value& rhs);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<bool> MessageValueHasFieldByNumber(
    uintptr_t msg, uintptr_t type_info, int64_t number);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<bool> MessageValueHasFieldByName(
    uintptr_t msg, uintptr_t type_info, absl::string_view name);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<Handle<Value>> MessageValueGetFieldByNumber(
    uintptr_t msg, uintptr_t type_info, ValueFactory& value_factory,
    int64_t number);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<Handle<Value>> MessageValueGetFieldByName(
    uintptr_t msg, uintptr_t type_info, ValueFactory& value_factory,
    absl::string_view name);

class LegacyStructValue final : public StructValue, public InlineData {
 public:
  static bool Is(const Value& value) {
    return value.kind() == kKind &&
           static_cast<const StructValue&>(value).TypeId() ==
               internal::TypeId<LegacyStructValue>();
  }

  Handle<StructType> type() const;

  std::string DebugString() const;

 protected:
  absl::StatusOr<Handle<Value>> GetFieldByName(ValueFactory& value_factory,
                                               absl::string_view name) const;

  absl::StatusOr<Handle<Value>> GetFieldByNumber(ValueFactory& value_factory,
                                                 int64_t number) const;

  absl::StatusOr<bool> HasFieldByName(TypeManager& type_manager,
                                      absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(TypeManager& type_manager,
                                        int64_t number) const;

  using StructValue::Is;

 private:
  struct GetFieldVisitor;
  struct HasFieldVisitor;

  friend struct GetFieldVisitor;
  friend struct HasFieldVisitor;
  friend internal::TypeInfo base_internal::GetStructValueTypeId(
      const StructValue& struct_value);
  friend class base_internal::ValueHandle;
  friend class cel::StructValue;
  template <size_t Size, size_t Align>
  friend class AnyData;
  friend struct interop_internal::LegacyStructValueAccess;

  static constexpr uintptr_t kMetadata =
      kStoredInline | kTrivial | (static_cast<uintptr_t>(kKind) << kKindShift);

  LegacyStructValue(uintptr_t msg, uintptr_t type_info)
      : StructValue(),
        InlineData(kMetadata),
        msg_(msg),
        type_info_(type_info) {}

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Handle.

  LegacyStructValue(const LegacyStructValue&) = delete;
  LegacyStructValue(LegacyStructValue&&) = delete;

  // Called by CEL_IMPLEMENT_STRUCT_VALUE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const {
    return internal::TypeId<LegacyStructValue>();
  }

  // This is a type erased pointer to google::protobuf::Message or google::protobuf::MessageLite, it
  // is tagged.
  uintptr_t msg_;
  // This is a type erased pointer to LegacyTypeInfoProvider.
  uintptr_t type_info_;
};

class AbstractStructValue : public StructValue, public HeapData {
 public:
  static bool Is(const Value& value) {
    return value.kind() == kKind &&
           static_cast<const StructValue&>(value).TypeId() !=
               internal::TypeId<LegacyStructValue>();
  }

  const Handle<StructType>& type() const { return type_; }

  virtual std::string DebugString() const = 0;

  using StructValue::Is;

 protected:
  explicit AbstractStructValue(Handle<StructType> type);

  virtual absl::StatusOr<Handle<Value>> GetFieldByName(
      ValueFactory& value_factory, absl::string_view name) const = 0;

  virtual absl::StatusOr<Handle<Value>> GetFieldByNumber(
      ValueFactory& value_factory, int64_t number) const = 0;

  virtual absl::StatusOr<bool> HasFieldByName(TypeManager& type_manager,
                                              absl::string_view name) const = 0;

  virtual absl::StatusOr<bool> HasFieldByNumber(TypeManager& type_manager,
                                                int64_t number) const = 0;

 private:
  struct GetFieldVisitor;
  struct HasFieldVisitor;

  friend struct GetFieldVisitor;
  friend struct HasFieldVisitor;
  friend internal::TypeInfo base_internal::GetStructValueTypeId(
      const StructValue& struct_value);
  friend class base_internal::ValueHandle;
  friend class cel::StructValue;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Handle.

  AbstractStructValue(const AbstractStructValue&) = delete;
  AbstractStructValue(AbstractStructValue&&) = delete;

  // Called by CEL_IMPLEMENT_STRUCT_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  const Handle<StructType> type_;
};

}  // namespace base_internal

#define CEL_STRUCT_VALUE_CLASS ::cel::base_internal::AbstractStructValue

// CEL_DECLARE_STRUCT_VALUE declares `struct_value` as an struct value. It must
// be part of the class definition of `struct_value`.
//
// class MyStructValue : public CEL_STRUCT_VALUE_CLASS {
//  ...
// private:
//   CEL_DECLARE_STRUCT_VALUE(MyStructValue);
// };
#define CEL_DECLARE_STRUCT_VALUE(struct_value) \
  CEL_INTERNAL_DECLARE_VALUE(Struct, struct_value)

// CEL_IMPLEMENT_STRUCT_VALUE implements `struct_value` as an struct
// value. It must be called after the class definition of `struct_value`.
//
// class MyStructValue : public CEL_STRUCT_VALUE_CLASS {
//  ...
// private:
//   CEL_DECLARE_STRUCT_VALUE(MyStructValue);
// };
//
// CEL_IMPLEMENT_STRUCT_VALUE(MyStructValue);
#define CEL_IMPLEMENT_STRUCT_VALUE(struct_value) \
  CEL_INTERNAL_IMPLEMENT_VALUE(Struct, struct_value)

namespace base_internal {

inline internal::TypeInfo GetStructValueTypeId(
    const StructValue& struct_value) {
  return struct_value.TypeId();
}

}  // namespace base_internal

namespace base_internal {

template <>
struct ValueTraits<StructValue> {
  using type = StructValue;

  using type_type = StructType;

  using underlying_type = void;

  static std::string DebugString(const type& value) {
    return value.DebugString();
  }

  static Handle<type> Wrap(ValueFactory& value_factory, Handle<type> value) {
    static_cast<void>(value_factory);
    return value;
  }

  static Handle<type> Unwrap(Handle<type> value) { return value; }
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_STRUCT_VALUE_H_

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
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/owner.h"
#include "base/type.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "common/any.h"
#include "common/json.h"
#include "common/native_type.h"

namespace google::api::expr::runtime {
class SelectStep;
}

namespace cel {

namespace interop_internal {
struct LegacyStructValueAccess;
}

class ValueFactory;
class StructValueBuilder;
class StructValueBuilderInterface;

struct FieldSpecifier {
  int64_t number;
  std::string name;
};

using SelectQualifier = absl::variant<FieldSpecifier, AttributeQualifier>;

// StructValue represents an instance of cel::StructType.
class StructValue : public Value,
                    public base_internal::EnableHandleFromThis<StructValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kStruct;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  using Value::Is;

  static const StructValue& Cast(const Value& value) {
    ABSL_DCHECK(Is(value)) << "cannot cast " << value.type()->name()
                           << " to struct";
    return static_cast<const StructValue&>(value);
  }

  using FieldId = StructType::FieldId;

  constexpr ValueKind kind() const { return kKind; }

  Handle<StructType> type() const;

  size_t field_count() const;

  std::string DebugString() const;

  absl::StatusOr<Handle<Value>> Equals(ValueFactory& value_factory,
                                       const Value& other) const;

  absl::StatusOr<Any> ConvertToAny(ValueFactory& value_factory) const;

  absl::StatusOr<Json> ConvertToJson(ValueFactory& value_factory) const;

  absl::StatusOr<Handle<Value>> ConvertToType(ValueFactory& value_factory,
                                              const Handle<Type>& type) const;

  absl::StatusOr<Handle<Value>> GetField(ValueFactory& value_factory,
                                         FieldId field) const;

  absl::StatusOr<Handle<Value>> GetFieldByName(ValueFactory& value_factory,
                                               absl::string_view name) const;

  absl::StatusOr<Handle<Value>> GetFieldByNumber(ValueFactory& value_factory,
                                                 int64_t number) const;

  // Apply a series of qualifications (representing field traversals) to the
  // given struct.
  //
  // absl::StatusCode::kUnimplemented has special meaning: if returned the
  // evaluator will attempt to apply the operation using the standard Get/Has
  // operations.
  absl::StatusOr<Handle<Value>> Qualify(
      ValueFactory& value_factory,
      absl::Span<const SelectQualifier> select_qualifiers,
      bool presence_test) const;

  absl::StatusOr<bool> HasField(TypeManager& type_manager, FieldId field) const;

  absl::StatusOr<bool> HasFieldByName(TypeManager& type_manager,
                                      absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(TypeManager& type_manager,
                                        int64_t number) const;

  class FieldIterator;

  absl::StatusOr<absl::Nonnull<std::unique_ptr<FieldIterator>>>
  NewFieldIterator(ValueFactory& value_factory) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  struct Field final {
    Field(FieldId id, Handle<Value> value) : id(id), value(std::move(value)) {}

    FieldId id;
    Handle<Value> value;
  };

 protected:
  static FieldId MakeFieldId(absl::string_view name) {
    return StructType::MakeFieldId(name);
  }

  static FieldId MakeFieldId(int64_t number) {
    return StructType::MakeFieldId(number);
  }

  template <typename E>
  static std::enable_if_t<
      std::conjunction_v<
          std::is_enum<E>,
          std::is_convertible<std::underlying_type_t<E>, int64_t>>,
      FieldId>
  MakeFieldId(E e) {
    return StructType::MakeFieldId(e);
  }

 private:
  struct GetFieldVisitor;
  struct HasFieldVisitor;

  friend struct GetFieldVisitor;
  friend struct HasFieldVisitor;
  friend NativeTypeId base_internal::GetStructValueTypeId(
      const StructValue& struct_value);
  friend class base_internal::ValueHandle;
  friend class base_internal::LegacyStructValue;
  friend class base_internal::AbstractStructValue;
  friend class google::api::expr::runtime::SelectStep;

  StructValue() = default;

  absl::StatusOr<Handle<Value>> GetWrappedFieldByName(
      ValueFactory& value_factory, absl::string_view name) const;

  // Called by CEL_IMPLEMENT_STRUCT_VALUE() and Is() to perform type checking.
  NativeTypeId TypeId() const;
};

class StructValue::FieldIterator {
 public:
  using Field = StructValue::Field;
  using FieldId = StructValue::FieldId;

  virtual ~FieldIterator() = default;

  ABSL_MUST_USE_RESULT virtual bool HasNext() = 0;

  virtual absl::StatusOr<Field> Next() = 0;

  virtual absl::StatusOr<FieldId> NextId();

  virtual absl::StatusOr<Handle<Value>> NextValue();
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
ABSL_ATTRIBUTE_WEAK size_t MessageValueFieldCount(uintptr_t msg,
                                                  uintptr_t type_info);
ABSL_ATTRIBUTE_WEAK std::vector<absl::string_view> MessageValueListFields(
    uintptr_t msg, uintptr_t type_info);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<bool> MessageValueHasFieldByNumber(
    uintptr_t msg, uintptr_t type_info, int64_t number);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<bool> MessageValueHasFieldByName(
    uintptr_t msg, uintptr_t type_info, absl::string_view name);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<Handle<Value>> MessageValueGetFieldByNumber(
    uintptr_t msg, uintptr_t type_info, ValueFactory& value_factory,
    int64_t number, bool unbox_null_wrapper_types);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<Handle<Value>> MessageValueQualify(
    uintptr_t msg, uintptr_t type_info, ValueFactory& value_factory,
    absl::Span<const SelectQualifier> qualifiers, bool presence_test);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<Handle<Value>> MessageValueGetFieldByName(
    uintptr_t msg, uintptr_t type_info, ValueFactory& value_factory,
    absl::string_view name, bool unbox_null_wrapper_types);

class LegacyStructValue final : public StructValue, public InlineData {
 public:
  static bool Is(const Value& value) {
    return value.kind() == kKind &&
           static_cast<const StructValue&>(value).TypeId() ==
               NativeTypeId::For<LegacyStructValue>();
  }

  using StructValue::Is;

  static const LegacyStructValue& Cast(const Value& value) {
    ABSL_ASSERT(Is(value));
    return static_cast<const LegacyStructValue&>(value);
  }

  Handle<StructType> type() const;

  std::string DebugString() const;

  absl::StatusOr<Any> ConvertToAny(ValueFactory& value_factory) const;

  absl::StatusOr<Json> ConvertToJson(ValueFactory& value_factory) const;

  size_t field_count() const;

  absl::StatusOr<Handle<Value>> GetFieldByName(ValueFactory& value_factory,
                                               absl::string_view name) const;

  absl::StatusOr<Handle<Value>> GetFieldByNumber(ValueFactory& value_factory,
                                                 int64_t number) const;

  absl::StatusOr<Handle<Value>> Qualify(
      ValueFactory& value_factory,
      absl::Span<const SelectQualifier> select_qualifiers,
      bool presence_test) const;

  absl::StatusOr<bool> HasFieldByName(TypeManager& type_manager,
                                      absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(TypeManager& type_manager,
                                        int64_t number) const;

  absl::StatusOr<absl::Nonnull<std::unique_ptr<FieldIterator>>>
  NewFieldIterator(ValueFactory& value_factory) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<Handle<Value>> Equals(ValueFactory& value_factory,
                                       const Value& other) const;

 private:
  struct GetFieldVisitor;
  struct HasFieldVisitor;

  friend struct GetFieldVisitor;
  friend struct HasFieldVisitor;
  friend NativeTypeId base_internal::GetStructValueTypeId(
      const StructValue& struct_value);
  friend class base_internal::ValueHandle;
  friend class cel::StructValue;
  template <size_t Size, size_t Align>
  friend struct AnyData;
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

  absl::StatusOr<Handle<Value>> GetWrappedFieldByName(
      ValueFactory& value_factory, absl::string_view name) const;

  // Called by CEL_IMPLEMENT_STRUCT_VALUE() and Is() to perform type checking.
  NativeTypeId TypeId() const { return NativeTypeId::For<LegacyStructValue>(); }

  // This is a type erased pointer to google::protobuf::Message or google::protobuf::MessageLite, it
  // is tagged.
  uintptr_t msg_;
  // This is a type erased pointer to LegacyTypeInfoProvider.
  uintptr_t type_info_;
};

class AbstractStructValue : public StructValue,
                            public HeapData,
                            public EnableOwnerFromThis<AbstractStructValue> {
 public:
  static bool Is(const Value& value) {
    return value.kind() == kKind &&
           static_cast<const StructValue&>(value).TypeId() !=
               NativeTypeId::For<LegacyStructValue>();
  }

  using StructValue::Is;

  static const AbstractStructValue& Cast(const Value& value) {
    ABSL_ASSERT(Is(value));
    return static_cast<const AbstractStructValue&>(value);
  }

  const Handle<StructType>& type() const { return type_; }

  virtual size_t field_count() const = 0;

  virtual std::string DebugString() const = 0;

  virtual absl::StatusOr<Any> ConvertToAny(ValueFactory& value_factory) const;

  virtual absl::StatusOr<Json> ConvertToJson(ValueFactory& value_factory) const;

  virtual absl::StatusOr<Handle<Value>> GetFieldByName(
      ValueFactory& value_factory, absl::string_view name) const = 0;

  virtual absl::StatusOr<Handle<Value>> GetFieldByNumber(
      ValueFactory& value_factory, int64_t number) const = 0;

  virtual absl::StatusOr<Handle<Value>> Qualify(
      ValueFactory& value_factory,
      absl::Span<const SelectQualifier> select_qualifiers,
      bool presence_test) const {
    return absl::UnimplementedError("Qualify not supported.");
  }

  virtual absl::StatusOr<bool> HasFieldByName(TypeManager& type_manager,
                                              absl::string_view name) const = 0;

  virtual absl::StatusOr<bool> HasFieldByNumber(TypeManager& type_manager,
                                                int64_t number) const = 0;

  virtual absl::StatusOr<absl::Nonnull<std::unique_ptr<FieldIterator>>>
  NewFieldIterator(ValueFactory& value_factory) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  virtual absl::StatusOr<Handle<Value>> Equals(ValueFactory& value_factory,
                                               const Value& other) const;

 protected:
  explicit AbstractStructValue(Handle<StructType> type);

 private:
  struct GetFieldVisitor;
  struct HasFieldVisitor;

  friend struct GetFieldVisitor;
  friend struct HasFieldVisitor;
  friend NativeTypeId base_internal::GetStructValueTypeId(
      const StructValue& struct_value);
  friend class base_internal::ValueHandle;
  friend class cel::StructValue;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Handle.

  AbstractStructValue(const AbstractStructValue&) = delete;
  AbstractStructValue(AbstractStructValue&&) = delete;

  absl::StatusOr<Handle<Value>> GetWrappedFieldByName(
      ValueFactory& value_factory, absl::string_view name) const;

  // Called by CEL_IMPLEMENT_STRUCT_VALUE() and Is() to perform type checking.
  virtual NativeTypeId TypeId() const = 0;

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

inline NativeTypeId GetStructValueTypeId(const StructValue& struct_value) {
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

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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPES_STRUCT_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPES_STRUCT_TYPE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/type.h"
#include "internal/rtti.h"

namespace cel {

namespace interop_internal {
struct LegacyStructTypeAccess;
}

class MemoryManager;
class StructValue;
class TypedStructValueFactory;
class TypeManager;
class StructValueBuilderInterface;
class ValueFactory;

// StructType represents an struct type. An struct is a set of fields
// that can be looked up by name and/or number.
class StructType : public Type {
 public:
  struct Field;

  class FieldId final {
   public:
    FieldId() = delete;

    FieldId(const FieldId&) = default;
    FieldId(FieldId&&) = default;
    FieldId& operator=(const FieldId&) = default;
    FieldId& operator=(FieldId&&) = default;

    std::string DebugString() const;

    friend bool operator==(const FieldId& lhs, const FieldId& rhs) {
      return lhs.data_ == rhs.data_;
    }

    friend bool operator<(const FieldId& lhs, const FieldId& rhs);

    template <typename H>
    friend H AbslHashValue(H state, const FieldId& id) {
      return H::combine(std::move(state), id.data_);
    }

    template <typename S>
    friend void AbslStringify(S& sink, const FieldId& id) {
      sink.Append(id.DebugString());
    }

   private:
    friend class StructType;
    friend class StructValue;
    friend struct base_internal::FieldIdFactory;

    explicit FieldId(absl::string_view name)
        : data_(absl::in_place_type<absl::string_view>, name) {}

    explicit FieldId(int64_t number)
        : data_(absl::in_place_type<int64_t>, number) {}

    absl::variant<absl::string_view, int64_t> data_;
  };

  static constexpr Kind kKind = Kind::kStruct;

  static bool Is(const Type& type) { return type.kind() == kKind; }

  using Type::Is;

  static const StructType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to struct";
    return static_cast<const StructType&>(type);
  }

  Kind kind() const { return kKind; }

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  std::string DebugString() const;

  size_t field_count() const;

  // Find the field definition for the given identifier. If the field does
  // not exist, an OK status and empty optional is returned. If the field
  // exists, an OK status and the field is returned. Otherwise an error is
  // returned.
  absl::StatusOr<absl::optional<Field>> FindField(TypeManager& type_manager,
                                                  FieldId id) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // Called by FindField.
  absl::StatusOr<absl::optional<Field>> FindFieldByName(
      TypeManager& type_manager,
      absl::string_view name) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // Called by FindField.
  absl::StatusOr<absl::optional<Field>> FindFieldByNumber(
      TypeManager& type_manager,
      int64_t number) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  class FieldIterator;

  absl::StatusOr<UniqueRef<FieldIterator>> NewFieldIterator(
      MemoryManager& memory_manager) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<UniqueRef<StructValueBuilderInterface>> NewValueBuilder(
      ValueFactory& value_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 protected:
  static FieldId MakeFieldId(absl::string_view name) { return FieldId(name); }

  static FieldId MakeFieldId(int64_t number) { return FieldId(number); }

  template <typename E>
  static std::enable_if_t<
      std::conjunction_v<
          std::is_enum<E>,
          std::is_convertible<std::underlying_type_t<E>, int64_t>>,
      FieldId>
  MakeFieldId(E e) {
    return MakeFieldId(
        static_cast<int64_t>(static_cast<std::underlying_type_t<E>>(e)));
  }

 private:
  friend internal::TypeInfo base_internal::GetStructTypeTypeId(
      const StructType& struct_type);
  struct FindFieldVisitor;

  friend struct FindFieldVisitor;
  friend class MemoryManager;
  friend class TypeFactory;
  friend class base_internal::TypeHandle;
  friend class StructValue;
  friend class base_internal::LegacyStructType;
  friend class base_internal::AbstractStructType;

  StructType() = default;

  // Called by CEL_IMPLEMENT_STRUCT_TYPE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const;
};

// Field describes a single field in a struct. All fields are valid so long as
// StructType is valid, except Field::type which is managed.
struct StructType::Field final {
  explicit Field(FieldId id, absl::string_view name, int64_t number,
                 Handle<Type> type, const void* hint = nullptr)
      : id(id), name(name), number(number), type(std::move(type)), hint(hint) {}

  // Identifier which allows the most efficient form of lookup, compared to
  // looking up by name or number.
  FieldId id;
  // The field name.
  absl::string_view name;
  // The field number.
  int64_t number;
  // The field type;
  Handle<Type> type;
  // Some implementation-specific data that can be laundered to the value
  // implementation for this type to enable potential optimizations.
  const void* hint = nullptr;
};

class StructType::FieldIterator {
 public:
  using FieldId = StructType::FieldId;
  using Field = StructType::Field;

  virtual ~FieldIterator() = default;

  ABSL_MUST_USE_RESULT virtual bool HasNext() = 0;

  virtual absl::StatusOr<Field> Next(TypeManager& type_manager) = 0;

  virtual absl::StatusOr<FieldId> NextId(TypeManager& type_manager);

  virtual absl::StatusOr<absl::string_view> NextName(TypeManager& type_manager);

  virtual absl::StatusOr<int64_t> NextNumber(TypeManager& type_manager);

  virtual absl::StatusOr<Handle<Type>> NextType(TypeManager& type_manager);
};

namespace base_internal {

// In an ideal world we would just make StructType a heap type. Unfortunately we
// have to deal with our legacy API and we do not want to unncessarily perform
// heap allocations during interop. So we have an inline variant and heap
// variant.

ABSL_ATTRIBUTE_WEAK absl::string_view MessageTypeName(uintptr_t msg);
ABSL_ATTRIBUTE_WEAK size_t MessageTypeFieldCount(uintptr_t msg);

class LegacyStructValueFieldIterator;

class LegacyStructType final : public StructType, public InlineData {
 public:
  static bool Is(const Type& type) {
    return StructType::Is(type) &&
           static_cast<const StructType&>(type).TypeId() ==
               internal::TypeId<LegacyStructType>();
  }

  using StructType::Is;

  static const LegacyStructType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to struct";
    return static_cast<const LegacyStructType&>(type);
  }

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // Always returns the same string.
  std::string DebugString() const { return std::string(name()); }

  size_t field_count() const;

  // Always returns an error.
  absl::StatusOr<absl::optional<Field>> FindFieldByName(
      TypeManager& type_manager,
      absl::string_view name) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // Always returns an error.
  absl::StatusOr<absl::optional<Field>> FindFieldByNumber(
      TypeManager& type_manager,
      int64_t number) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<UniqueRef<FieldIterator>> NewFieldIterator(
      MemoryManager& memory_manager) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<UniqueRef<StructValueBuilderInterface>> NewValueBuilder(
      ValueFactory& value_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 private:
  static constexpr uintptr_t kMetadata =
      kStoredInline | kTrivial | (static_cast<uintptr_t>(kKind) << kKindShift);

  friend class LegacyStructValueFieldIterator;
  friend struct interop_internal::LegacyStructTypeAccess;
  friend class cel::StructType;
  friend class LegacyStructValue;
  template <size_t Size, size_t Align>
  friend struct AnyData;

  explicit LegacyStructType(uintptr_t msg)
      : StructType(), InlineData(kMetadata), msg_(msg) {}

  internal::TypeInfo TypeId() const {
    return internal::TypeId<LegacyStructType>();
  }

  // This is a type erased pointer to google::protobuf::Message or LegacyTypeInfoApis. It
  // is tagged when it is google::protobuf::Message.
  uintptr_t msg_;
};

class AbstractStructType
    : public StructType,
      public HeapData,
      public EnableHandleFromThis<StructType, AbstractStructType> {
 public:
  static bool Is(const Type& type) {
    return StructType::Is(type) &&
           static_cast<const StructType&>(type).TypeId() !=
               internal::TypeId<LegacyStructType>();
  }

  using StructType::Is;

  static const AbstractStructType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to struct";
    return static_cast<const AbstractStructType&>(type);
  }

  virtual absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  virtual std::string DebugString() const { return std::string(name()); }

  virtual size_t field_count() const = 0;

  // Called by FindField.
  virtual absl::StatusOr<absl::optional<Field>> FindFieldByName(
      TypeManager& type_manager,
      absl::string_view name) const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  // Called by FindField.
  virtual absl::StatusOr<absl::optional<Field>> FindFieldByNumber(
      TypeManager& type_manager,
      int64_t number) const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  virtual absl::StatusOr<UniqueRef<FieldIterator>> NewFieldIterator(
      MemoryManager& memory_manager) const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  virtual absl::StatusOr<UniqueRef<StructValueBuilderInterface>>
  NewValueBuilder(ValueFactory& value_factory ABSL_ATTRIBUTE_LIFETIME_BOUND)
      const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 protected:
  AbstractStructType();

 private:
  friend internal::TypeInfo GetStructTypeTypeId(const StructType& struct_type);
  struct FindFieldVisitor;

  friend struct FindFieldVisitor;
  friend class MemoryManager;
  friend class TypeFactory;
  friend class TypeHandle;
  friend class StructValue;
  friend class cel::StructType;

  AbstractStructType(const AbstractStructType&) = delete;
  AbstractStructType(AbstractStructType&&) = delete;

  // Called by CEL_IMPLEMENT_STRUCT_TYPE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;
};

}  // namespace base_internal

#define CEL_STRUCT_TYPE_CLASS ::cel::base_internal::AbstractStructType

// CEL_DECLARE_STRUCT_TYPE declares `struct_type` as an struct type. It must be
// part of the class definition of `struct_type`.
//
// class MyStructType : public CEL_STRUCT_TYPE_CLASS {
//  ...
// private:
//   CEL_DECLARE_STRUCT_TYPE(MyStructType);
// };
#define CEL_DECLARE_STRUCT_TYPE(struct_type) \
  CEL_INTERNAL_DECLARE_TYPE(Struct, struct_type)

// CEL_IMPLEMENT_ENUM_TYPE implements `struct_type` as an struct type. It
// must be called after the class definition of `struct_type`.
//
// class MyStructType : public CEL_STRUCT_TYPE_CLASS {
//  ...
// private:
//   CEL_DECLARE_STRUCT_TYPE(MyStructType);
// };
//
// CEL_IMPLEMENT_STRUCT_TYPE(MyStructType);
#define CEL_IMPLEMENT_STRUCT_TYPE(struct_type) \
  CEL_INTERNAL_IMPLEMENT_TYPE(Struct, struct_type)

CEL_INTERNAL_TYPE_DECL(StructType);

namespace base_internal {

// This should be used for testing only, and is private.
struct FieldIdFactory {
  static StructType::FieldId Make(absl::string_view name) {
    return StructType::FieldId(name);
  }

  static StructType::FieldId Make(int64_t number) {
    return StructType::FieldId(number);
  }
};

inline internal::TypeInfo GetStructTypeTypeId(const StructType& struct_type) {
  return struct_type.TypeId();
}

}  // namespace base_internal

namespace base_internal {

template <>
struct TypeTraits<StructType> {
  using value_type = StructValue;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_STRUCT_TYPE_H_

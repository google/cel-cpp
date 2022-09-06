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

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "internal/rtti.h"

namespace cel {

class MemoryManager;
class StructValue;
class TypedStructValueFactory;
class TypeManager;

// StructType represents an struct type. An struct is a set of fields
// that can be looked up by name and/or number.
class StructType : public Type {
 public:
  struct Field;

  class FieldId final {
   public:
    explicit FieldId(absl::string_view name)
        : data_(absl::in_place_type<absl::string_view>, name) {}

    explicit FieldId(int64_t number)
        : data_(absl::in_place_type<int64_t>, number) {}

    FieldId() = delete;

    FieldId(const FieldId&) = default;
    FieldId& operator=(const FieldId&) = default;

   private:
    friend class StructType;
    friend class StructValue;

    absl::variant<absl::string_view, int64_t> data_;
  };

  static constexpr Kind kKind = Kind::kStruct;

  static bool Is(const Type& type) { return type.kind() == kKind; }

  Kind kind() const { return kKind; }

  absl::string_view name() const;

  std::string DebugString() const;

  void HashValue(absl::HashState state) const;

  bool Equals(const Type& other) const;

  // Find the field definition for the given identifier.
  absl::StatusOr<Field> FindField(TypeManager& type_manager, FieldId id) const;

 protected:
  absl::StatusOr<Persistent<StructValue>> NewInstance(
      TypedStructValueFactory& factory) const;

  // Called by FindField.
  absl::StatusOr<Field> FindFieldByName(TypeManager& type_manager,
                                        absl::string_view name) const;

  // Called by FindField.
  absl::StatusOr<Field> FindFieldByNumber(TypeManager& type_manager,
                                          int64_t number) const;

 private:
  friend internal::TypeInfo base_internal::GetStructTypeTypeId(
      const StructType& struct_type);
  struct FindFieldVisitor;

  friend struct FindFieldVisitor;
  friend class MemoryManager;
  friend class TypeFactory;
  friend class base_internal::PersistentTypeHandle;
  friend class StructValue;
  friend class base_internal::LegacyStructType;
  friend class base_internal::AbstractStructType;

  StructType() = default;

  // Called by CEL_IMPLEMENT_STRUCT_TYPE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const;
};

namespace base_internal {

// In an ideal world we would just make StructType a heap type. Unfortunately we
// have to deal with our legacy API and we do not want to unncessarily perform
// heap allocations during interop. So we have an inline variant and heap
// variant.

ABSL_ATTRIBUTE_WEAK absl::string_view MessageTypeName(uintptr_t msg);
ABSL_ATTRIBUTE_WEAK bool MessageTypeHash(uintptr_t msg, absl::HashState state);
ABSL_ATTRIBUTE_WEAK bool MessageTypeEquals(uintptr_t lhs, const Type& rhs);

class LegacyStructType final : public StructType,
                               public base_internal::InlineData {
 public:
  static bool Is(const Type& type) {
    return type.kind() == kKind &&
           static_cast<const StructType&>(type).TypeId() ==
               internal::TypeId<LegacyStructType>();
  }

  absl::string_view name() const;

  // Always returns the same string.
  std::string DebugString() const { return std::string(name()); }

  void HashValue(absl::HashState state) const;

  bool Equals(const Type& other) const;

  // Always returns an error.
  absl::StatusOr<Field> FindField(TypeManager& type_manager, FieldId id) const;

 protected:
  // Always returns an error.
  absl::StatusOr<Persistent<StructValue>> NewInstance(
      TypedStructValueFactory& factory) const;

  // Always returns an error.
  absl::StatusOr<Field> FindFieldByName(TypeManager& type_manager,
                                        absl::string_view name) const;

  // Always returns an error.
  absl::StatusOr<Field> FindFieldByNumber(TypeManager& type_manager,
                                          int64_t number) const;

 private:
  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline | base_internal::kTriviallyCopyable |
      base_internal::kTriviallyDestructible |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift);

  friend class cel::StructType;
  friend class base_internal::LegacyStructValue;
  template <size_t Size, size_t Align>
  friend class AnyData;

  explicit LegacyStructType(uintptr_t msg)
      : StructType(), base_internal::InlineData(kMetadata), msg_(msg) {}

  internal::TypeInfo TypeId() const {
    return internal::TypeId<LegacyStructType>();
  }

  // This is a type erased pointer to google::protobuf::Message or google::protobuf::MessageLite. It
  // is not tagged.
  uintptr_t msg_;
};

class AbstractStructType : public StructType, public base_internal::HeapData {
 public:
  static bool Is(const Type& type) {
    return type.kind() == kKind &&
           static_cast<const StructType&>(type).TypeId() !=
               internal::TypeId<LegacyStructType>();
  }

  virtual absl::string_view name() const = 0;

  virtual std::string DebugString() const { return std::string(name()); }

  virtual void HashValue(absl::HashState state) const;

  virtual bool Equals(const Type& other) const;

 protected:
  AbstractStructType();

  virtual absl::StatusOr<Persistent<StructValue>> NewInstance(
      TypedStructValueFactory& factory) const = 0;

  // Called by FindField.
  virtual absl::StatusOr<Field> FindFieldByName(
      TypeManager& type_manager, absl::string_view name) const = 0;

  // Called by FindField.
  virtual absl::StatusOr<Field> FindFieldByNumber(TypeManager& type_manager,
                                                  int64_t number) const = 0;

 private:
  friend internal::TypeInfo base_internal::GetStructTypeTypeId(
      const StructType& struct_type);
  struct FindFieldVisitor;

  friend struct FindFieldVisitor;
  friend class MemoryManager;
  friend class TypeFactory;
  friend class base_internal::PersistentTypeHandle;
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

struct StructType::Field final {
  explicit Field(absl::string_view name, int64_t number,
                 Persistent<const Type> type)
      : name(name), number(number), type(std::move(type)) {}

  // The field name.
  absl::string_view name;
  // The field number.
  int64_t number;
  // The field type;
  Persistent<const Type> type;
};

CEL_INTERNAL_TYPE_DECL(StructType);

namespace base_internal {

inline internal::TypeInfo GetStructTypeTypeId(const StructType& struct_type) {
  return struct_type.TypeId();
}

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_STRUCT_TYPE_H_

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

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
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
class StructType : public Type, public base_internal::HeapData {
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

  virtual absl::string_view name() const = 0;

  std::string DebugString() const { return std::string(name()); }

  virtual void HashValue(absl::HashState state) const;

  virtual bool Equals(const Type& other) const;

  // Find the field definition for the given identifier.
  absl::StatusOr<Field> FindField(TypeManager& type_manager, FieldId id) const;

 protected:
  StructType();

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

  StructType(const StructType&) = delete;
  StructType(StructType&&) = delete;

  // Called by CEL_IMPLEMENT_STRUCT_TYPE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;
};

// CEL_DECLARE_STRUCT_TYPE declares `struct_type` as an struct type. It must be
// part of the class definition of `struct_type`.
//
// class MyStructType : public cel::StructType {
//  ...
// private:
//   CEL_DECLARE_STRUCT_TYPE(MyStructType);
// };
#define CEL_DECLARE_STRUCT_TYPE(struct_type) \
  CEL_INTERNAL_DECLARE_TYPE(Struct, struct_type)

// CEL_IMPLEMENT_ENUM_TYPE implements `struct_type` as an struct type. It
// must be called after the class definition of `struct_type`.
//
// class MyStructType : public cel::StructType {
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

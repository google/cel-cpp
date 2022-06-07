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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPES_ENUM_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPES_ENUM_TYPE_H_

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

class TypedEnumValueFactory;
class TypeManager;

// EnumType represents an enumeration type. An enumeration is a set of constants
// that can be looked up by name and/or number.
class EnumType : public Type {
 public:
  struct Constant;

  class ConstantId final {
   public:
    explicit ConstantId(absl::string_view name)
        : data_(absl::in_place_type<absl::string_view>, name) {}

    explicit ConstantId(int64_t number)
        : data_(absl::in_place_type<int64_t>, number) {}

    ConstantId() = delete;

    ConstantId(const ConstantId&) = default;
    ConstantId& operator=(const ConstantId&) = default;

   private:
    friend class EnumType;
    friend class EnumValue;

    absl::variant<absl::string_view, int64_t> data_;
  };

  Kind kind() const final { return Kind::kEnum; }

  // Find the constant definition for the given identifier.
  absl::StatusOr<Constant> FindConstant(ConstantId id) const;

 protected:
  EnumType() = default;

  // Construct a new instance of EnumValue with a type of this. Called by
  // EnumValue::New.
  virtual absl::StatusOr<Persistent<const EnumValue>> NewInstanceByName(
      TypedEnumValueFactory& factory, absl::string_view name) const = 0;

  // Construct a new instance of EnumValue with a type of this. Called by
  // EnumValue::New.
  virtual absl::StatusOr<Persistent<const EnumValue>> NewInstanceByNumber(
      TypedEnumValueFactory& factory, int64_t number) const = 0;

  // Called by FindConstant.
  virtual absl::StatusOr<Constant> FindConstantByName(
      absl::string_view name) const = 0;

  // Called by FindConstant.
  virtual absl::StatusOr<Constant> FindConstantByNumber(
      int64_t number) const = 0;

 private:
  friend internal::TypeInfo base_internal::GetEnumTypeTypeId(
      const EnumType& enum_type);
  struct NewInstanceVisitor;
  struct FindConstantVisitor;

  friend struct NewInstanceVisitor;
  friend struct FindConstantVisitor;
  friend class EnumValue;
  friend class TypeFactory;
  friend class base_internal::TypeHandleBase;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kEnum; }

  EnumType(const EnumType&) = delete;
  EnumType(EnumType&&) = delete;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by CEL_IMPLEMENT_ENUM_TYPE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;
};

// CEL_DECLARE_ENUM_TYPE declares `enum_type` as an enumeration type. It must be
// part of the class definition of `enum_type`.
//
// class MyEnumType : public cel::EnumType {
//  ...
// private:
//   CEL_DECLARE_ENUM_TYPE(MyEnumType);
// };
#define CEL_DECLARE_ENUM_TYPE(enum_type) \
  CEL_INTERNAL_DECLARE_TYPE(Enum, enum_type)

// CEL_IMPLEMENT_ENUM_TYPE implements `enum_type` as an enumeration type. It
// must be called after the class definition of `enum_type`.
//
// class MyEnumType : public cel::EnumType {
//  ...
// private:
//   CEL_DECLARE_ENUM_TYPE(MyEnumType);
// };
//
// CEL_IMPLEMENT_ENUM_TYPE(MyEnumType);
#define CEL_IMPLEMENT_ENUM_TYPE(enum_type) \
  CEL_INTERNAL_IMPLEMENT_TYPE(Enum, enum_type)

struct EnumType::Constant final {
  explicit Constant(absl::string_view name, int64_t number)
      : name(name), number(number) {}

  // The unqualified enumeration value name.
  absl::string_view name;
  // The enumeration value number.
  int64_t number;
};

CEL_INTERNAL_TYPE_DECL(EnumType);

namespace base_internal {

inline internal::TypeInfo GetEnumTypeTypeId(const EnumType& enum_type) {
  return enum_type.TypeId();
}

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_ENUM_TYPE_H_

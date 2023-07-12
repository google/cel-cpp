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
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/type.h"
#include "internal/rtti.h"

namespace cel {

class MemoryManager;
class EnumValue;
class TypedEnumValueFactory;
class TypeManager;

// EnumType represents an enumeration type. An enumeration is a set of constants
// that can be looked up by name and/or number.
class EnumType : public Type, public base_internal::HeapData {
 public:
  struct Constant;

  class ConstantId final {
   public:
    ConstantId() = delete;

    ConstantId(const ConstantId&) = default;
    ConstantId(ConstantId&&) = default;
    ConstantId& operator=(const ConstantId&) = default;
    ConstantId& operator=(ConstantId&&) = default;

    std::string DebugString() const;

    friend bool operator==(const ConstantId& lhs, const ConstantId& rhs) {
      return lhs.data_ == rhs.data_;
    }

    friend bool operator<(const ConstantId& lhs, const ConstantId& rhs);

    template <typename H>
    friend H AbslHashValue(H state, const ConstantId& id) {
      return H::combine(std::move(state), id.data_);
    }

    template <typename S>
    friend void AbslStringify(S& sink, const ConstantId& id) {
      sink.Append(id.DebugString());
    }

   private:
    friend class EnumType;
    friend class EnumValue;

    explicit ConstantId(absl::string_view name)
        : data_(absl::in_place_type<absl::string_view>, name) {}

    explicit ConstantId(int64_t number)
        : data_(absl::in_place_type<int64_t>, number) {}

    absl::variant<absl::string_view, int64_t> data_;
  };

  static constexpr TypeKind kKind = TypeKind::kEnum;

  using Type::Is;

  static bool Is(const Type& type) { return type.kind() == kKind; }

  static const EnumType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to enum";
    return static_cast<const EnumType&>(type);
  }

  TypeKind kind() const { return kKind; }

  virtual absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  std::string DebugString() const { return std::string(name()); }

  virtual size_t constant_count() const = 0;

  // Find the constant definition for the given identifier. If the constant does
  // not exist, an OK status and empty optional is returned. If the constant
  // exists, an OK status and the constant is returned. Otherwise an error is
  // returned.
  absl::StatusOr<absl::optional<Constant>> FindConstant(ConstantId id) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // Called by FindConstant.
  virtual absl::StatusOr<absl::optional<Constant>> FindConstantByName(
      absl::string_view name) const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  // Called by FindConstant.
  virtual absl::StatusOr<absl::optional<Constant>> FindConstantByNumber(
      int64_t number) const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  class ConstantIterator;

  // Returns an iterator which can iterate over all the constants defined by
  // this enumeration. The order with which iteration occurs is undefined.
  virtual absl::StatusOr<UniqueRef<ConstantIterator>> NewConstantIterator(
      MemoryManager& memory_manager) const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  absl::StatusOr<Handle<Value>> NewValueFromAny(ValueFactory& value_factory,
                                                const absl::Cord& value) const;

 protected:
  static ConstantId MakeConstantId(absl::string_view name) {
    return ConstantId(name);
  }

  static ConstantId MakeConstantId(int64_t number) {
    return ConstantId(number);
  }

  template <typename E>
  static std::enable_if_t<
      std::conjunction_v<
          std::is_enum<E>,
          std::is_convertible<std::underlying_type_t<E>, int64_t>>,
      ConstantId>
  MakeConstantId(E e) {
    return MakeConstantId(
        static_cast<int64_t>(static_cast<std::underlying_type_t<E>>(e)));
  }

  EnumType();

 private:
  friend internal::TypeInfo base_internal::GetEnumTypeTypeId(
      const EnumType& enum_type);
  struct NewInstanceVisitor;
  struct FindConstantVisitor;

  friend struct NewInstanceVisitor;
  friend struct FindConstantVisitor;
  friend class MemoryManager;
  friend class EnumValue;
  friend class TypeFactory;
  friend class base_internal::TypeHandle;

  EnumType(const EnumType&) = delete;
  EnumType(EnumType&&) = delete;

  // Called by CEL_IMPLEMENT_ENUM_TYPE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;
};

// Constant describes a single value in an enumeration. All fields are valid so
// long as EnumType is valid.
struct EnumType::Constant final {
  Constant(ConstantId id, absl::string_view name, int64_t number,
           const void* hint = nullptr)
      : id(id), name(name), number(number), hint(hint) {}

  // Identifier which allows the most efficient form of lookup, compared to
  // looking up by name or number.
  ConstantId id;
  // The unqualified enumeration value name.
  absl::string_view name;
  // The enumeration value number.
  int64_t number;
  // Some implementation-specific data that can be laundered to the value
  // implementation for this type to perform optimizations.
  const void* hint = nullptr;
};

class EnumType::ConstantIterator {
 public:
  using Constant = EnumType::Constant;

  virtual ~ConstantIterator() = default;

  ABSL_MUST_USE_RESULT virtual bool HasNext() = 0;

  virtual absl::StatusOr<Constant> Next() = 0;

  virtual absl::StatusOr<absl::string_view> NextName();

  virtual absl::StatusOr<int64_t> NextNumber();
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

CEL_INTERNAL_TYPE_DECL(EnumType);

namespace base_internal {

inline internal::TypeInfo GetEnumTypeTypeId(const EnumType& enum_type) {
  return enum_type.TypeId();
}

}  // namespace base_internal

namespace base_internal {

template <>
struct TypeTraits<EnumType> {
  using value_type = EnumValue;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_ENUM_TYPE_H_

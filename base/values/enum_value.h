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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_ENUM_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_ENUM_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/enum_type.h"
#include "base/value.h"
#include "internal/rtti.h"

namespace cel {

class ValueFactory;

// EnumValue represents a single constant belonging to cel::EnumType.
class EnumValue : public Value {
 public:
  static absl::StatusOr<Persistent<const EnumValue>> New(
      const Persistent<const EnumType>& enum_type, ValueFactory& value_factory,
      EnumType::ConstantId id);

  Persistent<const Type> type() const final { return type_; }

  Kind kind() const final { return Kind::kEnum; }

  virtual int64_t number() const = 0;

  virtual absl::string_view name() const = 0;

 protected:
  explicit EnumValue(const Persistent<const EnumType>& type) : type_(type) {
    ABSL_ASSERT(type_);
  }

 private:
  friend internal::TypeInfo base_internal::GetEnumValueTypeId(
      const EnumValue& enum_value);
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kEnum; }

  EnumValue(const EnumValue&) = delete;
  EnumValue(EnumValue&&) = delete;

  bool Equals(const Value& other) const final;
  void HashValue(absl::HashState state) const final;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by CEL_IMPLEMENT_ENUM_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  Persistent<const EnumType> type_;
};

// CEL_DECLARE_ENUM_VALUE declares `enum_value` as an enumeration value. It must
// be part of the class definition of `enum_value`.
//
// class MyEnumValue : public cel::EnumValue {
//  ...
// private:
//   CEL_DECLARE_ENUM_VALUE(MyEnumValue);
// };
#define CEL_DECLARE_ENUM_VALUE(enum_value) \
  CEL_INTERNAL_DECLARE_VALUE(Enum, enum_value)

// CEL_IMPLEMENT_ENUM_VALUE implements `enum_value` as an enumeration value. It
// must be called after the class definition of `enum_value`.
//
// class MyEnumValue : public cel::EnumValue {
//  ...
// private:
//   CEL_DECLARE_ENUM_VALUE(MyEnumValue);
// };
//
// CEL_IMPLEMENT_ENUM_VALUE(MyEnumValue);
#define CEL_IMPLEMENT_ENUM_VALUE(enum_value) \
  CEL_INTERNAL_IMPLEMENT_VALUE(Enum, enum_value)

namespace base_internal {

inline internal::TypeInfo GetEnumValueTypeId(const EnumValue& enum_value) {
  return enum_value.TypeId();
}

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_ENUM_VALUE_H_

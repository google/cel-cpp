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
#include <utility>

#include "absl/base/macros.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "internal/rtti.h"

namespace cel {

class ValueFactory;

// StructValue represents an instance of cel::StructType.
class StructValue : public Value {
 public:
  using FieldId = StructType::FieldId;

  static absl::StatusOr<Persistent<StructValue>> New(
      const Persistent<const StructType>& struct_type,
      ValueFactory& value_factory);

  Persistent<const Type> type() const final { return type_; }

  Kind kind() const final { return Kind::kStruct; }

  absl::Status SetField(FieldId field, const Persistent<const Value>& value);

  absl::StatusOr<Persistent<const Value>> GetField(ValueFactory& value_factory,
                                                   FieldId field) const;

  absl::StatusOr<bool> HasField(FieldId field) const;

 protected:
  explicit StructValue(const Persistent<const StructType>& type) : type_(type) {
    ABSL_ASSERT(type_);
  }

  virtual absl::Status SetFieldByName(absl::string_view name,
                                      const Persistent<const Value>& value) = 0;

  virtual absl::Status SetFieldByNumber(
      int64_t number, const Persistent<const Value>& value) = 0;

  virtual absl::StatusOr<Persistent<const Value>> GetFieldByName(
      ValueFactory& value_factory, absl::string_view name) const = 0;

  virtual absl::StatusOr<Persistent<const Value>> GetFieldByNumber(
      ValueFactory& value_factory, int64_t number) const = 0;

  virtual absl::StatusOr<bool> HasFieldByName(absl::string_view name) const = 0;

  virtual absl::StatusOr<bool> HasFieldByNumber(int64_t number) const = 0;

 private:
  struct SetFieldVisitor;
  struct GetFieldVisitor;
  struct HasFieldVisitor;

  friend struct SetFieldVisitor;
  friend struct GetFieldVisitor;
  friend struct HasFieldVisitor;
  friend internal::TypeInfo base_internal::GetStructValueTypeId(
      const StructValue& struct_value);
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kStruct; }

  StructValue(const StructValue&) = delete;
  StructValue(StructValue&&) = delete;

  bool Equals(const Value& other) const override = 0;
  void HashValue(absl::HashState state) const override = 0;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by CEL_IMPLEMENT_STRUCT_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  Persistent<const StructType> type_;
};

// CEL_DECLARE_STRUCT_VALUE declares `struct_value` as an struct value. It must
// be part of the class definition of `struct_value`.
//
// class MyStructValue : public cel::StructValue {
//  ...
// private:
//   CEL_DECLARE_STRUCT_VALUE(MyStructValue);
// };
#define CEL_DECLARE_STRUCT_VALUE(struct_value) \
  CEL_INTERNAL_DECLARE_VALUE(Struct, struct_value)

// CEL_IMPLEMENT_STRUCT_VALUE implements `struct_value` as an struct
// value. It must be called after the class definition of `struct_value`.
//
// class MyStructValue : public cel::StructValue {
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

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_STRUCT_VALUE_H_

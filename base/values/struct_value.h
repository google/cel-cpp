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
class StructValue : public Value, public base_internal::HeapData {
 public:
  static constexpr Kind kKind = Kind::kStruct;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  using FieldId = StructType::FieldId;

  static absl::StatusOr<Persistent<StructValue>> New(
      const Persistent<const StructType>& struct_type,
      ValueFactory& value_factory);

  constexpr Kind kind() const { return kKind; }

  constexpr const Persistent<const StructType>& type() const { return type_; }

  virtual std::string DebugString() const = 0;

  virtual void HashValue(absl::HashState state) const = 0;

  virtual bool Equals(const Value& other) const = 0;

  absl::Status SetField(FieldId field, const Persistent<const Value>& value);

  absl::StatusOr<Persistent<const Value>> GetField(ValueFactory& value_factory,
                                                   FieldId field) const;

  absl::StatusOr<bool> HasField(FieldId field) const;

 protected:
  explicit StructValue(Persistent<const StructType> type);

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
  friend class base_internal::PersistentValueHandle;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.

  StructValue(const StructValue&) = delete;
  StructValue(StructValue&&) = delete;

  // Called by CEL_IMPLEMENT_STRUCT_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  const Persistent<const StructType> type_;
};

CEL_INTERNAL_VALUE_DECL(StructValue);

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

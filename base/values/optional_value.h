// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_OPTIONAL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_OPTIONAL_VALUE_H_

#include <string>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/type.h"
#include "base/types/optional_type.h"
#include "base/value.h"
#include "base/values/opaque_value.h"
#include "internal/rtti.h"

namespace cel {

class ValueFactory;

namespace base_internal {
class EmptyOptionalValue;
class FullOptionalValue;
}  // namespace base_internal

class OptionalValue : public OpaqueValue {
 public:
  static bool Is(const Value& value) {
    return OpaqueValue::Is(value) &&
           OpaqueValue::TypeId(static_cast<const OpaqueValue&>(value)) ==
               internal::TypeId<OptionalValue>();
  }

  using OpaqueValue::Is;

  static const OptionalValue& Cast(const Value& value) {
    ABSL_DCHECK(Is(value)) << "cannot cast " << value.type()->DebugString()
                           << " to optional";
    return static_cast<const OptionalValue&>(value);
  }

  // Create a new optional value which does not have a value. If the type is not
  // yet known, use `DynType`.
  static absl::StatusOr<Handle<OptionalValue>> None(ValueFactory& value_factory,
                                                    Handle<Type> type);

  // Create a new optional value which has a value.
  static absl::StatusOr<Handle<OptionalValue>> Of(ValueFactory& value_factory,
                                                  Handle<Value> value);

  const Handle<OptionalType>& type() const {
    return OpaqueValue::type().As<OptionalType>();
  }

  std::string DebugString() const final;

  virtual bool has_value() const = 0;

  // Requires `OptionalValue::has_value()` be true, otherwise behavior is
  // undefined.
  virtual const Handle<Value>& value() const = 0;

 private:
  friend class base_internal::EmptyOptionalValue;
  friend class base_internal::FullOptionalValue;

  explicit OptionalValue(Handle<OptionalType> type)
      : OpaqueValue(std::move(type)) {}

  internal::TypeInfo TypeId() const final {
    return internal::TypeId<OptionalValue>();
  }
};

namespace base_internal {

class EmptyOptionalValue final : public OptionalValue {
 public:
  bool has_value() const override { return false; }

  const Handle<Value>& value() const override;

 private:
  friend class cel::MemoryManager;

  // Called by Arena-based memory managers to determine whether we actually need
  // our destructor called.
  static bool IsDestructorSkippable(const EmptyOptionalValue& value) noexcept {
    return base_internal::Metadata::IsDestructorSkippable(*value.type());
  }

  explicit EmptyOptionalValue(Handle<OptionalType> type)
      : OptionalValue(std::move(type)) {}
};

class FullOptionalValue final : public OptionalValue {
 public:
  bool has_value() const override { return true; }

  const Handle<Value>& value() const override { return value_; }

 private:
  friend class cel::MemoryManager;

  // Called by Arena-based memory managers to determine whether we actually need
  // our destructor called.
  static bool IsDestructorSkippable(const FullOptionalValue& value) noexcept {
    return base_internal::Metadata::IsDestructorSkippable(*value.type()) &&
           base_internal::Metadata::IsDestructorSkippable(*value.value());
  }

  FullOptionalValue(Handle<OptionalType> type, Handle<Value> value)
      : OptionalValue(std::move(type)), value_(std::move(value)) {
    ABSL_CHECK(static_cast<bool>(value_));  // Crask OK
  }

  const Handle<Value> value_;
};

}  // namespace base_internal

extern template class Handle<OptionalValue>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_OPTIONAL_VALUE_H_

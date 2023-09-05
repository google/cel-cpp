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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_DOUBLE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_DOUBLE_VALUE_H_

#include <string>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "base/types/double_type.h"
#include "base/value.h"

namespace cel {

class DoubleValue final
    : public base_internal::SimpleValue<DoubleType, double> {
 private:
  using Base = base_internal::SimpleValue<DoubleType, double>;

 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static std::string DebugString(double value);

  using Base::kKind;

  using Base::Is;

  static const DoubleValue& Cast(const Value& value) {
    ABSL_DCHECK(Is(value)) << "cannot cast " << value.type()->name()
                           << " to double";
    return static_cast<const DoubleValue&>(value);
  }

  static Handle<DoubleValue> NaN(ValueFactory& value_factory);

  static Handle<DoubleValue> PositiveInfinity(ValueFactory& value_factory);

  static Handle<DoubleValue> NegativeInfinity(ValueFactory& value_factory);

  using Base::kind;

  using Base::type;

  std::string DebugString() const;

  absl::StatusOr<Any> ConvertToAny(ValueFactory&) const;

  absl::StatusOr<Json> ConvertToJson(ValueFactory&) const;

  absl::StatusOr<Handle<Value>> Equals(ValueFactory& value_factory,
                                       const Value& other) const;

  using Base::value;

 private:
  using Base::Base;

  CEL_INTERNAL_SIMPLE_VALUE_MEMBERS(DoubleValue);
};

CEL_INTERNAL_SIMPLE_VALUE_STANDALONES(DoubleValue);

inline bool operator==(const DoubleValue& lhs, const DoubleValue& rhs) {
  return lhs.value() == rhs.value();
}

namespace base_internal {

template <>
struct ValueTraits<DoubleValue> {
  using type = DoubleValue;

  using type_type = DoubleType;

  using underlying_type = double;

  static std::string DebugString(underlying_type value) {
    return type::DebugString(value);
  }

  static std::string DebugString(const type& value) {
    return value.DebugString();
  }

  static Handle<type> Wrap(ValueFactory& value_factory, underlying_type value);

  static underlying_type Unwrap(underlying_type value) { return value; }

  static underlying_type Unwrap(const Handle<type>& value) {
    return Unwrap(value->value());
  }
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_DOUBLE_VALUE_H_

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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_NULL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_NULL_VALUE_H_

#include <string>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/types/null_type.h"
#include "base/value.h"
#include "common/any.h"
#include "common/json.h"

namespace cel {

class NullValue final : public base_internal::SimpleValue<NullType, void>,
                        public base_internal::EnableHandleFromThis<NullValue> {
 private:
  using Base = base_internal::SimpleValue<NullType, void>;

 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static std::string DebugString();

  using Base::kKind;

  using Base::Is;

  static const NullValue& Cast(const Value& value) {
    ABSL_DCHECK(Is(value)) << "cannot cast " << value.type()->name()
                           << " to null";
    return static_cast<const NullValue&>(value);
  }

  static Handle<NullValue> Get(ValueFactory& value_factory);

  absl::StatusOr<Handle<Value>> Equals(ValueFactory& value_factory,
                                       const Value& other) const;

  using Base::kind;

  using Base::type;

  absl::StatusOr<Any> ConvertToAny(ValueFactory&) const;

  absl::StatusOr<Json> ConvertToJson(ValueFactory&) const;

  absl::StatusOr<Handle<Value>> ConvertToType(ValueFactory& value_factory,
                                              const Handle<Type>& type) const;

 private:
  NullValue() = default;
  CEL_INTERNAL_SIMPLE_VALUE_MEMBERS(NullValue);
};

CEL_INTERNAL_SIMPLE_VALUE_STANDALONES(NullValue);

namespace base_internal {

template <>
struct ValueTraits<NullValue> {
  using type = NullValue;

  using type_type = NullType;

  using underlying_type = void;

  static std::string DebugString(const type& value) {
    return value.DebugString();
  }

  static Handle<type> Wrap(ValueFactory& value_factory, Handle<type> value) {
    static_cast<void>(value_factory);
    return value;
  }

  static Handle<type> Unwrap(Handle<type> value) { return value; }
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_NULL_VALUE_H_

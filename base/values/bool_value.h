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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_BOOL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_BOOL_VALUE_H_

#include <string>

#include "base/types/bool_type.h"
#include "base/value.h"

namespace cel {

class BoolValue final : public base_internal::SimpleValue<BoolType, bool> {
 private:
  using Base = base_internal::SimpleValue<BoolType, bool>;

 public:
  using Base::kKind;

  using Base::Is;

  static Persistent<const BoolValue> False(ValueFactory& value_factory);

  static Persistent<const BoolValue> True(ValueFactory& value_factory);

  using Base::kind;

  using Base::type;

  std::string DebugString() const;

  using Base::HashValue;

  using Base::Equals;

  using Base::value;

 private:
  using Base::Base;

  CEL_INTERNAL_SIMPLE_VALUE_MEMBERS(BoolValue);
};

CEL_INTERNAL_SIMPLE_VALUE_STANDALONES(BoolValue);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_BOOL_VALUE_H_

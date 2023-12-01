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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_INTERFACE_H_

#include <string>

#include "absl/base/attributes.h"
#include "common/casting.h"
#include "common/internal/data_interface.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class ValueInterface : public common_internal::DataInterface {
 public:
  using DataInterface::DataInterface;

  ABSL_ATTRIBUTE_PURE_FUNCTION
  virtual ValueKind kind() const = 0;

  TypeView type() const { return get_type(); }

  virtual std::string DebugString() const = 0;

 protected:
  ABSL_ATTRIBUTE_PURE_FUNCTION
  virtual TypeView get_type() const = 0;
};

// Enable `InstanceOf`, `Cast`, and `As` using subsumption relationships.
template <typename To, typename From>
struct CastTraits<To, From,
                  EnableIfSubsumptionCastable<To, From, ValueInterface>>
    : SubsumptionCastTraits<To, From> {};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_INTERFACE_H_

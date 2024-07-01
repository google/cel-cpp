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

// IWYU pragma: private, include "common/type.h"
// IWYU pragma: friend "common/type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTERFACE_H_

#include <string>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/internal/data_interface.h"
#include "common/type_kind.h"

namespace cel {

class TypeInterface : public common_internal::DataInterface {
 public:
  using DataInterface::DataInterface;

  virtual TypeKind kind() const = 0;

  virtual absl::string_view name() const = 0;

  virtual std::string DebugString() const = 0;
};

// Enable `InstanceOf`, `Cast`, and `As` using subsumption relationships.
template <typename To, typename From>
struct CastTraits<To, From,
                  EnableIfSubsumptionCastable<To, From, TypeInterface>>
    : SubsumptionCastTraits<To, From> {};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_INTERFACE_H_

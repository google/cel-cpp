// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_OPERATORS_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_OPERATORS_H_

#include "absl/strings/string_view.h"

namespace cel {

enum class OperatorId;

namespace base_internal {

struct OperatorData final {
  OperatorData() = delete;

  OperatorData(const OperatorData&) = delete;

  OperatorData(OperatorData&&) = delete;

  constexpr OperatorData(cel::OperatorId id, absl::string_view name,
                         absl::string_view display_name, int precedence,
                         int arity)
      : id(id),
        name(name),
        display_name(display_name),
        precedence(precedence),
        arity(arity) {}

  const cel::OperatorId id;
  const absl::string_view name;
  const absl::string_view display_name;
  const int precedence;
  const int arity;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_OPERATORS_H_

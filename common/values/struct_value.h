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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_

#include <cstdint>

#include "absl/strings/string_view.h"

namespace cel {

class Value;

class StructValueBuilder {
 public:
  virtual ~StructValueBuilder() = default;

  virtual void SetFieldByName(absl::string_view name, Value value) = 0;

  virtual void SetFieldByNumber(int64_t number, Value value) = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_

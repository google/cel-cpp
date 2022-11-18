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

#include "base/values/error_value.h"

#include <algorithm>
#include <string>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(ErrorValue);

std::string ErrorValue::DebugString() const { return value().ToString(); }

const absl::Status& ErrorValue::value() const {
  return base_internal::Metadata::IsTrivial(*this) ? *value_ptr_ : value_;
}

}  // namespace cel

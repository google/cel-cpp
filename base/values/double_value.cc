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

#include "base/values/double_value.h"

#include <cmath>
#include <string>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(DoubleValue);

std::string DoubleValue::DebugString() const {
  if (std::isfinite(value())) {
    if (std::floor(value()) != value()) {
      // The double is not representable as a whole number, so use
      // absl::StrCat which will add decimal places.
      return absl::StrCat(value());
    }
    // absl::StrCat historically would represent 0.0 as 0, and we want the
    // decimal places so ZetaSQL correctly assumes the type as double
    // instead of int64_t.
    std::string stringified = absl::StrCat(value());
    if (!absl::StrContains(stringified, '.')) {
      absl::StrAppend(&stringified, ".0");
    } else {
      // absl::StrCat has a decimal now? Use it directly.
    }
    return stringified;
  }
  if (std::isnan(value())) {
    return "nan";
  }
  if (std::signbit(value())) {
    return "-infinity";
  }
  return "+infinity";
}

}  // namespace cel

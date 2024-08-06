// Copyright 2024 Google LLC
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

#include <array>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "common/type.h"

namespace cel {

bool IsWellKnownMessageType(absl::string_view name) {
  ABSL_CONST_INIT static constexpr absl::string_view kPrefix =
      "google.protobuf.";
  ABSL_CONST_INIT static constexpr std::array<absl::string_view, 15> kNames = {
      "Any",         "BoolValue",   "BytesValue", "DoubleValue", "Duration",
      "FloatValue",  "Int32Value",  "Int64Value", "ListValue",   "UInt32Value",
      "UInt64Value", "StringValue", "Struct",     "Timestamp",   "Value",
  };
  if (!absl::ConsumePrefix(&name, kPrefix)) {
    return false;
  }
  return absl::c_binary_search(kNames, name);
}

}  // namespace cel

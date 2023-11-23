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

#include <string>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "common/value.h"

namespace cel {

namespace {

std::string ErrorDebugString(const absl::Status& value) {
  ABSL_DCHECK(!value.ok()) << "use of moved-from ErrorValue";
  return value.ToString(absl::StatusToStringMode::kWithEverything);
}

}  // namespace

std::string ErrorValue::DebugString() const { return ErrorDebugString(value_); }

std::string ErrorValueView::DebugString() const {
  return ErrorDebugString(*value_);
}

}  // namespace cel

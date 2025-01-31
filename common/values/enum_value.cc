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

#include "google/protobuf/wrappers.pb.h"
#include "absl/status/statusor.h"
#include "common/value.h"

namespace cel {

absl::StatusOr<Value> EnumValue::Equal(ValueManager& value_manager,
                                       const Value& other) const {
  return IntValue(NativeValue()).Equal(value_manager, other);
}

}  // namespace cel

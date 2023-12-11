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

#include "base/types/duration_type.h"

#include "absl/strings/cord.h"
#include "absl/time/time.h"
#include "base/value_factory.h"
#include "base/values/duration_value.h"
#include "internal/deserialize.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

using internal::DeserializeDuration;

}  // namespace

CEL_INTERNAL_TYPE_IMPL(DurationType);

absl::StatusOr<Handle<DurationValue>> DurationType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.Duration.
  CEL_ASSIGN_OR_RETURN(auto deserialized_value, DeserializeDuration(value));
  return value_factory.CreateDurationValue(deserialized_value);
}

}  // namespace cel

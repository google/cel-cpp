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

#include "base/values/opaque_value.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "base/value_factory.h"

namespace cel {

template class Handle<OpaqueValue>;

absl::StatusOr<Any> OpaqueValue::ConvertToAny(
    ValueFactory& value_factory) const {
  return absl::FailedPreconditionError(absl::StrCat(
      type()->name(), " cannot be serialized as google.protobuf.Any"));
}

absl::StatusOr<Json> OpaqueValue::ConvertToJson(
    ValueFactory& value_factory) const {
  return absl::FailedPreconditionError(absl::StrCat(
      type()->name(), " cannot be serialized as google.protobuf.Value"));
}

absl::StatusOr<Handle<Value>> OpaqueValue::Equals(ValueFactory& value_factory,
                                                  const Value&) const {
  return value_factory.CreateBoolValue(false);
}

}  // namespace cel

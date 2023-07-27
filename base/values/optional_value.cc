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

#include "base/values/optional_value.h"

#include <cstdlib>
#include <string>
#include <utility>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "internal/status_macros.h"

namespace cel {

template class Handle<OptionalValue>;

absl::StatusOr<Handle<OptionalValue>> OptionalValue::None(
    ValueFactory& value_factory, Handle<Type> type) {
  CEL_ASSIGN_OR_RETURN(
      auto optional_type,
      value_factory.type_factory().CreateOptionalType(std::move(type)));
  return base_internal::HandleFactory<OptionalValue>::template Make<
      base_internal::EmptyOptionalValue>(value_factory.memory_manager(),
                                         std::move(optional_type));
}

absl::StatusOr<Handle<OptionalValue>> OptionalValue::Of(
    ValueFactory& value_factory, Handle<Value> value) {
  CEL_ASSIGN_OR_RETURN(
      auto optional_type,
      value_factory.type_factory().CreateOptionalType(value->type()));
  return base_internal::HandleFactory<OptionalValue>::template Make<
      base_internal::FullOptionalValue>(value_factory.memory_manager(),
                                        std::move(optional_type),
                                        std::move(value));
}

std::string OptionalValue::DebugString() const {
  if (!has_value()) {
    return "optional()";
  }
  return absl::StrCat("optional(", value()->DebugString(), ")");
}

absl::StatusOr<Any> OptionalValue::ConvertToAny(
    ValueFactory& value_factory) const {
  if (!has_value()) {
    return absl::FailedPreconditionError(
        "empty optional values cannot be serialized as google.protobuf.Any");
  }
  return value()->ConvertToAny(value_factory);
}

absl::StatusOr<Json> OptionalValue::ConvertToJson(
    ValueFactory& value_factory) const {
  if (!has_value()) {
    return absl::FailedPreconditionError(
        "empty optional values cannot be serialized as google.protobuf.Value");
  }
  return value()->ConvertToJson(value_factory);
}

namespace base_internal {

const Handle<Value>& EmptyOptionalValue::value() const {
  ABSL_LOG(FATAL) << "cannot access value of empty optional";  // Crask OK
  std::abort();                                                // Crash OK
}

}  // namespace base_internal

}  // namespace cel

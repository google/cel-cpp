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

#include "extensions/protobuf/value.h"

#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "extensions/protobuf/internal/time.h"
#include "internal/status_macros.h"

namespace cel::extensions {

absl::StatusOr<Handle<Value>> ProtoValue::Create(ValueFactory& value_factory,
                                                 const google::protobuf::Message& value) {
  const auto* desc = value.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError("protocol buffer message missing descriptor");
  }
  const auto& type_name = desc->full_name();
  if (type_name == "google.protobuf.Duration") {
    CEL_ASSIGN_OR_RETURN(
        auto duration, protobuf_internal::AbslDurationFromDurationProto(value));
    return value_factory.CreateUncheckedDurationValue(duration);
  }
  if (type_name == "google.protobuf.Timestamp") {
    CEL_ASSIGN_OR_RETURN(auto time,
                         protobuf_internal::AbslTimeFromTimestampProto(value));
    return value_factory.CreateUncheckedTimestampValue(time);
  }
  return ProtoStructValue::Create(value_factory, value);
}

absl::StatusOr<Handle<Value>> ProtoValue::Create(ValueFactory& value_factory,
                                                 google::protobuf::Message&& value) {
  const auto* desc = value.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError("protocol buffer message missing descriptor");
  }
  const auto& type_name = desc->full_name();
  if (type_name == "google.protobuf.Duration") {
    CEL_ASSIGN_OR_RETURN(
        auto duration, protobuf_internal::AbslDurationFromDurationProto(value));
    return value_factory.CreateUncheckedDurationValue(duration);
  }
  if (type_name == "google.protobuf.Timestamp") {
    CEL_ASSIGN_OR_RETURN(auto time,
                         protobuf_internal::AbslTimeFromTimestampProto(value));
    return value_factory.CreateUncheckedTimestampValue(time);
  }
  return ProtoStructValue::Create(value_factory, std::move(value));
}

}  // namespace cel::extensions

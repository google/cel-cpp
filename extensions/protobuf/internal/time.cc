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

#include "extensions/protobuf/internal/time.h"

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions::protobuf_internal {

namespace {

absl::StatusOr<absl::Duration> AbslDurationFromProto(
    const google::protobuf::Message& message, int seconds_field_number,
    int nanos_field_number) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  // seconds is field number 1 on google.protobuf.Duration and
  // google.protobuf.Timestamp.
  const auto* seconds_field = desc->FindFieldByNumber(seconds_field_number);
  if (ABSL_PREDICT_FALSE(seconds_field == nullptr)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(), " missing seconds field descriptor"));
  }
  if (ABSL_PREDICT_FALSE(seconds_field->cpp_type() !=
                         google::protobuf::FieldDescriptor::CPPTYPE_INT64)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(), " has unexpected seconds field type: ",
        seconds_field->cpp_type_name()));
  }
  // nanos is field number 2 on google.protobuf.Duration and
  // google.protobuf.Timestamp.
  const auto* nanos_field = desc->FindFieldByNumber(nanos_field_number);
  if (ABSL_PREDICT_FALSE(nanos_field == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing nanos field descriptor"));
  }
  if (ABSL_PREDICT_FALSE(nanos_field->cpp_type() !=
                         google::protobuf::FieldDescriptor::CPPTYPE_INT32)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(),
        " has unexpected nanos field type: ", nanos_field->cpp_type_name()));
  }
  return absl::Seconds(reflect->GetInt64(message, seconds_field)) +
         absl::Nanoseconds(reflect->GetInt32(message, nanos_field));
}

}  // namespace

absl::StatusOr<absl::Duration> AbslDurationFromDurationProto(
    const google::protobuf::Message& message) {
  return AbslDurationFromProto(message,
                               google::protobuf::Duration::kSecondsFieldNumber,
                               google::protobuf::Duration::kNanosFieldNumber);
}

absl::StatusOr<absl::Time> AbslTimeFromTimestampProto(
    const google::protobuf::Message& message) {
  CEL_ASSIGN_OR_RETURN(
      auto duration,
      AbslDurationFromProto(message,
                            google::protobuf::Timestamp::kSecondsFieldNumber,
                            google::protobuf::Timestamp::kNanosFieldNumber));
  return absl::UnixEpoch() + duration;
}

}  // namespace cel::extensions::protobuf_internal

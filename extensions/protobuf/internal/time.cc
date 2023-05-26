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
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions::protobuf_internal {

namespace {

template <typename T>
absl::StatusOr<absl::Duration> AbslDurationFromProto(
    const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (desc == T::descriptor()) {
    // Fast path.
    const auto& derived = cel::internal::down_cast<const T&>(message);
    return absl::Seconds(derived.seconds()) +
           absl::Nanoseconds(derived.nanos());
  }
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  // seconds is field number 1 on google.protobuf.Duration and
  // google.protobuf.Timestamp.
  const auto* seconds_field = desc->FindFieldByNumber(T::kSecondsFieldNumber);
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
  const auto* nanos_field = desc->FindFieldByNumber(T::kNanosFieldNumber);
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

template <typename T>
absl::Status AbslDurationToProto(google::protobuf::Message& message,
                                 absl::Duration duration) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (ABSL_PREDICT_TRUE(desc == T::descriptor())) {
    T& proto = cel::internal::down_cast<T&>(message);
    proto.set_seconds(
        absl::IDivDuration(duration, absl::Seconds(1), &duration));
    proto.set_nanos(static_cast<int32_t>(
        absl::IDivDuration(duration, absl::Nanoseconds(1), &duration)));
    return absl::OkStatus();
  }
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  // seconds is field number 1 on google.protobuf.Duration and
  // google.protobuf.Timestamp.
  const auto* seconds_field = desc->FindFieldByNumber(T::kSecondsFieldNumber);
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
  const auto* nanos_field = desc->FindFieldByNumber(T::kNanosFieldNumber);
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
  reflect->SetInt64(&message, seconds_field,
                    absl::IDivDuration(duration, absl::Seconds(1), &duration));
  reflect->SetInt32(&message, nanos_field,
                    static_cast<int32_t>(absl::IDivDuration(
                        duration, absl::Nanoseconds(1), &duration)));
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<absl::Duration> AbslDurationFromDurationProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Duration");
  return AbslDurationFromProto<google::protobuf::Duration>(message);
}

absl::StatusOr<absl::Time> AbslTimeFromTimestampProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Timestamp");
  CEL_ASSIGN_OR_RETURN(
      auto duration,
      AbslDurationFromProto<google::protobuf::Timestamp>(message));
  return absl::UnixEpoch() + duration;
}

absl::Status AbslDurationToDurationProto(google::protobuf::Message& message,
                                         absl::Duration duration) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Duration");
  return AbslDurationToProto<google::protobuf::Duration>(message, duration);
}

absl::Status AbslTimeToTimestampProto(google::protobuf::Message& message,
                                      absl::Time time) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Timestamp");
  return AbslDurationToProto<google::protobuf::Timestamp>(
      message, time - absl::UnixEpoch());
}

}  // namespace cel::extensions::protobuf_internal

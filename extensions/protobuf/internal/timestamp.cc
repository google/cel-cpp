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

#include "extensions/protobuf/internal/timestamp.h"

#include <cstdint>

#include "google/protobuf/timestamp.pb.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<absl::Time> UnwrapDynamicTimestampProto(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Timestamp");
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (desc == google::protobuf::Timestamp::descriptor()) {
    // Fast path.
    return UnwrapGeneratedTimestampProto(
        google::protobuf::DownCastToGenerated<google::protobuf::Timestamp>(message));
  }
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  const auto* seconds_field =
      desc->FindFieldByNumber(google::protobuf::Timestamp::kSecondsFieldNumber);
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
  if (ABSL_PREDICT_FALSE(seconds_field->is_map() ||
                         seconds_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " has unexpected ",
                     seconds_field->name(), " field cardinality: REPEATED"));
  }
  const auto* nanos_field =
      desc->FindFieldByNumber(google::protobuf::Timestamp::kNanosFieldNumber);
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
  if (ABSL_PREDICT_FALSE(nanos_field->is_map() || nanos_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " has unexpected ",
                     nanos_field->name(), " field cardinality: REPEATED"));
  }
  return absl::UnixEpoch() +
         absl::Seconds(reflect->GetInt64(message, seconds_field)) +
         absl::Nanoseconds(reflect->GetInt32(message, nanos_field));
}

absl::Status WrapDynamicTimestampProto(absl::Time value,
                                       google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.Timestamp");
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if (ABSL_PREDICT_TRUE(desc == google::protobuf::Timestamp::descriptor())) {
    return WrapGeneratedTimestampProto(
        value,
        google::protobuf::DownCastToGenerated<google::protobuf::Timestamp>(message));
  }
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  const auto* seconds_field =
      desc->FindFieldByNumber(google::protobuf::Timestamp::kSecondsFieldNumber);
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
  if (ABSL_PREDICT_FALSE(seconds_field->is_map() ||
                         seconds_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " has unexpected ",
                     seconds_field->name(), " field cardinality: REPEATED"));
  }
  const auto* nanos_field =
      desc->FindFieldByNumber(google::protobuf::Timestamp::kNanosFieldNumber);
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
  if (ABSL_PREDICT_FALSE(nanos_field->is_map() || nanos_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " has unexpected ",
                     nanos_field->name(), " field cardinality: REPEATED"));
  }
  auto duration = value - absl::UnixEpoch();
  reflect->SetInt64(&message, seconds_field,
                    absl::IDivDuration(duration, absl::Seconds(1), &duration));
  reflect->SetInt32(&message, nanos_field,
                    static_cast<int32_t>(absl::IDivDuration(
                        duration, absl::Nanoseconds(1), &duration)));
  return absl::OkStatus();
}

}  // namespace cel::extensions::protobuf_internal

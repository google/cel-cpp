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

#include "extensions/protobuf/internal/field_mask.h"

#include <string>

#include "google/protobuf/field_mask.pb.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "common/json.h"
#include "extensions/protobuf/internal/field_mask_lite.h"
#include "extensions/protobuf/internal/is_generated_message.h"
#include "extensions/protobuf/internal/is_message_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/reflection.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<JsonString> DynamicFieldMaskProtoToJsonString(
    const google::protobuf::Message& message) {
  ABSL_DCHECK_EQ(message.GetTypeName(), "google.protobuf.FieldMask");
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing descriptor"));
  }
  if constexpr (NotMessageLite<google::protobuf::FieldMask>) {
    if (IsGeneratedMessage(message)) {
      return GeneratedFieldMaskProtoToJsonString(
          google::protobuf::DownCastToGenerated<google::protobuf::FieldMask>(message));
    }
  }
  const auto* reflection = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflection == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing reflection"));
  }
  const auto* paths_field =
      desc->FindFieldByNumber(google::protobuf::FieldMask::kPathsFieldNumber);
  if (ABSL_PREDICT_FALSE(paths_field == nullptr)) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(), " missing paths field descriptor"));
  }
  if (ABSL_PREDICT_FALSE(paths_field->cpp_type() !=
                         google::protobuf::FieldDescriptor::CPPTYPE_STRING)) {
    return absl::InternalError(absl::StrCat(
        message.GetTypeName(),
        " has unexpected paths field type: ", paths_field->cpp_type_name()));
  }
  if (ABSL_PREDICT_FALSE(!paths_field->is_repeated())) {
    return absl::InternalError(
        absl::StrCat(message.GetTypeName(),
                     " has unexpected paths field cardinality: UNKNOWN"));
  }
  return JsonString(absl::StrJoin(
      reflection->GetRepeatedFieldRef<std::string>(message, paths_field), ","));
}

}  // namespace cel::extensions::protobuf_internal

// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_PROTO_MESSAGE_TYPE_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_PROTO_MESSAGE_TYPE_ADAPTER_H_

#include <string>
#include <vector>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/memory.h"
#include "base/values/struct_value.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"

namespace google::api::expr::runtime {

// Implementation for legacy struct (message) type apis using reflection.
//
// Note: The type info API implementation attached to message values is
// generally the duck-typed instance to support the default behavior of
// deferring to the protobuf reflection apis on the message instance.
class ProtoMessageTypeAdapter : public LegacyTypeInfoApis,
                                public LegacyTypeAccessApis,
                                public LegacyTypeMutationApis {
 public:
  ProtoMessageTypeAdapter(const google::protobuf::Descriptor* descriptor,
                          google::protobuf::MessageFactory* message_factory)
      : message_factory_(message_factory), descriptor_(descriptor) {}

  ~ProtoMessageTypeAdapter() override = default;

  // Implement LegacyTypeInfoApis
  std::string DebugString(const MessageWrapper& wrapped_message) const override;

  const std::string& GetTypename(
      const MessageWrapper& wrapped_message) const override;

  const LegacyTypeAccessApis* GetAccessApis(
      const MessageWrapper& wrapped_message) const override;

  const LegacyTypeMutationApis* GetMutationApis(
      const MessageWrapper& wrapped_message) const override;

  absl::optional<LegacyTypeInfoApis::FieldDescription> FindFieldByName(
      absl::string_view field_name) const override;

  // Implement LegacyTypeMutation APIs.
  absl::StatusOr<CelValue::MessageWrapper::Builder> NewInstance(
      cel::MemoryManager& memory_manager) const override;

  bool DefinesField(absl::string_view field_name) const override;

  absl::Status SetField(
      absl::string_view field_name, const CelValue& value,
      cel::MemoryManager& memory_manager,
      CelValue::MessageWrapper::Builder& instance) const override;

  absl::StatusOr<CelValue> AdaptFromWellKnownType(
      cel::MemoryManager& memory_manager,
      CelValue::MessageWrapper::Builder instance) const override;

  // Implement LegacyTypeAccessAPIs.
  absl::StatusOr<CelValue> GetField(
      absl::string_view field_name, const CelValue::MessageWrapper& instance,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManager& memory_manager) const override;

  absl::StatusOr<bool> HasField(
      absl::string_view field_name,
      const CelValue::MessageWrapper& value) const override;

  absl::StatusOr<CelValue> Qualify(
      absl::Span<const cel::SelectQualifier> qualifiers,
      const CelValue::MessageWrapper& instance, bool presence_test,
      cel::MemoryManager& memory_manager) const override;

  bool IsEqualTo(const CelValue::MessageWrapper& instance,
                 const CelValue::MessageWrapper& other_instance) const override;

  std::vector<absl::string_view> ListFields(
      const CelValue::MessageWrapper& instance) const override;

 private:
  // Helper for standardizing error messages for SetField operation.
  absl::Status ValidateSetFieldOp(bool assertion, absl::string_view field,
                                  absl::string_view detail) const;

  google::protobuf::MessageFactory* message_factory_;
  const google::protobuf::Descriptor* descriptor_;
};

// Returns a TypeInfo provider representing an arbitrary message.
// This allows for the legacy duck-typed behavior of messages on field access
// instead of expecting a particular message type given a TypeInfo.
const LegacyTypeInfoApis& GetGenericProtoTypeInfoInstance();

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_PROTO_MESSAGE_TYPE_ADAPTER_H_

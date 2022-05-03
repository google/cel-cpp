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

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/memory_manager.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"

namespace google::api::expr::runtime {

class ProtoMessageTypeAdapter : public LegacyTypeAccessApis,
                                public LegacyTypeMutationApis {
 public:
  ProtoMessageTypeAdapter(const google::protobuf::Descriptor* descriptor,
                          google::protobuf::MessageFactory* message_factory)
      : message_factory_(message_factory), descriptor_(descriptor) {}

  ~ProtoMessageTypeAdapter() override = default;

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

  absl::StatusOr<CelValue> GetField(
      absl::string_view field_name, const CelValue::MessageWrapper& instance,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManager& memory_manager) const override;

  absl::StatusOr<bool> HasField(
      absl::string_view field_name,
      const CelValue::MessageWrapper& value) const override;

  bool IsEqualTo(const CelValue::MessageWrapper& instance,
                 const CelValue::MessageWrapper& other_instance) const override;

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

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
#include "eval/public/cel_value.h"
#include "eval/public/structs/legacy_type_adapter.h"

namespace google::api::expr::runtime {

class ProtoMessageTypeAdapter : public LegacyTypeAdapter::AccessApis,
                                public LegacyTypeAdapter::MutationApis {
 public:
  ProtoMessageTypeAdapter(const google::protobuf::Descriptor* descriptor,
                          google::protobuf::MessageFactory* message_factory)
      : message_factory_(message_factory), descriptor_(descriptor) {}

  ~ProtoMessageTypeAdapter() override = default;

  absl::StatusOr<CelValue> NewInstance(
      cel::MemoryManager& memory_manager) const override;

  bool DefinesField(absl::string_view field_name) const override;

  absl::Status SetField(absl::string_view field_name, const CelValue& value,

                        cel::MemoryManager& memory_manager,
                        CelValue& instance) const override;

  absl::Status AdaptFromWellKnownType(cel::MemoryManager& memory_manager,
                                      CelValue& instance) const override;

  absl::StatusOr<CelValue> GetField(
      absl::string_view field_name, const CelValue& instance,
      cel::MemoryManager& memory_manager) const override;

  absl::StatusOr<bool> HasField(absl::string_view field_name,
                                const CelValue& value) const override;

 private:
  // Helper for standardizing error messages for SetField operation.
  absl::Status ValidateSetFieldOp(bool assertion, absl::string_view field,
                                  absl::string_view detail) const;

  google::protobuf::MessageFactory* message_factory_;
  const google::protobuf::Descriptor* descriptor_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_PROTO_MESSAGE_TYPE_ADAPTER_H_

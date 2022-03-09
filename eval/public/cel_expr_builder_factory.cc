/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "eval/public/cel_expr_builder_factory.h"

#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "absl/status/status.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/cel_options.h"

namespace google::api::expr::runtime {

namespace {
template <class MessageType>
absl::Status ValidateStandardMessageType(
    const google::protobuf::DescriptorPool* descriptor_pool) {
  const google::protobuf::Descriptor* descriptor = MessageType::descriptor();
  const google::protobuf::Descriptor* descriptor_from_pool =
      descriptor_pool->FindMessageTypeByName(descriptor->full_name());
  if (descriptor_from_pool == nullptr) {
    return absl::NotFoundError(
        absl::StrFormat("Descriptor '%s' not found in descriptor pool",
                        descriptor->full_name()));
  }
  if (descriptor_from_pool == descriptor) {
    return absl::OkStatus();
  }
  google::protobuf::DescriptorProto descriptor_proto;
  google::protobuf::DescriptorProto descriptor_from_pool_proto;
  descriptor->CopyTo(&descriptor_proto);
  descriptor_from_pool->CopyTo(&descriptor_from_pool_proto);
  if (!google::protobuf::util::MessageDifferencer::Equals(descriptor_proto,
                                                descriptor_from_pool_proto)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "The descriptor for '%s' in the descriptor pool differs from the "
        "compiled-in generated version",
        descriptor->full_name()));
  }
  return absl::OkStatus();
}

template <class MessageType>
absl::Status AddOrValidateMessageType(google::protobuf::DescriptorPool* descriptor_pool) {
  const google::protobuf::Descriptor* descriptor = MessageType::descriptor();
  if (descriptor_pool->FindMessageTypeByName(descriptor->full_name()) !=
      nullptr) {
    return ValidateStandardMessageType<MessageType>(descriptor_pool);
  }
  google::protobuf::FileDescriptorProto file_descriptor_proto;
  descriptor->file()->CopyTo(&file_descriptor_proto);
  if (descriptor_pool->BuildFile(file_descriptor_proto) == nullptr) {
    return absl::InternalError(
        absl::StrFormat("Failed to add descriptor '%s' to descriptor pool",
                        descriptor->full_name()));
  }
  return absl::OkStatus();
}

absl::Status ValidateStandardMessageTypes(
    const google::protobuf::DescriptorPool* descriptor_pool) {
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::Any>(descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::BoolValue>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::BytesValue>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::DoubleValue>(
          descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::Duration>(descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::FloatValue>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::Int32Value>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::Int64Value>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::ListValue>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::StringValue>(
          descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::Struct>(descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::Timestamp>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::UInt32Value>(
          descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::UInt64Value>(
          descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::Value>(descriptor_pool));
  return absl::OkStatus();
}

}  // namespace

absl::Status AddStandardMessageTypesToDescriptorPool(
    google::protobuf::DescriptorPool* descriptor_pool) {
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Any>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::BoolValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::BytesValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::DoubleValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Duration>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::FloatValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Int32Value>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Int64Value>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::ListValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::StringValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Struct>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Timestamp>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::UInt32Value>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::UInt64Value>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Value>(descriptor_pool));
  return absl::OkStatus();
}

std::unique_ptr<CelExpressionBuilder> CreateCelExpressionBuilder(
    const google::protobuf::DescriptorPool* descriptor_pool,
    google::protobuf::MessageFactory* message_factory,
    const InterpreterOptions& options) {
  if (!ValidateStandardMessageTypes(descriptor_pool).ok()) {
    return nullptr;
  }
  auto builder =
      absl::make_unique<FlatExprBuilder>(descriptor_pool, message_factory);
  builder->set_shortcircuiting(options.short_circuiting);
  builder->set_constant_folding(options.constant_folding,
                                options.constant_arena);
  builder->set_enable_comprehension(options.enable_comprehension);
  builder->set_enable_comprehension_list_append(
      options.enable_comprehension_list_append);
  builder->set_comprehension_max_iterations(
      options.comprehension_max_iterations);
  builder->set_fail_on_warnings(options.fail_on_warnings);
  builder->set_enable_qualified_type_identifiers(
      options.enable_qualified_type_identifiers);
  builder->set_enable_comprehension_vulnerability_check(
      options.enable_comprehension_vulnerability_check);
  builder->set_enable_null_coercion(options.enable_null_to_message_coercion);
  builder->set_enable_wrapper_type_null_unboxing(
      options.enable_empty_wrapper_null_unboxing);
  builder->set_enable_heterogeneous_equality(
      options.enable_heterogeneous_equality);

  switch (options.unknown_processing) {
    case UnknownProcessingOptions::kAttributeAndFunction:
      builder->set_enable_unknown_function_results(true);
      builder->set_enable_unknowns(true);
      break;
    case UnknownProcessingOptions::kAttributeOnly:
      builder->set_enable_unknowns(true);
      break;
    case UnknownProcessingOptions::kDisabled:
      break;
  }

  builder->set_enable_missing_attribute_errors(
      options.enable_missing_attribute_errors);

  return builder;
}

}  // namespace google::api::expr::runtime

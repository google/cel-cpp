#include "eval/eval/create_struct_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/values/unknown_value.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Handle;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::interop_internal::CreateErrorValueFromView;
using ::cel::interop_internal::CreateLegacyMapValue;
using ::cel::interop_internal::LegacyValueToModernValueOrDie;

class CreateStructStepForMessage final : public ExpressionStepBase {
 public:
  struct FieldEntry {
    std::string field_name;
  };

  CreateStructStepForMessage(int64_t expr_id,
                             const LegacyTypeMutationApis* type_adapter,
                             std::vector<FieldEntry> entries)
      : ExpressionStepBase(expr_id),
        type_adapter_(type_adapter),
        entries_(std::move(entries)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Handle<Value>> DoEvaluate(ExecutionFrame* frame) const;

  const LegacyTypeMutationApis* type_adapter_;
  std::vector<FieldEntry> entries_;
};

class CreateStructStepForMap final : public ExpressionStepBase {
 public:
  CreateStructStepForMap(int64_t expr_id, size_t entry_count)
      : ExpressionStepBase(expr_id), entry_count_(entry_count) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Handle<Value>> DoEvaluate(ExecutionFrame* frame) const;

  size_t entry_count_;
};

absl::StatusOr<Handle<Value>> CreateStructStepForMessage::DoEvaluate(
    ExecutionFrame* frame) const {
  int entries_size = entries_.size();

  auto args = frame->value_stack().GetSpan(entries_size);

  if (frame->enable_unknowns()) {
    absl::optional<Handle<UnknownValue>> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(entries_size),
            /*use_partial=*/true);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  // TODO(uncreated-issue/32): switch to new cel::StructValue in phase 2
  CEL_ASSIGN_OR_RETURN(MessageWrapper::Builder instance,
                       type_adapter_->NewInstance(frame->memory_manager()));

  int index = 0;
  for (const auto& entry : entries_) {
    const CelValue& arg = cel::interop_internal::ModernValueToLegacyValueOrDie(
        frame->memory_manager(), args[index++]);

    CEL_RETURN_IF_ERROR(type_adapter_->SetField(
        entry.field_name, arg, frame->memory_manager(), instance));
  }

  CEL_ASSIGN_OR_RETURN(auto result, type_adapter_->AdaptFromWellKnownType(
                                        frame->memory_manager(), instance));
  return LegacyValueToModernValueOrDie(frame->memory_manager(), result);
}

absl::Status CreateStructStepForMessage::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < entries_.size()) {
    return absl::InternalError("CreateStructStepForMessage: stack underflow");
  }

  Handle<Value> result;
  auto status_or_result = DoEvaluate(frame);
  if (status_or_result.ok()) {
    result = std::move(status_or_result).value();
  } else {
    result = CreateErrorValueFromView(google::protobuf::Arena::Create<absl::Status>(
        cel::extensions::ProtoMemoryManager::CastToProtoArena(
            frame->memory_manager()),
        status_or_result.status()));
  }
  frame->value_stack().Pop(entries_.size());
  frame->value_stack().Push(std::move(result));

  return absl::OkStatus();
}

absl::StatusOr<Handle<Value>> CreateStructStepForMap::DoEvaluate(
    ExecutionFrame* frame) const {
  auto args = frame->value_stack().GetSpan(2 * entry_count_);

  if (frame->enable_unknowns()) {
    absl::optional<Handle<UnknownValue>> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(args.size()), true);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  // TODO(uncreated-issue/32): switch to new cel::MapValue in phase 2
  auto* map_builder = google::protobuf::Arena::Create<CelMapBuilder>(
      cel::extensions::ProtoMemoryManager::CastToProtoArena(
          frame->memory_manager()));

  for (size_t i = 0; i < entry_count_; i += 1) {
    int map_key_index = 2 * i;
    int map_value_index = map_key_index + 1;
    const CelValue& map_key =
        cel::interop_internal::ModernValueToLegacyValueOrDie(
            frame->memory_manager(), args[map_key_index]);
    CEL_RETURN_IF_ERROR(CelValue::CheckMapKeyType(map_key));
    auto key_status = map_builder->Add(
        map_key, cel::interop_internal::ModernValueToLegacyValueOrDie(
                     frame->memory_manager(), args[map_value_index]));
    if (!key_status.ok()) {
      return CreateErrorValueFromView(google::protobuf::Arena::Create<absl::Status>(
          cel::extensions::ProtoMemoryManager::CastToProtoArena(
              frame->memory_manager()),
          key_status));
    }
  }

  return CreateLegacyMapValue(map_builder);
}

absl::Status CreateStructStepForMap::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < 2 * entry_count_) {
    return absl::InternalError("CreateStructStepForMap: stack underflow");
  }

  CEL_ASSIGN_OR_RETURN(auto result, DoEvaluate(frame));

  frame->value_stack().Pop(2 * entry_count_);
  frame->value_stack().Push(std::move(result));

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStep(
    const cel::ast::internal::CreateStruct& create_struct_expr,
    const LegacyTypeMutationApis* type_adapter, int64_t expr_id) {
  if (type_adapter != nullptr) {
    std::vector<CreateStructStepForMessage::FieldEntry> entries;

    for (const auto& entry : create_struct_expr.entries()) {
      if (!type_adapter->DefinesField(entry.field_key())) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Invalid message creation: field '", entry.field_key(),
            "' not found in '", create_struct_expr.message_name(), "'"));
      }
      entries.push_back({entry.field_key()});
    }

    return std::make_unique<CreateStructStepForMessage>(expr_id, type_adapter,
                                                        std::move(entries));
  } else {
    // Make map-creating step.
    return std::make_unique<CreateStructStepForMap>(
        expr_id, create_struct_expr.entries().size());
  }
}

}  // namespace google::api::expr::runtime

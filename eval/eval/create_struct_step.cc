#include "eval/eval/create_struct_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

class CreateStructStepForMessage : public ExpressionStepBase {
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
  absl::Status DoEvaluate(ExecutionFrame* frame, CelValue* result) const;

  const LegacyTypeMutationApis* type_adapter_;
  std::vector<FieldEntry> entries_;
};

class CreateStructStepForMap : public ExpressionStepBase {
 public:
  CreateStructStepForMap(int64_t expr_id, size_t entry_count)
      : ExpressionStepBase(expr_id), entry_count_(entry_count) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status DoEvaluate(ExecutionFrame* frame, CelValue* result) const;

  size_t entry_count_;
};

absl::Status CreateStructStepForMessage::DoEvaluate(ExecutionFrame* frame,
                                                    CelValue* result) const {
  int entries_size = entries_.size();

  absl::Span<const CelValue> args = frame->value_stack().GetSpan(entries_size);

  if (frame->enable_unknowns()) {
    auto unknown_set = frame->attribute_utility().MergeUnknowns(
        args, frame->value_stack().GetAttributeSpan(entries_size),
        /*initial_set=*/nullptr,
        /*use_partial=*/true);
    if (unknown_set != nullptr) {
      *result = CelValue::CreateUnknownSet(unknown_set);
      return absl::OkStatus();
    }
  }

  CEL_ASSIGN_OR_RETURN(MessageWrapper::Builder instance,
                       type_adapter_->NewInstance(frame->memory_manager()));

  int index = 0;
  for (const auto& entry : entries_) {
    const CelValue& arg = args[index++];

    CEL_RETURN_IF_ERROR(type_adapter_->SetField(
        entry.field_name, arg, frame->memory_manager(), instance));
  }

  CEL_ASSIGN_OR_RETURN(*result, type_adapter_->AdaptFromWellKnownType(
                                    frame->memory_manager(), instance));

  return absl::OkStatus();
}

absl::Status CreateStructStepForMessage::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < entries_.size()) {
    return absl::InternalError("CreateStructStepForMessage: stack underflow");
  }

  CelValue result;
  absl::Status status = DoEvaluate(frame, &result);
  if (!status.ok()) {
    result = CreateErrorValue(frame->memory_manager(), status);
  }
  frame->value_stack().Pop(entries_.size());
  frame->value_stack().Push(result);

  return absl::OkStatus();
}

absl::Status CreateStructStepForMap::DoEvaluate(ExecutionFrame* frame,
                                                CelValue* result) const {
  absl::Span<const CelValue> args =
      frame->value_stack().GetSpan(2 * entry_count_);

  if (frame->enable_unknowns()) {
    const UnknownSet* unknown_set = frame->attribute_utility().MergeUnknowns(
        args, frame->value_stack().GetAttributeSpan(args.size()),
        /*initial_set=*/nullptr, true);
    if (unknown_set != nullptr) {
      *result = CelValue::CreateUnknownSet(unknown_set);
      return absl::OkStatus();
    }
  }

  std::vector<std::pair<CelValue, CelValue>> map_entries;
  auto map_builder = frame->memory_manager().New<CelMapBuilder>();

  for (size_t i = 0; i < entry_count_; i += 1) {
    int map_key_index = 2 * i;
    int map_value_index = map_key_index + 1;
    const CelValue& map_key = args[map_key_index];
    CEL_RETURN_IF_ERROR(CelValue::CheckMapKeyType(map_key));
    auto key_status = map_builder->Add(map_key, args[map_value_index]);
    if (!key_status.ok()) {
      *result = CreateErrorValue(frame->memory_manager(), key_status);
      return absl::OkStatus();
    }
  }

  *result = CelValue::CreateMap(map_builder.release());

  return absl::OkStatus();
}

absl::Status CreateStructStepForMap::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < 2 * entry_count_) {
    return absl::InternalError("CreateStructStepForMap: stack underflow");
  }

  CelValue result;
  CEL_RETURN_IF_ERROR(DoEvaluate(frame, &result));

  frame->value_stack().Pop(2 * entry_count_);
  frame->value_stack().Push(result);

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

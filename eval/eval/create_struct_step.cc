#include "eval/eval/create_struct_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/value_manager.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/overflow.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValue;
using ::cel::BytesValue;
using ::cel::DoubleValue;
using ::cel::Handle;
using ::cel::IntValue;
using ::cel::ListValue;
using ::cel::MapValue;
using ::cel::StringValue;
using ::cel::StructType;
using ::cel::StructValue;
using ::cel::StructValueBuilderInterface;
using ::cel::Type;
using ::cel::TypeManager;
using ::cel::UintValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueFactory;

// `CreateStruct` implementation for message/struct.
class CreateStructStepForStruct final : public ExpressionStepBase {
 public:
  CreateStructStepForStruct(int64_t expr_id, std::string name,
                            std::vector<std::string> entries)
      : ExpressionStepBase(expr_id),
        name_(std::move(name)),
        entries_(std::move(entries)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Value> DoEvaluate(ExecutionFrame* frame) const;

  std::string name_;
  std::vector<std::string> entries_;
};

// `CreateStruct` implementation for map.
class CreateStructStepForMap final : public ExpressionStepBase {
 public:
  CreateStructStepForMap(int64_t expr_id, size_t entry_count)
      : ExpressionStepBase(expr_id), entry_count_(entry_count) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Value> DoEvaluate(ExecutionFrame* frame) const;

  size_t entry_count_;
};

absl::StatusOr<Value> CreateStructStepForStruct::DoEvaluate(
    ExecutionFrame* frame) const {
  int entries_size = entries_.size();

  auto args = frame->value_stack().GetSpan(entries_size);

  if (frame->enable_unknowns()) {
    absl::optional<UnknownValue> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(entries_size),
            /*use_partial=*/true);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  auto builder_or_status = frame->value_manager().NewValueBuilder(name_);
  if (!builder_or_status.ok()) {
    return builder_or_status.status();
  }
  auto maybe_builder = std::move(*builder_or_status);
  if (!maybe_builder.has_value()) {
    return absl::NotFoundError(absl::StrCat("Unable to find builder: ", name_));
  }
  auto builder = std::move(*maybe_builder);

  int index = 0;
  for (const auto& entry : entries_) {
    CEL_RETURN_IF_ERROR(
        builder->SetFieldByName(entry, std::move(args[index++])));
  }
  return std::move(*builder).Build();
}

absl::Status CreateStructStepForStruct::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < entries_.size()) {
    return absl::InternalError("CreateStructStepForStruct: stack underflow");
  }

  Value result;
  auto status_or_result = DoEvaluate(frame);
  if (status_or_result.ok()) {
    result = std::move(status_or_result).value();
  } else {
    result = frame->value_factory().CreateErrorValue(status_or_result.status());
  }
  frame->value_stack().PopAndPush(entries_.size(), std::move(result));

  return absl::OkStatus();
}

absl::StatusOr<Value> CreateStructStepForMap::DoEvaluate(
    ExecutionFrame* frame) const {
  auto args = frame->value_stack().GetSpan(2 * entry_count_);

  if (frame->enable_unknowns()) {
    absl::optional<UnknownValue> unknown_set =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            args, frame->value_stack().GetAttributeSpan(args.size()), true);
    if (unknown_set.has_value()) {
      return *unknown_set;
    }
  }

  CEL_ASSIGN_OR_RETURN(auto builder, frame->value_manager().NewMapValueBuilder(
                                         cel::MapTypeView{}));
  builder->Reserve(entry_count_);

  for (size_t i = 0; i < entry_count_; i += 1) {
    int map_key_index = 2 * i;
    int map_value_index = map_key_index + 1;
    CEL_RETURN_IF_ERROR(cel::CheckMapKey(args[map_key_index]));
    auto key_status = builder->Put(std::move(args[map_key_index]),
                                   std::move(args[map_value_index]));
    if (!key_status.ok()) {
      return frame->value_factory().CreateErrorValue(key_status);
    }
  }

  return std::move(*builder).Build();
}

absl::Status CreateStructStepForMap::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < 2 * entry_count_) {
    return absl::InternalError("CreateStructStepForMap: stack underflow");
  }

  CEL_ASSIGN_OR_RETURN(auto result, DoEvaluate(frame));

  frame->value_stack().PopAndPush(2 * entry_count_, std::move(result));

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStepForStruct(
    const cel::ast_internal::CreateStruct& create_struct_expr, std::string name,
    int64_t expr_id, cel::TypeManager& type_manager) {
  // We resolved to a struct type. Use it.
  std::vector<std::string> entries;
  entries.reserve(create_struct_expr.entries().size());
  for (const auto& entry : create_struct_expr.entries()) {
    CEL_ASSIGN_OR_RETURN(auto field, type_manager.FindStructTypeFieldByName(
                                         name, entry.field_key()));
    if (!field.has_value()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid message creation: field '", entry.field_key(),
          "' not found in '", create_struct_expr.message_name(), "'"));
    }
    entries.push_back(entry.field_key());
  }
  return std::make_unique<CreateStructStepForStruct>(expr_id, std::move(name),
                                                     std::move(entries));
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStepForMap(
    const cel::ast_internal::CreateStruct& create_struct_expr,
    int64_t expr_id) {
  // Make map-creating step.
  return std::make_unique<CreateStructStepForMap>(
      expr_id, create_struct_expr.entries().size());
}

}  // namespace google::api::expr::runtime

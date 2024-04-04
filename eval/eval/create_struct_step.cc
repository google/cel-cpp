// Copyright 2017 Google LLC
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

#include "eval/eval/create_struct_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::StructValueBuilderInterface;
using ::cel::UnknownValue;
using ::cel::Value;

absl::flat_hash_set<int32_t> MakeOptionalIndicesSet(
    const cel::ast_internal::CreateStruct& create_struct_expr) {
  absl::flat_hash_set<int32_t> optional_indices;
  for (size_t i = 0; i < create_struct_expr.entries().size(); ++i) {
    if (create_struct_expr.entries()[i].optional_entry()) {
      optional_indices.insert(static_cast<int32_t>(i));
    }
  }
  return optional_indices;
}

// `CreateStruct` implementation for message/struct.
class CreateStructStepForStruct final : public ExpressionStepBase {
 public:
  CreateStructStepForStruct(int64_t expr_id, std::string name,
                            std::vector<std::string> entries,
                            absl::flat_hash_set<int32_t> optional_indices)
      : ExpressionStepBase(expr_id),
        name_(std::move(name)),
        entries_(std::move(entries)),
        optional_indices_(std::move(optional_indices)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Value> DoEvaluate(ExecutionFrame* frame) const;

  std::string name_;
  std::vector<std::string> entries_;
  absl::flat_hash_set<int32_t> optional_indices_;
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

  for (int i = 0; i < entries_size; ++i) {
    const auto& entry = entries_[i];
    auto& arg = args[i];
    if (optional_indices_.contains(static_cast<int32_t>(i))) {
      if (auto optional_arg = cel::As<cel::OptionalValue>(arg); optional_arg) {
        if (!optional_arg->HasValue()) {
          continue;
        }
        CEL_RETURN_IF_ERROR(
            builder->SetFieldByName(entry, optional_arg->Value()));
      }
    } else {
      CEL_RETURN_IF_ERROR(builder->SetFieldByName(entry, std::move(arg)));
    }
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
  return std::make_unique<CreateStructStepForStruct>(
      expr_id, std::move(name), std::move(entries),
      MakeOptionalIndicesSet(create_struct_expr));
}
}  // namespace google::api::expr::runtime

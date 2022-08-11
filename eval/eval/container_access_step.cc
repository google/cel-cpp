#include "eval/eval/container_access_step.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/memory_manager.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_number.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"

namespace google::api::expr::runtime {

namespace {

inline constexpr int kNumContainerAccessArguments = 2;

// ContainerAccessStep performs message field access specified by Expr::Select
// message.
class ContainerAccessStep : public ExpressionStepBase {
 public:
  explicit ContainerAccessStep(int64_t expr_id) : ExpressionStepBase(expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  using ValueAttributePair = std::pair<CelValue, AttributeTrail>;

  ValueAttributePair PerformLookup(ExecutionFrame* frame) const;
  CelValue LookupInMap(const CelMap* cel_map, const CelValue& key,
                       ExecutionFrame* frame) const;
  CelValue LookupInList(const CelList* cel_list, const CelValue& key,
                        ExecutionFrame* frame) const;
};

inline CelValue ContainerAccessStep::LookupInMap(const CelMap* cel_map,
                                                 const CelValue& key,
                                                 ExecutionFrame* frame) const {
  if (frame->enable_heterogeneous_numeric_lookups()) {
    // Double isn't a supported key type but may be convertible to an integer.
    absl::optional<CelNumber> number = GetNumberFromCelValue(key);
    if (number.has_value()) {
      // consider uint as uint first then try coercion.
      if (key.IsUint64()) {
        absl::optional<CelValue> maybe_value = (*cel_map)[key];
        if (maybe_value.has_value()) {
          return *maybe_value;
        }
      }
      if (number->LosslessConvertibleToInt()) {
        absl::optional<CelValue> maybe_value =
            (*cel_map)[CelValue::CreateInt64(number->AsInt())];
        if (maybe_value.has_value()) {
          return *maybe_value;
        }
      }
      if (number->LosslessConvertibleToUint()) {
        absl::optional<CelValue> maybe_value =
            (*cel_map)[CelValue::CreateUint64(number->AsUint())];
        if (maybe_value.has_value()) {
          return *maybe_value;
        }
      }
      return CreateNoSuchKeyError(frame->memory_manager(), key.DebugString());
    }
  }

  absl::Status status = CelValue::CheckMapKeyType(key);
  if (!status.ok()) {
    return CreateErrorValue(frame->memory_manager(), status);
  }
  absl::optional<CelValue> maybe_value = (*cel_map)[key];
  if (maybe_value.has_value()) {
    return maybe_value.value();
  }

  return CreateNoSuchKeyError(frame->memory_manager(), key.DebugString());
}

inline CelValue ContainerAccessStep::LookupInList(const CelList* cel_list,
                                                  const CelValue& key,
                                                  ExecutionFrame* frame) const {
  absl::optional<int64_t> maybe_idx;
  if (frame->enable_heterogeneous_numeric_lookups()) {
    auto number = GetNumberFromCelValue(key);
    if (number.has_value() && number->LosslessConvertibleToInt()) {
      maybe_idx = number->AsInt();
    }
  } else if (int64_t held_int; key.GetValue(&held_int)) {
    maybe_idx = held_int;
  }

  if (maybe_idx.has_value()) {
    int64_t idx = *maybe_idx;
    if (idx < 0 || idx >= cel_list->size()) {
      return CreateErrorValue(
          frame->memory_manager(),
          absl::StrCat("Index error: index=", idx, " size=", cel_list->size()));
    }
    return (*cel_list)[idx];
  }

  return CreateErrorValue(
      frame->memory_manager(),
      absl::StrCat("Index error: expected integer type, got ",
                   CelValue::TypeName(key.type())));
}

ContainerAccessStep::ValueAttributePair ContainerAccessStep::PerformLookup(
    ExecutionFrame* frame) const {
  auto input_args = frame->value_stack().GetSpan(kNumContainerAccessArguments);
  AttributeTrail trail;

  const CelValue& container = input_args[0];
  const CelValue& key = input_args[1];

  if (frame->enable_unknowns()) {
    auto unknown_set =
        frame->attribute_utility().MergeUnknowns(input_args, nullptr);

    if (unknown_set) {
      return {CelValue::CreateUnknownSet(unknown_set), trail};
    }

    // We guarantee that GetAttributeSpan can aquire this number of arguments
    // by calling HasEnough() at the beginning of Execute() method.
    auto input_attrs =
        frame->value_stack().GetAttributeSpan(kNumContainerAccessArguments);
    auto container_trail = input_attrs[0];
    trail = container_trail.Step(CelAttributeQualifier::Create(key),
                                 frame->memory_manager());

    if (frame->attribute_utility().CheckForUnknown(trail,
                                                   /*use_partial=*/false)) {
      auto unknown_set =
          frame->attribute_utility().CreateUnknownSet(trail.attribute());

      return {CelValue::CreateUnknownSet(unknown_set), trail};
    }
  }

  for (const auto& value : input_args) {
    if (value.IsError()) {
      return {value, trail};
    }
  }

  // Select steps can be applied to either maps or messages
  switch (container.type()) {
    case CelValue::Type::kMap: {
      const CelMap* cel_map = container.MapOrDie();
      return {LookupInMap(cel_map, key, frame), trail};
    }
    case CelValue::Type::kList: {
      const CelList* cel_list = container.ListOrDie();
      return {LookupInList(cel_list, key, frame), trail};
    }
    default: {
      auto error =
          CreateErrorValue(frame->memory_manager(),
                           absl::InvalidArgumentError(absl::StrCat(
                               "Invalid container type: '",
                               CelValue::TypeName(container.type()), "'")));
      return {error, trail};
    }
  }
}

absl::Status ContainerAccessStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(kNumContainerAccessArguments)) {
    return absl::Status(
        absl::StatusCode::kInternal,
        "Insufficient arguments supplied for ContainerAccess-type expression");
  }

  auto result = PerformLookup(frame);
  frame->value_stack().Pop(kNumContainerAccessArguments);
  frame->value_stack().Push(result.first, result.second);

  return absl::OkStatus();
}
}  // namespace

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateContainerAccessStep(
    const cel::ast::internal::Call& call, int64_t expr_id) {
  int arg_count = call.args().size() + (call.has_target() ? 1 : 0);
  if (arg_count != kNumContainerAccessArguments) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid argument count for index operation: ", arg_count));
  }
  return absl::make_unique<ContainerAccessStep>(expr_id);
}

}  // namespace google::api::expr::runtime

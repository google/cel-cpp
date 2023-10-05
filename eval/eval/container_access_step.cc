#include "eval/eval/container_access_step.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "base/kind.h"
#include "base/value.h"
#include "base/values/bool_value.h"
#include "base/values/double_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/string_value.h"
#include "base/values/uint_value.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/number.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::AttributeQualifier;
using ::cel::BoolValue;
using ::cel::DoubleValue;
using ::cel::Handle;
using ::cel::IntValue;
using ::cel::ListValue;
using ::cel::MapValue;
using ::cel::StringValue;
using ::cel::UintValue;
using ::cel::Value;
using ::cel::ValueKind;
using ::cel::ValueKindToString;
using ::cel::internal::Number;
using ::cel::runtime_internal::CreateNoSuchKeyError;

inline constexpr int kNumContainerAccessArguments = 2;

// ContainerAccessStep performs message field access specified by Expr::Select
// message.
class ContainerAccessStep : public ExpressionStepBase {
 public:
  explicit ContainerAccessStep(int64_t expr_id) : ExpressionStepBase(expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  struct LookupResult {
    Handle<Value> value;
    AttributeTrail trail;
  };

  LookupResult PerformLookup(ExecutionFrame* frame) const;
  absl::StatusOr<Handle<Value>> LookupInMap(const Handle<MapValue>& cel_map,
                                            const Handle<Value>& key,
                                            ExecutionFrame* frame) const;
  absl::StatusOr<Handle<Value>> LookupInList(const Handle<ListValue>& cel_list,
                                             const Handle<Value>& key,
                                             ExecutionFrame* frame) const;
};

absl::optional<Number> CelNumberFromValue(const Handle<Value>& value) {
  switch (value->kind()) {
    case ValueKind::kInt64:
      return Number::FromInt64(value.As<IntValue>()->value());
    case ValueKind::kUint64:
      return Number::FromUint64(value.As<UintValue>()->value());
    case ValueKind::kDouble:
      return Number::FromDouble(value.As<DoubleValue>()->value());
    default:
      return absl::nullopt;
  }
}

absl::Status CheckMapKeyType(const Handle<Value>& key) {
  ValueKind kind = key->kind();
  switch (kind) {
    case ValueKind::kString:
    case ValueKind::kInt64:
    case ValueKind::kUint64:
    case ValueKind::kBool:
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Invalid map key type: '", ValueKindToString(kind), "'"));
  }
}

AttributeQualifier AttributeQualifierFromValue(const Handle<Value>& v) {
  switch (v->kind()) {
    case ValueKind::kString:
      return AttributeQualifier::OfString(v.As<StringValue>()->ToString());
    case ValueKind::kInt64:
      return AttributeQualifier::OfInt(v.As<IntValue>()->value());
    case ValueKind::kUint64:
      return AttributeQualifier::OfUint(v.As<UintValue>()->value());
    case ValueKind::kBool:
      return AttributeQualifier::OfBool(v.As<BoolValue>()->value());
    default:
      // Non-matching qualifier.
      return AttributeQualifier();
  }
}

absl::StatusOr<Handle<Value>> ContainerAccessStep::LookupInMap(
    const Handle<MapValue>& cel_map, const Handle<Value>& key,
    ExecutionFrame* frame) const {
  if (frame->enable_heterogeneous_numeric_lookups()) {
    // Double isn't a supported key type but may be convertible to an integer.
    absl::optional<Number> number = CelNumberFromValue(key);
    if (number.has_value()) {
      // Consider uint as uint first then try coercion (prefer matching the
      // original type of the key value).
      if (key->Is<UintValue>()) {
        Handle<Value> value;
        bool ok;
        CEL_ASSIGN_OR_RETURN(std::tie(value, ok),
                             cel_map->Find(frame->value_factory(), key));
        if (ok) {
          return value;
        }
      }
      // double / int / uint -> int
      if (number->LosslessConvertibleToInt()) {
        Handle<Value> value;
        bool ok;
        CEL_ASSIGN_OR_RETURN(
            std::tie(value, ok),
            cel_map->Find(
                frame->value_factory(),
                frame->value_factory().CreateIntValue(number->AsInt())));
        if (ok) {
          return value;
        }
      }
      // double / int -> uint
      if (number->LosslessConvertibleToUint()) {
        Handle<Value> value;
        bool ok;
        CEL_ASSIGN_OR_RETURN(
            std::tie(value, ok),
            cel_map->Find(
                frame->value_factory(),
                frame->value_factory().CreateUintValue(number->AsUint())));
        if (ok) {
          return value;
        }
      }
      return frame->value_factory().CreateErrorValue(
          CreateNoSuchKeyError(key->DebugString()));
    }
  }

  CEL_RETURN_IF_ERROR(CheckMapKeyType(key));

  return cel_map->Get(frame->value_factory(), key);
}

absl::StatusOr<Handle<Value>> ContainerAccessStep::LookupInList(
    const Handle<ListValue>& cel_list, const Handle<Value>& key,
    ExecutionFrame* frame) const {
  absl::optional<int64_t> maybe_idx;
  if (frame->enable_heterogeneous_numeric_lookups()) {
    auto number = CelNumberFromValue(key);
    if (number.has_value() && number->LosslessConvertibleToInt()) {
      maybe_idx = number->AsInt();
    }
  } else if (key->Is<IntValue>()) {
    maybe_idx = key.As<IntValue>()->value();
  }

  if (maybe_idx.has_value()) {
    int64_t idx = *maybe_idx;
    if (idx < 0 || idx >= cel_list->size()) {
      return absl::UnknownError(
          absl::StrCat("Index error: index=", idx, " size=", cel_list->size()));
    }
    return cel_list->Get(frame->value_factory(), idx);
  }

  return absl::UnknownError(
      absl::StrCat("Index error: expected integer type, got ",
                   cel::KindToString(ValueKindToKind(key->kind()))));
}

ContainerAccessStep::LookupResult ContainerAccessStep::PerformLookup(
    ExecutionFrame* frame) const {
  auto input_args = frame->value_stack().GetSpan(kNumContainerAccessArguments);
  AttributeTrail trail;

  const Handle<Value> container = input_args[0];
  const Handle<Value> key = input_args[1];

  if (frame->enable_unknowns()) {
    auto unknown_set = frame->attribute_utility().MergeUnknowns(input_args);

    if (unknown_set) {
      return {std::move(unknown_set).value(), std::move(trail)};
    }

    // We guarantee that GetAttributeSpan can acquire this number of arguments
    // by calling HasEnough() at the beginning of Execute() method.
    absl::Span<const AttributeTrail> input_attrs =
        frame->value_stack().GetAttributeSpan(kNumContainerAccessArguments);
    const auto& container_trail = input_attrs[0];
    trail = container_trail.Step(AttributeQualifierFromValue(key));

    if (frame->attribute_utility().CheckForUnknown(trail,
                                                   /*use_partial=*/false)) {
      cel::Attribute attr = trail.attribute();
      return {frame->attribute_utility().CreateUnknownSet(attr),
              std::move(trail)};
    }
  }

  for (const auto& value : input_args) {
    if (value->Is<cel::ErrorValue>()) {
      return {value, std::move(trail)};
    }
  }

  // Select steps can be applied to either maps or messages
  switch (container->kind()) {
    case ValueKind::kMap: {
      auto result = LookupInMap(container.As<MapValue>(), key, frame);
      if (!result.ok()) {
        return {
            frame->value_factory().CreateErrorValue(std::move(result).status()),
            std::move(trail)};
      }
      return {std::move(result).value(), std::move(trail)};
    }
    case ValueKind::kList: {
      auto result = LookupInList(container.As<ListValue>(), key, frame);
      if (!result.ok()) {
        return {
            frame->value_factory().CreateErrorValue(std::move(result).status()),
            std::move(trail)};
      }
      return {std::move(result).value(), std::move(trail)};
    }
    default:
      return {
          frame->value_factory().CreateErrorValue(absl::InvalidArgumentError(
              absl::StrCat("Invalid container type: '",
                           ValueKindToString(container->kind()), "'"))),
          std::move(trail)};
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
  frame->value_stack().Push(std::move(result.value), std::move(result.trail));

  return absl::OkStatus();
}
}  // namespace

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateContainerAccessStep(
    const cel::ast_internal::Call& call, int64_t expr_id) {
  int arg_count = call.args().size() + (call.has_target() ? 1 : 0);
  if (arg_count != kNumContainerAccessArguments) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid argument count for index operation: ", arg_count));
  }
  return std::make_unique<ContainerAccessStep>(expr_id);
}

}  // namespace google::api::expr::runtime

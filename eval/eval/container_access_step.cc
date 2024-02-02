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
#include "common/value.h"
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
using ::cel::ValueView;
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
    ValueView value;
    AttributeTrail trail;
  };

  LookupResult PerformLookup(ExecutionFrame* frame, Value& scratch) const;
  absl::StatusOr<ValueView> LookupInMap(const MapValue& cel_map,
                                        const Value& key, ExecutionFrame* frame,
                                        Value& scratch) const;
  absl::StatusOr<ValueView> LookupInList(const ListValue& cel_list,
                                         const Value& key,
                                         ExecutionFrame* frame,
                                         Value& scratch) const;
};

absl::optional<Number> CelNumberFromValue(const Value& value) {
  switch (value->kind()) {
    case ValueKind::kInt64:
      return Number::FromInt64(value.As<IntValue>().NativeValue());
    case ValueKind::kUint64:
      return Number::FromUint64(value.As<UintValue>().NativeValue());
    case ValueKind::kDouble:
      return Number::FromDouble(value.As<DoubleValue>().NativeValue());
    default:
      return absl::nullopt;
  }
}

absl::Status CheckMapKeyType(const Value& key) {
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

AttributeQualifier AttributeQualifierFromValue(const Value& v) {
  switch (v->kind()) {
    case ValueKind::kString:
      return AttributeQualifier::OfString(v.As<StringValue>().ToString());
    case ValueKind::kInt64:
      return AttributeQualifier::OfInt(v.As<IntValue>().NativeValue());
    case ValueKind::kUint64:
      return AttributeQualifier::OfUint(v.As<UintValue>().NativeValue());
    case ValueKind::kBool:
      return AttributeQualifier::OfBool(v.As<BoolValue>().NativeValue());
    default:
      // Non-matching qualifier.
      return AttributeQualifier();
  }
}

absl::StatusOr<ValueView> ContainerAccessStep::LookupInMap(
    const MapValue& cel_map, const Value& key, ExecutionFrame* frame,
    Value& scratch) const {
  if (frame->enable_heterogeneous_numeric_lookups()) {
    // Double isn't a supported key type but may be convertible to an integer.
    absl::optional<Number> number = CelNumberFromValue(key);
    if (number.has_value()) {
      // Consider uint as uint first then try coercion (prefer matching the
      // original type of the key value).
      if (key->Is<UintValue>()) {
        ValueView value;
        bool ok;
        CEL_ASSIGN_OR_RETURN(
            std::tie(value, ok),
            cel_map.Find(frame->value_factory(), key, scratch));
        if (ok) {
          return value;
        }
      }
      // double / int / uint -> int
      if (number->LosslessConvertibleToInt()) {
        ValueView value;
        bool ok;
        CEL_ASSIGN_OR_RETURN(
            std::tie(value, ok),
            cel_map.Find(frame->value_factory(),
                         frame->value_factory().CreateIntValue(number->AsInt()),
                         scratch));
        if (ok) {
          return value;
        }
      }
      // double / int -> uint
      if (number->LosslessConvertibleToUint()) {
        ValueView value;
        bool ok;
        CEL_ASSIGN_OR_RETURN(
            std::tie(value, ok),
            cel_map.Find(
                frame->value_factory(),
                frame->value_factory().CreateUintValue(number->AsUint()),
                scratch));
        if (ok) {
          return value;
        }
      }
      scratch = frame->value_factory().CreateErrorValue(
          CreateNoSuchKeyError(key->DebugString()));
      return scratch;
    }
  }

  CEL_RETURN_IF_ERROR(CheckMapKeyType(key));

  return cel_map.Get(frame->value_factory(), key, scratch);
}

absl::StatusOr<ValueView> ContainerAccessStep::LookupInList(
    const ListValue& cel_list, const Value& key, ExecutionFrame* frame,
    Value& scratch) const {
  absl::optional<int64_t> maybe_idx;
  if (frame->enable_heterogeneous_numeric_lookups()) {
    auto number = CelNumberFromValue(key);
    if (number.has_value() && number->LosslessConvertibleToInt()) {
      maybe_idx = number->AsInt();
    }
  } else if (key->Is<IntValue>()) {
    maybe_idx = key.As<IntValue>().NativeValue();
  }

  if (maybe_idx.has_value()) {
    int64_t idx = *maybe_idx;
    if (idx < 0 || idx >= cel_list.Size()) {
      return absl::UnknownError(
          absl::StrCat("Index error: index=", idx, " size=", cel_list.Size()));
    }
    return cel_list.Get(frame->value_factory(), idx, scratch);
  }

  return absl::UnknownError(
      absl::StrCat("Index error: expected integer type, got ",
                   cel::KindToString(ValueKindToKind(key->kind()))));
}

ContainerAccessStep::LookupResult ContainerAccessStep::PerformLookup(
    ExecutionFrame* frame, Value& scratch) const {
  auto input_args = frame->value_stack().GetSpan(kNumContainerAccessArguments);
  AttributeTrail trail;

  const Value container = input_args[0];
  const Value key = input_args[1];

  if (frame->enable_unknowns()) {
    auto unknown_set = frame->attribute_utility().MergeUnknowns(input_args);

    if (unknown_set) {
      scratch = std::move(unknown_set).value();
      return {ValueView{scratch}, std::move(trail)};
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
      scratch = frame->attribute_utility().CreateUnknownSet(attr);
      return {ValueView{scratch}, std::move(trail)};
    }
  }

  for (const auto& value : input_args) {
    if (value->Is<cel::ErrorValue>()) {
      return {ValueView{value}, std::move(trail)};
    }
  }

  // Select steps can be applied to either maps or messages
  switch (container->kind()) {
    case ValueKind::kMap: {
      auto result = LookupInMap(container.As<MapValue>(), key, frame, scratch);
      if (!result.ok()) {
        scratch =
            frame->value_factory().CreateErrorValue(std::move(result).status());
        return {ValueView{scratch}, std::move(trail)};
      }
      return {*result, std::move(trail)};
    }
    case ValueKind::kList: {
      auto result =
          LookupInList(container.As<ListValue>(), key, frame, scratch);
      if (!result.ok()) {
        scratch =
            frame->value_factory().CreateErrorValue(std::move(result).status());
        return {ValueView{scratch}, std::move(trail)};
      }
      return {*result, std::move(trail)};
    }
    default:
      scratch =
          frame->value_factory().CreateErrorValue(absl::InvalidArgumentError(
              absl::StrCat("Invalid container type: '",
                           ValueKindToString(container->kind()), "'")));
      return {ValueView{scratch}, std::move(trail)};
  }
}

absl::Status ContainerAccessStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(kNumContainerAccessArguments)) {
    return absl::Status(
        absl::StatusCode::kInternal,
        "Insufficient arguments supplied for ContainerAccess-type expression");
  }

  Value scratch;
  auto result = PerformLookup(frame, scratch);
  frame->value_stack().PopAndPush(kNumContainerAccessArguments,
                                  Value{result.value}, std::move(result.trail));

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

#include "eval/eval/container_access_step.h"

#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/ast_internal/expr.h"
#include "base/attribute.h"
#include "base/kind.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/attribute_utility.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/casts.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "runtime/internal/errors.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::AttributeQualifier;
using ::cel::BoolValue;
using ::cel::Cast;
using ::cel::DoubleValue;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
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

ValueView LookupInMap(const MapValue& cel_map, const Value& key,
                      ExecutionFrameBase& frame, Value& scratch) {
  if (frame.options().enable_heterogeneous_equality) {
    // Double isn't a supported key type but may be convertible to an integer.
    absl::optional<Number> number = CelNumberFromValue(key);
    if (number.has_value()) {
      // Consider uint as uint first then try coercion (prefer matching the
      // original type of the key value).
      if (key->Is<UintValue>()) {
        auto lookup = cel_map.Find(frame.value_manager(), key, scratch);
        if (!lookup.ok()) {
          scratch = frame.value_manager().CreateErrorValue(
              std::move(lookup).status());
          return ValueView{scratch};
        }
        if (lookup->second) {
          return lookup->first;
        }
      }
      // double / int / uint -> int
      if (number->LosslessConvertibleToInt()) {
        auto lookup = cel_map.Find(
            frame.value_manager(),
            frame.value_manager().CreateIntValue(number->AsInt()), scratch);
        if (!lookup.ok()) {
          scratch = frame.value_manager().CreateErrorValue(
              std::move(lookup).status());
          return ValueView{scratch};
        }
        if (lookup->second) {
          return lookup->first;
        }
      }
      // double / int -> uint
      if (number->LosslessConvertibleToUint()) {
        auto lookup = cel_map.Find(
            frame.value_manager(),
            frame.value_manager().CreateUintValue(number->AsUint()), scratch);
        if (!lookup.ok()) {
          scratch = frame.value_manager().CreateErrorValue(
              std::move(lookup).status());
          return ValueView{scratch};
        }
        if (lookup->second) {
          return lookup->first;
        }
      }
      scratch = frame.value_manager().CreateErrorValue(
          CreateNoSuchKeyError(key->DebugString()));
      return ValueView{scratch};
    }
  }

  absl::Status status = CheckMapKeyType(key);
  if (!status.ok()) {
    scratch = frame.value_manager().CreateErrorValue(std::move(status));
    return ValueView{scratch};
  }

  absl::StatusOr<ValueView> lookup =
      cel_map.Get(frame.value_manager(), key, scratch);
  if (!lookup.ok()) {
    scratch =
        frame.value_manager().CreateErrorValue(std::move(lookup).status());
    return ValueView{scratch};
  }
  return *lookup;
}

ValueView LookupInList(const ListValue& cel_list, const Value& key,
                       ExecutionFrameBase& frame, Value& scratch) {
  absl::optional<int64_t> maybe_idx;
  if (frame.options().enable_heterogeneous_equality) {
    auto number = CelNumberFromValue(key);
    if (number.has_value() && number->LosslessConvertibleToInt()) {
      maybe_idx = number->AsInt();
    }
  } else if (InstanceOf<IntValue>(key)) {
    maybe_idx = key.As<IntValue>().NativeValue();
  }

  if (!maybe_idx.has_value()) {
    scratch = frame.value_manager().CreateErrorValue(absl::UnknownError(
        absl::StrCat("Index error: expected integer type, got ",
                     cel::KindToString(ValueKindToKind(key->kind())))));
    return ValueView{scratch};
  }

  int64_t idx = *maybe_idx;
  if (idx < 0 || idx >= cel_list.Size()) {
    scratch = frame.value_manager().CreateErrorValue(absl::UnknownError(
        absl::StrCat("Index error: index=", idx, " size=", cel_list.Size())));
    return ValueView{scratch};
  }

  absl::StatusOr<ValueView> lookup =
      cel_list.Get(frame.value_manager(), idx, scratch);

  if (!lookup.ok()) {
    scratch =
        frame.value_manager().CreateErrorValue(std::move(lookup).status());
    return ValueView{scratch};
  }
  return *lookup;
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

// ContainerAccessStep performs message field access specified by Expr::Select
// message.
class ContainerAccessStep : public ExpressionStepBase {
 public:
  ContainerAccessStep(int64_t expr_id, bool enable_optional_types)
      : ExpressionStepBase(expr_id),
        enable_optional_types_(enable_optional_types) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  struct LookupResult {
    ValueView value;
    AttributeTrail trail;
  };

  LookupResult PerformLookup(ExecutionFrame* frame, Value& scratch) const;
  absl::StatusOr<ValueView> Lookup(const Value& container, const Value& key,
                                   ExecutionFrame* frame, Value& scratch) const;

  const bool enable_optional_types_;
};

absl::StatusOr<ValueView> ContainerAccessStep::Lookup(const Value& container,
                                                      const Value& key,
                                                      ExecutionFrame* frame,
                                                      Value& scratch) const {
  // Select steps can be applied to either maps or messages
  switch (container->kind()) {
    case ValueKind::kMap: {
      return LookupInMap(container.As<MapValue>(), key, *frame, scratch);
    }
    case ValueKind::kList: {
      return LookupInList(container.As<ListValue>(), key, *frame, scratch);
    }
    default:
      scratch =
          frame->value_factory().CreateErrorValue(absl::InvalidArgumentError(
              absl::StrCat("Invalid container type: '",
                           ValueKindToString(container->kind()), "'")));
      return ValueView{scratch};
  }
}
ContainerAccessStep::LookupResult ContainerAccessStep::PerformLookup(
    ExecutionFrame* frame, Value& scratch) const {
  auto input_args = frame->value_stack().GetSpan(kNumContainerAccessArguments);
  AttributeTrail trail;

  const Value& container = input_args[0];
  const Value& key = input_args[1];

  if (frame->enable_unknowns()) {
    AttributeUtility::Accumulator unknowns =
        frame->attribute_utility().CreateAccumulator();
    unknowns.MaybeAdd(container);
    unknowns.MaybeAdd(key);

    if (!unknowns.IsEmpty()) {
      scratch = std::move(unknowns).Build();
      return {ValueView{scratch}, std::move(trail)};
    }

    // We guarantee that GetAttributeSpan can acquire this number of arguments
    // by calling HasEnough() at the beginning of Execute() method.
    absl::Span<const AttributeTrail> input_attrs =
        frame->value_stack().GetAttributeSpan(kNumContainerAccessArguments);
    const auto& container_trail = input_attrs[0];
    trail = container_trail.Step(AttributeQualifierFromValue(key));

    if (frame->attribute_utility().CheckForUnknownExact(trail)) {
      cel::Attribute attr = trail.attribute();
      scratch = frame->attribute_utility().CreateUnknownSet(attr);
      return {ValueView{scratch}, std::move(trail)};
    }
  }

  if (InstanceOf<ErrorValue>(container)) {
    scratch = container;
    return {ValueView{scratch}, std::move(trail)};
  }
  if (InstanceOf<ErrorValue>(key)) {
    scratch = key;
    return {ValueView{scratch}, std::move(trail)};
  }

  if (enable_optional_types_ &&
      cel::NativeTypeId::Of(container) ==
          cel::NativeTypeId::For<cel::OptionalValueInterface>()) {
    const auto& optional_value =
        *cel::internal::down_cast<const cel::OptionalValueInterface*>(
            cel::Cast<cel::OpaqueValue>(container).operator->());
    if (!optional_value.HasValue()) {
      scratch = cel::OptionalValue::None();
      return {ValueView{scratch}, std::move(trail)};
    }
    auto result = Lookup(optional_value.Value(), key, frame, scratch);
    if (!result.ok()) {
      scratch =
          frame->value_factory().CreateErrorValue(std::move(result).status());
      return {ValueView{scratch}, std::move(trail)};
    }
    if (auto error_value = cel::As<cel::ErrorValueView>(*result);
        error_value && cel::IsNoSuchKey(error_value->NativeValue())) {
      scratch = cel::OptionalValue::None();
      return {ValueView{scratch}, std::move(trail)};
    }
    scratch = cel::OptionalValue::Of(frame->memory_manager(), Value{*result});
    return {ValueView{scratch}, std::move(trail)};
  }
  auto result = Lookup(container, key, frame, scratch);
  if (!result.ok()) {
    scratch =
        frame->value_factory().CreateErrorValue(std::move(result).status());
    return {ValueView{scratch}, std::move(trail)};
  }
  return {*result, std::move(trail)};
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
    const cel::ast_internal::Call& call, int64_t expr_id,
    bool enable_optional_types) {
  int arg_count = call.args().size() + (call.has_target() ? 1 : 0);
  if (arg_count != kNumContainerAccessArguments) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid argument count for index operation: ", arg_count));
  }
  return std::make_unique<ContainerAccessStep>(expr_id, enable_optional_types);
}

}  // namespace google::api::expr::runtime

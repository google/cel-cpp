#include "eval/eval/container_access_step.h"

#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

constexpr int NUM_CONTAINER_ACCESS_ARGUMENTS = 2;

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
                       google::protobuf::Arena* arena) const;
  CelValue LookupInList(const CelList* cel_list, const CelValue& key,
                        google::protobuf::Arena* arena) const;
};

inline CelValue ContainerAccessStep::LookupInMap(const CelMap* cel_map,
                                                 const CelValue& key,
                                                 google::protobuf::Arena* arena) const {
  switch (key.type()) {
    case CelValue::Type::kBool:
    case CelValue::Type::kInt64:
    case CelValue::Type::kUint64:
    case CelValue::Type::kString: {
      absl::optional<CelValue> maybe_value = (*cel_map)[key];
      if (maybe_value.has_value()) {
        return maybe_value.value();
      }
      break;
    }
    default: {
      break;
    }
  }
  return CreateNoSuchKeyError(arena, "Key not found in map");
}

inline CelValue ContainerAccessStep::LookupInList(const CelList* cel_list,
                                                  const CelValue& key,
                                                  google::protobuf::Arena* arena) const {
  switch (key.type()) {
    case CelValue::Type::kInt64: {
      int64_t idx = key.Int64OrDie();
      if (idx < 0 || idx >= cel_list->size()) {
        return CreateErrorValue(arena,
                                absl::StrCat("Index error: index=", idx,
                                             " size=", cel_list->size()));
      }
      return (*cel_list)[idx];
    }
    default: {
      return CreateErrorValue(
          arena, absl::StrCat("Index error: expected integer type, got ",
                              CelValue::TypeName(key.type())));
    }
  }
}

ContainerAccessStep::ValueAttributePair ContainerAccessStep::PerformLookup(
    ExecutionFrame* frame) const {
  auto input_args =
      frame->value_stack().GetSpan(NUM_CONTAINER_ACCESS_ARGUMENTS);
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
        frame->value_stack().GetAttributeSpan(NUM_CONTAINER_ACCESS_ARGUMENTS);
    auto container_trail = input_attrs[0];
    trail = container_trail.Step(CelAttributeQualifier::Create(key),
                                 frame->arena());

    if (frame->attribute_utility().CheckForUnknown(trail,
                                                   /*use_partial=*/false)) {
      auto unknown_set = google::protobuf::Arena::Create<UnknownSet>(
          frame->arena(), UnknownAttributeSet({trail.attribute()}));

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
      return {LookupInMap(cel_map, key, frame->arena()), trail};
    }
    case CelValue::Type::kList: {
      const CelList* cel_list = container.ListOrDie();
      return {LookupInList(cel_list, key, frame->arena()), trail};
    }
    default: {
      auto error = CreateErrorValue(
          frame->arena(),
          absl::StrCat("Unexpected container type for [] operation: ",
                       CelValue::TypeName(key.type())));
      return {error, trail};
    }
  }
}

absl::Status ContainerAccessStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(NUM_CONTAINER_ACCESS_ARGUMENTS)) {
    return absl::Status(
        absl::StatusCode::kInternal,
        "Insufficient arguments supplied for ContainerAccess-type expression");
  }

  auto result = PerformLookup(frame);
  frame->value_stack().Pop(NUM_CONTAINER_ACCESS_ARGUMENTS);
  frame->value_stack().Push(result.first, result.second);

  return absl::OkStatus();
}
}  // namespace

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateContainerAccessStep(
    const google::api::expr::v1alpha1::Expr::Call*, int64_t expr_id) {
  return absl::make_unique<ContainerAccessStep>(expr_id);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

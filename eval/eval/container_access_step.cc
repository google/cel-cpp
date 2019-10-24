#include "eval/eval/container_access_step.h"

#include "google/protobuf/arena.h"
#include "absl/strings/str_cat.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_value.h"
#include "base/status.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

// ContainerAccessStep performs message field access specified by Expr::Select
// message.
class ContainerAccessStep : public ExpressionStepBase {
 public:
  ContainerAccessStep(int64_t expr_id) : ExpressionStepBase(expr_id) {}

  cel_base::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  CelValue PerformLookup(const CelValue& container, const CelValue& key,
                         google::protobuf::Arena* arena) const;
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
  return CreateNoSuchKeyError(arena, absl::StrCat("Key not found in map"));
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

CelValue ContainerAccessStep::PerformLookup(const CelValue& container,
                                            const CelValue& key,
                                            google::protobuf::Arena* arena) const {
  if (container.IsError()) {
    return container;
  }
  if (key.IsError()) {
    return key;
  }
  // Select steps can be applied to either maps or messages
  switch (container.type()) {
    case CelValue::Type::kMap: {
      const CelMap* cel_map = container.MapOrDie();
      return LookupInMap(cel_map, key, arena);
    }
    case CelValue::Type::kList: {
      const CelList* cel_list = container.ListOrDie();
      return LookupInList(cel_list, key, arena);
    }
    default: {
      return CreateErrorValue(
          arena, absl::StrCat("Unexpected container type for [] operation: ",
                              CelValue::TypeName(key.type())));
    }
  }
}

cel_base::Status ContainerAccessStep::Evaluate(ExecutionFrame* frame) const {
  const int NUM_ARGUMENTS = 2;

  if (!frame->value_stack().HasEnough(NUM_ARGUMENTS)) {
    return cel_base::Status(
        cel_base::StatusCode::kInternal,
        "Insufficient arguments supplied for ContainerAccess-type expression");
  }

  auto input_args = frame->value_stack().GetSpan(NUM_ARGUMENTS);

  const CelValue& container = input_args[0];
  const CelValue& key = input_args[1];

  CelValue result = PerformLookup(container, key, frame->arena());
  frame->value_stack().Pop(NUM_ARGUMENTS);
  frame->value_stack().Push(result);

  return cel_base::OkStatus();
}
}  // namespace

// Factory method for Select - based Execution step
cel_base::StatusOr<std::unique_ptr<ExpressionStep>> CreateContainerAccessStep(
    const google::api::expr::v1alpha1::Expr::Call*, int64_t expr_id) {
  std::unique_ptr<ExpressionStep> step =
      absl::make_unique<ContainerAccessStep>(expr_id);
  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

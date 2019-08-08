#include "eval/eval/select_step.h"
#include "eval/eval/expression_step_base.h"
#include "eval/eval/field_access.h"
#include "eval/eval/field_backed_list_impl.h"
#include "eval/eval/field_backed_map_impl.h"
#include "absl/strings/str_cat.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::protobuf::Reflection;
using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;

// SelectStep performs message field access specified by Expr::Select
// message.
class SelectStep : public ExpressionStepBase {
 public:
  SelectStep(absl::string_view field, bool test_field_presence, int64_t expr_id,
             absl::string_view select_path)
      : ExpressionStepBase(expr_id),
        field_(field),
        test_field_presence_(test_field_presence),
        select_path_(select_path) {}

  util::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  util::Status CreateValueFromField(const google::protobuf::Message* message,
                                    google::protobuf::Arena* arena,
                                    CelValue* result) const;

  std::string field_;
  bool test_field_presence_;
  std::string select_path_;
};

util::Status SelectStep::CreateValueFromField(const google::protobuf::Message* msg,
                                              google::protobuf::Arena* arena,
                                              CelValue* result) const {
  const Reflection* reflection = msg->GetReflection();
  const Descriptor* desc = msg->GetDescriptor();
  const FieldDescriptor* field_desc = desc->FindFieldByName(field_);

  if (field_desc == nullptr) {
    *result = CreateNoSuchFieldError(arena);
    return util::OkStatus();
  }

  if (field_desc->is_map()) {
    *result = CelValue::CreateMap(google::protobuf::Arena::Create<FieldBackedMapImpl>(
        arena, msg, field_desc, arena));
    return util::OkStatus();
  }
  if (field_desc->is_repeated()) {
    *result = CelValue::CreateList(google::protobuf::Arena::Create<FieldBackedListImpl>(
        arena, msg, field_desc, arena));
    return util::OkStatus();
  }
  if (test_field_presence_) {
    *result = CelValue::CreateBool(reflection->HasField(*msg, field_desc));
    return util::OkStatus();
  }
  return CreateValueFromSingleField(msg, field_desc, arena, result);
}

util::Status SelectStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return util::MakeStatus(google::rpc::Code::INTERNAL,
                        "No arguments supplied for Select-type expression");
  }

  CelValue arg = frame->value_stack().Peek();

  // Non-empty select path - check if value mapped to unknown.
  bool unknown_value = false;
  if (!select_path_.empty()) {
    unknown_value = frame->activation().IsPathUnknown(select_path_);
  }

  // Select steps can be applied to either maps or messages
  switch (arg.type()) {
    case CelValue::Type::kMessage: {
      const google::protobuf::Message* msg = arg.MessageOrDie();

      if (msg == nullptr) {
        CelValue error_value =
            CreateErrorValue(frame->arena(), "Message is NULL");
        frame->value_stack().PopAndPush(error_value);
        return util::OkStatus();
      }

      if (unknown_value) {
        CelValue error_value = CreateErrorValue(
            frame->arena(), absl::StrCat("Unknown value ", select_path_));
        frame->value_stack().PopAndPush(error_value);
        return util::OkStatus();
      }

      util::Status status = CreateValueFromField(msg, frame->arena(), &arg);

      if (util::IsOk(status)) {
        frame->value_stack().PopAndPush(arg);
      }

      return status;
    }
    case CelValue::Type::kMap: {
      const CelMap* cel_map = arg.MapOrDie();

      if (cel_map == nullptr) {
        CelValue error_value = CreateErrorValue(frame->arena(), "Map is NULL");
        frame->value_stack().PopAndPush(error_value);
        return util::OkStatus();
      }

      if (unknown_value) {
        CelValue error_value = CreateErrorValue(
            frame->arena(), absl::StrCat("Unknown value ", select_path_));
        frame->value_stack().PopAndPush(error_value);
        return util::OkStatus();
      }

      auto lookup_result = (*cel_map)[CelValue::CreateString(&field_)];

      // Test only Select expression.
      if (test_field_presence_) {
        arg = CelValue::CreateBool(lookup_result.has_value());
        frame->value_stack().PopAndPush(arg);
        return util::OkStatus();
      }

      // If object is not found, we return Error, per CEL specification.
      if (lookup_result) {
        arg = lookup_result.value();
      } else {
        arg = CreateNoSuchKeyError(frame->arena(), field_);
      }
      frame->value_stack().PopAndPush(arg);

      return util::OkStatus();
    }
    case CelValue::Type::kError: {
      // If argument is CelError, we propagate it forward.
      // It is already on the top of the stack.
      return util::OkStatus();
    }
    default:
      return util::MakeStatus(google::rpc::Code::INVALID_ARGUMENT,
                          "Applying SELECT to non-message type");
  }
}

}  // namespace

// Factory method for Select - based Execution step
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateSelectStep(
    const google::api::expr::v1alpha1::Expr::Select* select_expr, int64_t expr_id,
    absl::string_view select_path) {
  std::unique_ptr<ExpressionStep> step = absl::make_unique<SelectStep>(
      select_expr->field(), select_expr->test_only(), expr_id, select_path);
  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

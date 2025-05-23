#include "eval/eval/create_list_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/attribute.h"
#include "base/attribute_set.h"
#include "base/type_provider.h"
#include "common/casting.h"
#include "common/expr.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/ident_step.h"
#include "eval/internal/interop.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "eval/public/testing/matchers.h"
#include "eval/public/unknown_attribute_set.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "runtime/activation.h"
#include "runtime/internal/runtime_env.h"
#include "runtime/internal/runtime_env_testing.h"
#include "runtime/internal/runtime_type_provider.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::Attribute;
using ::cel::AttributeQualifier;
using ::cel::AttributeSet;
using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::Expr;
using ::cel::InstanceOf;
using ::cel::IntValue;
using ::cel::ListValue;
using ::cel::TypeProvider;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::runtime_internal::NewTestingRuntimeEnv;
using ::cel::runtime_internal::RuntimeEnv;
using ::cel::test::IntValueIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

// Helper method. Creates simple pipeline containing Select step and runs it.
absl::StatusOr<CelValue> RunExpression(
    const ABSL_NONNULL std::shared_ptr<const RuntimeEnv>& env,
    const std::vector<int64_t>& values, google::protobuf::Arena* arena,
    bool enable_unknowns) {
  ExecutionPath path;
  Expr dummy_expr;

  auto& create_list = dummy_expr.mutable_list_expr();
  for (auto value : values) {
    auto& expr0 = create_list.mutable_elements().emplace_back().mutable_expr();
    expr0.mutable_const_expr().set_int64_value(value);
    CEL_ASSIGN_OR_RETURN(
        auto const_step,
        CreateConstValueStep(cel::interop_internal::CreateIntValue(value),
                             /*expr_id=*/-1));
    path.push_back(std::move(const_step));
  }

  CEL_ASSIGN_OR_RETURN(auto step,
                       CreateCreateListStep(create_list, dummy_expr.id()));
  path.push_back(std::move(step));
  cel::RuntimeOptions options;
  if (enable_unknowns) {
    options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  }
  CelExpressionFlatImpl cel_expr(
      env,

      FlatExpression(std::move(path),
                     /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));
  Activation activation;

  return cel_expr.Evaluate(activation, arena);
}

// Helper method. Creates simple pipeline containing Select step and runs it.
absl::StatusOr<CelValue> RunExpressionWithCelValues(
    const ABSL_NONNULL std::shared_ptr<const RuntimeEnv>& env,
    const std::vector<CelValue>& values, google::protobuf::Arena* arena,
    bool enable_unknowns) {
  ExecutionPath path;
  Expr dummy_expr;

  Activation activation;
  auto& create_list = dummy_expr.mutable_list_expr();
  int ind = 0;
  for (auto value : values) {
    std::string var_name = absl::StrCat("name_", ind++);
    auto& expr0 = create_list.mutable_elements().emplace_back().mutable_expr();
    expr0.set_id(ind);
    expr0.mutable_ident_expr().set_name(var_name);

    CEL_ASSIGN_OR_RETURN(auto ident_step,
                         CreateIdentStep(expr0.ident_expr(), expr0.id()));
    path.push_back(std::move(ident_step));
    activation.InsertValue(var_name, value);
  }

  CEL_ASSIGN_OR_RETURN(auto step0,
                       CreateCreateListStep(create_list, dummy_expr.id()));
  path.push_back(std::move(step0));

  cel::RuntimeOptions options;
  if (enable_unknowns) {
    options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  }

  CelExpressionFlatImpl cel_expr(
      env,
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     env->type_registry.GetComposedTypeProvider(), options));

  return cel_expr.Evaluate(activation, arena);
}

class CreateListStepTest : public testing::TestWithParam<bool> {
 public:
  CreateListStepTest() : env_(NewTestingRuntimeEnv()) {}

 protected:
  ABSL_NONNULL std::shared_ptr<const RuntimeEnv> env_;
  google::protobuf::Arena arena_;
};

// Tests error when not enough list elements are on the stack during list
// creation.
TEST(CreateListStepTest, TestCreateListStackUnderflow) {
  ExecutionPath path;
  Expr dummy_expr;

  auto& create_list = dummy_expr.mutable_list_expr();
  auto& expr0 = create_list.mutable_elements().emplace_back().mutable_expr();
  expr0.mutable_const_expr().set_int64_value(1);

  ASSERT_OK_AND_ASSIGN(auto step0,
                       CreateCreateListStep(create_list, dummy_expr.id()));
  path.push_back(std::move(step0));

  auto env = NewTestingRuntimeEnv();
  CelExpressionFlatImpl cel_expr(
      env, FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                          env->type_registry.GetComposedTypeProvider(),
                          cel::RuntimeOptions{}));
  Activation activation;

  google::protobuf::Arena arena;

  auto status = cel_expr.Evaluate(activation, &arena);
  ASSERT_THAT(status, Not(IsOk()));
}

TEST_P(CreateListStepTest, CreateListEmpty) {
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(env_, {}, &arena_, GetParam()));
  ASSERT_TRUE(result.IsList());
  EXPECT_THAT(result.ListOrDie()->size(), Eq(0));
}

TEST_P(CreateListStepTest, CreateListOne) {
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(env_, {100}, &arena_, GetParam()));
  ASSERT_TRUE(result.IsList());
  const auto& list = *result.ListOrDie();
  ASSERT_THAT(list.size(), Eq(1));
  const CelValue& value = list.Get(&arena_, 0);
  EXPECT_THAT(value, test::IsCelInt64(100));
}

TEST_P(CreateListStepTest, CreateListWithError) {
  std::vector<CelValue> values;
  CelError error = absl::InvalidArgumentError("bad arg");
  values.push_back(CelValue::CreateError(&error));
  ASSERT_OK_AND_ASSIGN(CelValue result, RunExpressionWithCelValues(
                                            env_, values, &arena_, GetParam()));

  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(), Eq(absl::InvalidArgumentError("bad arg")));
}

TEST_P(CreateListStepTest, CreateListWithErrorAndUnknown) {
  // list composition is: {unknown, error}
  std::vector<CelValue> values;
  Expr expr0;
  expr0.mutable_ident_expr().set_name("name0");
  CelAttribute attr0(expr0.ident_expr().name(), {});
  UnknownSet unknown_set0(UnknownAttributeSet({attr0}));
  values.push_back(CelValue::CreateUnknownSet(&unknown_set0));
  CelError error = absl::InvalidArgumentError("bad arg");
  values.push_back(CelValue::CreateError(&error));

  ASSERT_OK_AND_ASSIGN(CelValue result, RunExpressionWithCelValues(
                                            env_, values, &arena_, GetParam()));

  // The bad arg should win.
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*result.ErrorOrDie(), Eq(absl::InvalidArgumentError("bad arg")));
}

TEST_P(CreateListStepTest, CreateListHundred) {
  std::vector<int64_t> values;
  for (size_t i = 0; i < 100; i++) {
    values.push_back(i);
  }
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(env_, values, &arena_, GetParam()));
  ASSERT_TRUE(result.IsList());
  const auto& list = *result.ListOrDie();
  EXPECT_THAT(list.size(), Eq(static_cast<int>(values.size())));
  for (size_t i = 0; i < values.size(); i++) {
    EXPECT_THAT(list.Get(&arena_, i), test::IsCelInt64(values[i]));
  }
}

INSTANTIATE_TEST_SUITE_P(CombinedCreateListTest, CreateListStepTest,
                         testing::Bool());

TEST(CreateListStepTest, CreateListHundredAnd2Unknowns) {
  google::protobuf::Arena arena;
  std::vector<CelValue> values;

  Expr expr0;
  expr0.mutable_ident_expr().set_name("name0");
  CelAttribute attr0(expr0.ident_expr().name(), {});
  Expr expr1;
  expr1.mutable_ident_expr().set_name("name1");
  CelAttribute attr1(expr1.ident_expr().name(), {});
  UnknownSet unknown_set0(UnknownAttributeSet({attr0}));
  UnknownSet unknown_set1(UnknownAttributeSet({attr1}));
  for (size_t i = 0; i < 100; i++) {
    values.push_back(CelValue::CreateInt64(i));
  }
  values.push_back(CelValue::CreateUnknownSet(&unknown_set0));
  values.push_back(CelValue::CreateUnknownSet(&unknown_set1));

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpressionWithCelValues(NewTestingRuntimeEnv(), values, &arena, true));
  ASSERT_TRUE(result.IsUnknownSet());
  const UnknownSet* result_set = result.UnknownSetOrDie();
  EXPECT_THAT(result_set->unknown_attributes().size(), Eq(2));
}

TEST(CreateDirectListStep, Basic) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  cel::Activation activation;
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateConstValueDirectStep(IntValue(1), -1));
  deps.push_back(CreateConstValueDirectStep(IntValue(2), -1));
  auto step = CreateDirectListStep(std::move(deps), {}, -1);

  cel::Value result;
  AttributeTrail attr;

  ASSERT_OK(step->Evaluate(frame, result, attr));

  ASSERT_TRUE(InstanceOf<ListValue>(result));
  EXPECT_THAT(Cast<ListValue>(result).Size(), IsOkAndHolds(2));
}

TEST(CreateDirectListStep, ForwardFirstError) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  cel::Activation activation;
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateConstValueDirectStep(
      cel::ErrorValue(absl::InternalError("test1")), -1));
  deps.push_back(CreateConstValueDirectStep(
      cel::ErrorValue(absl::InternalError("test2")), -1));
  auto step = CreateDirectListStep(std::move(deps), {}, -1);

  cel::Value result;
  AttributeTrail attr;

  ASSERT_OK(step->Evaluate(frame, result, attr));

  ASSERT_TRUE(InstanceOf<ErrorValue>(result));
  EXPECT_THAT(Cast<ErrorValue>(result).NativeValue(),
              StatusIs(absl::StatusCode::kInternal, "test1"));
}

std::vector<std::string> UnknownAttrNames(const UnknownValue& v) {
  std::vector<std::string> names;
  names.reserve(v.attribute_set().size());

  for (const auto& attr : v.attribute_set()) {
    EXPECT_OK(attr.AsString().status());
    names.push_back(attr.AsString().value_or("<empty>"));
  }
  return names;
}

TEST(CreateDirectListStep, MergeUnknowns) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  cel::Activation activation;
  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  AttributeSet attr_set1({Attribute("var1")});
  AttributeSet attr_set2({Attribute("var2")});

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateConstValueDirectStep(
      cel::UnknownValue(cel::Unknown(std::move(attr_set1))), -1));
  deps.push_back(CreateConstValueDirectStep(
      cel::UnknownValue(cel::Unknown(std::move(attr_set2))), -1));
  auto step = CreateDirectListStep(std::move(deps), {}, -1);

  cel::Value result;
  AttributeTrail attr;

  ASSERT_OK(step->Evaluate(frame, result, attr));

  ASSERT_TRUE(InstanceOf<UnknownValue>(result));
  EXPECT_THAT(UnknownAttrNames(Cast<UnknownValue>(result)),
              UnorderedElementsAre("var1", "var2"));
}

TEST(CreateDirectListStep, ErrorBeforeUnknown) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  cel::Activation activation;
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  AttributeSet attr_set1({Attribute("var1")});

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateConstValueDirectStep(
      cel::ErrorValue(absl::InternalError("test1")), -1));
  deps.push_back(CreateConstValueDirectStep(
      cel::ErrorValue(absl::InternalError("test2")), -1));
  auto step = CreateDirectListStep(std::move(deps), {}, -1);

  cel::Value result;
  AttributeTrail attr;

  ASSERT_OK(step->Evaluate(frame, result, attr));

  ASSERT_TRUE(InstanceOf<ErrorValue>(result));
  EXPECT_THAT(Cast<ErrorValue>(result).NativeValue(),
              StatusIs(absl::StatusCode::kInternal, "test1"));
}

class SetAttrDirectStep : public DirectExpressionStep {
 public:
  explicit SetAttrDirectStep(Attribute attr)
      : DirectExpressionStep(-1), attr_(std::move(attr)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attr) const override {
    result = cel::NullValue();
    attr = AttributeTrail(attr_);
    return absl::OkStatus();
  }

 private:
  cel::Attribute attr_;
};

TEST(CreateDirectListStep, MissingAttribute) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  cel::Activation activation;
  cel::RuntimeOptions options;
  options.enable_missing_attribute_errors = true;

  activation.SetMissingPatterns({cel::AttributePattern(
      "var1", {cel::AttributeQualifierPattern::OfString("field1")})});

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateConstValueDirectStep(cel::NullValue(), -1));
  deps.push_back(std::make_unique<SetAttrDirectStep>(
      Attribute("var1", {AttributeQualifier::OfString("field1")})));
  auto step = CreateDirectListStep(std::move(deps), {}, -1);

  cel::Value result;
  AttributeTrail attr;

  ASSERT_OK(step->Evaluate(frame, result, attr));

  ASSERT_TRUE(InstanceOf<ErrorValue>(result));
  EXPECT_THAT(
      Cast<ErrorValue>(result).NativeValue(),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("var1.field1")));
}

TEST(CreateDirectListStep, OptionalPresentSet) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  cel::Activation activation;
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateConstValueDirectStep(IntValue(1), -1));
  deps.push_back(CreateConstValueDirectStep(
      cel::OptionalValue::Of(IntValue(2), &arena), -1));
  auto step = CreateDirectListStep(std::move(deps), {1}, -1);

  cel::Value result;
  AttributeTrail attr;

  ASSERT_OK(step->Evaluate(frame, result, attr));

  ASSERT_TRUE(InstanceOf<ListValue>(result));
  auto list = Cast<ListValue>(result);
  EXPECT_THAT(list.Size(), IsOkAndHolds(2));
  EXPECT_THAT(list.Get(0, cel::internal::GetTestingDescriptorPool(),
                       cel::internal::GetTestingMessageFactory(), &arena),
              IsOkAndHolds(IntValueIs(1)));
  EXPECT_THAT(list.Get(1, cel::internal::GetTestingDescriptorPool(),
                       cel::internal::GetTestingMessageFactory(), &arena),
              IsOkAndHolds(IntValueIs(2)));
}

TEST(CreateDirectListStep, OptionalAbsentNotSet) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  cel::Activation activation;
  cel::RuntimeOptions options;

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateConstValueDirectStep(IntValue(1), -1));
  deps.push_back(CreateConstValueDirectStep(cel::OptionalValue::None(), -1));
  auto step = CreateDirectListStep(std::move(deps), {1}, -1);

  cel::Value result;
  AttributeTrail attr;

  ASSERT_OK(step->Evaluate(frame, result, attr));

  ASSERT_TRUE(InstanceOf<ListValue>(result));
  auto list = Cast<ListValue>(result);
  EXPECT_THAT(list.Size(), IsOkAndHolds(1));
  EXPECT_THAT(list.Get(0, cel::internal::GetTestingDescriptorPool(),
                       cel::internal::GetTestingMessageFactory(), &arena),
              IsOkAndHolds(IntValueIs(1)));
}

TEST(CreateDirectListStep, PartialUnknown) {
  google::protobuf::Arena arena;
  cel::runtime_internal::RuntimeTypeProvider type_provider(
      cel::internal::GetTestingDescriptorPool());

  cel::Activation activation;
  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  activation.SetUnknownPatterns({cel::AttributePattern(
      "var1", {cel::AttributeQualifierPattern::OfString("field1")})});

  ExecutionFrameBase frame(activation, options, type_provider,
                           cel::internal::GetTestingDescriptorPool(),
                           cel::internal::GetTestingMessageFactory(), &arena);

  std::vector<std::unique_ptr<DirectExpressionStep>> deps;
  deps.push_back(CreateConstValueDirectStep(cel::IntValue(1), -1));
  deps.push_back(std::make_unique<SetAttrDirectStep>(Attribute("var1", {})));
  auto step = CreateDirectListStep(std::move(deps), {}, -1);

  cel::Value result;
  AttributeTrail attr;

  ASSERT_OK(step->Evaluate(frame, result, attr));

  ASSERT_TRUE(InstanceOf<UnknownValue>(result));
  EXPECT_THAT(UnknownAttrNames(Cast<UnknownValue>(result)),
              UnorderedElementsAre("var1"));
}

}  // namespace

}  // namespace google::api::expr::runtime

// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "runtime/optional_types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "base/function.h"
#include "base/function_descriptor.h"
#include "common/kind.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/values/legacy_value_manager.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "internal/testing.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::cel::extensions::ProtobufRuntimeAdapter;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::parser::ParserOptions;
using testing::ElementsAre;
using testing::HasSubstr;
using cel::internal::IsOk;
using cel::internal::StatusIs;

MATCHER_P(MatchesOptionalReceiver1, name, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;

  std::vector<Kind> types{Kind::kOpaque};
  return descriptor.name() == name && descriptor.receiver_style() == true &&
         descriptor.types() == types;
}

MATCHER_P2(MatchesOptionalReceiver2, name, kind, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;

  std::vector<Kind> types{Kind::kOpaque, kind};
  return descriptor.name() == name && descriptor.receiver_style() == true &&
         descriptor.types() == types;
}

MATCHER_P2(MatchesOptionalSelect, kind1, kind2, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;

  std::vector<Kind> types{kind1, kind2};
  return descriptor.name() == "_?._" && descriptor.receiver_style() == false &&
         descriptor.types() == types;
}

MATCHER_P2(MatchesOptionalIndex, kind1, kind2, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;

  std::vector<Kind> types{kind1, kind2};
  return descriptor.name() == "_[?_]" && descriptor.receiver_style() == false &&
         descriptor.types() == types;
}

TEST(EnableOptionalTypes, HeterogeneousEqualityRequired) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(RuntimeOptions{
                           .enable_qualified_type_identifiers = true,
                           .enable_heterogeneous_equality = false}));
  EXPECT_THAT(EnableOptionalTypes(builder),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(EnableOptionalTypes, QualifiedTypeIdentifiersRequired) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(RuntimeOptions{
                           .enable_qualified_type_identifiers = false,
                           .enable_heterogeneous_equality = true}));
  EXPECT_THAT(EnableOptionalTypes(builder),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(EnableOptionalTypes, PreconditionsSatisfied) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(RuntimeOptions{
                           .enable_qualified_type_identifiers = true,
                           .enable_heterogeneous_equality = true}));
  EXPECT_THAT(EnableOptionalTypes(builder), IsOk());
}

TEST(EnableOptionalTypes, Functions) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(RuntimeOptions{
                           .enable_qualified_type_identifiers = true,
                           .enable_heterogeneous_equality = true}));
  ASSERT_THAT(EnableOptionalTypes(builder), IsOk());

  EXPECT_THAT(builder.function_registry().FindStaticOverloads("hasValue", true,
                                                              {Kind::kOpaque}),
              ElementsAre(MatchesOptionalReceiver1("hasValue")));

  EXPECT_THAT(builder.function_registry().FindStaticOverloads("value", true,
                                                              {Kind::kOpaque}),
              ElementsAre(MatchesOptionalReceiver1("value")));

  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "or", true, {Kind::kOpaque, Kind::kOpaque}),
              ElementsAre(MatchesOptionalReceiver2("or", Kind::kOpaque)));

  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "orValue", true, {Kind::kOpaque, Kind::kAny}),
              ElementsAre(MatchesOptionalReceiver2("orValue", Kind::kAny)));

  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_?._", false, {Kind::kStruct, Kind::kString}),
              ElementsAre(MatchesOptionalSelect(Kind::kStruct, Kind::kString)));
  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_?._", false, {Kind::kMap, Kind::kString}),
              ElementsAre(MatchesOptionalSelect(Kind::kMap, Kind::kString)));
  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_?._", false, {Kind::kOpaque, Kind::kString}),
              ElementsAre(MatchesOptionalSelect(Kind::kOpaque, Kind::kString)));

  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_[?_]", false, {Kind::kMap, Kind::kAny}),
              ElementsAre(MatchesOptionalIndex(Kind::kMap, Kind::kAny)));
  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_[?_]", false, {Kind::kList, Kind::kInt}),
              ElementsAre(MatchesOptionalIndex(Kind::kList, Kind::kInt)));
  EXPECT_THAT(builder.function_registry().FindStaticOverloads(
                  "_[?_]", false, {Kind::kOpaque, Kind::kAny}),
              ElementsAre(MatchesOptionalIndex(Kind::kOpaque, Kind::kAny)));
}

struct EvaluateResultTestCase {
  std::string name;
  std::string expression;
  bool expected_result;
};

class OptionalTypesTest
    : public ::testing::TestWithParam<EvaluateResultTestCase> {};

std::string TestCaseName(
    const testing::TestParamInfo<EvaluateResultTestCase>& info) {
  return info.param.name;
}

TEST_P(OptionalTypesTest, DefaultsRefCounted) {
  RuntimeOptions opts{.enable_qualified_type_identifiers = true};
  const EvaluateResultTestCase& test_case = GetParam();
  MemoryManagerRef memory_manager = MemoryManagerRef::ReferenceCounting();

  ASSERT_OK_AND_ASSIGN(auto builder, CreateStandardRuntimeBuilder(opts));

  ASSERT_OK(EnableOptionalTypes(builder));
  ASSERT_OK(
      EnableReferenceResolver(builder, ReferenceResolverEnabled::kAlways));

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse(test_case.expression, "<input>",
                             ParserOptions{.enable_optional_syntax = true}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  cel::common_internal::LegacyValueManager value_factory(
      memory_manager, runtime->GetTypeProvider());

  Activation activation;

  ASSERT_OK_AND_ASSIGN(Value result,
                       program->Evaluate(activation, value_factory));

  ASSERT_TRUE(result->Is<BoolValue>()) << result->DebugString();
  EXPECT_EQ(result->As<BoolValue>().NativeValue(), test_case.expected_result)
      << test_case.expression;
}

TEST_P(OptionalTypesTest, DefaultsArena) {
  RuntimeOptions opts{.enable_qualified_type_identifiers = true};
  const EvaluateResultTestCase& test_case = GetParam();
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);

  ASSERT_OK_AND_ASSIGN(auto builder, CreateStandardRuntimeBuilder(opts));

  ASSERT_OK(EnableOptionalTypes(builder));
  ASSERT_OK(
      EnableReferenceResolver(builder, ReferenceResolverEnabled::kAlways));

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse(test_case.expression, "<input>",
                             ParserOptions{.enable_optional_syntax = true}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  common_internal::LegacyValueManager value_factory(memory_manager,
                                                    runtime->GetTypeProvider());

  Activation activation;

  ASSERT_OK_AND_ASSIGN(Value result,
                       program->Evaluate(activation, value_factory));

  ASSERT_TRUE(result->Is<BoolValue>()) << result->DebugString();
  EXPECT_EQ(result->As<BoolValue>().NativeValue(), test_case.expected_result)
      << test_case.expression;
}

INSTANTIATE_TEST_SUITE_P(
    Basic, OptionalTypesTest,
    testing::ValuesIn(std::vector<EvaluateResultTestCase>{
        {"optional_none_hasValue", "optional.none().hasValue()", false},
        {"optional_of_hasValue", "optional.of(0).hasValue()", true},
        {"optional_ofNonZeroValue_hasValue",
         "optional.ofNonZeroValue(0).hasValue()", false},
    }),
    TestCaseName);

class UnreachableFunction final : public cel::Function {
 public:
  explicit UnreachableFunction(int64_t* count) : count_(count) {}

  absl::StatusOr<Value> Invoke(const InvokeContext& context,
                               absl::Span<const Value> args) const override {
    ++(*count_);
    return ErrorValue{absl::CancelledError()};
  }

 private:
  int64_t* const count_;
};

TEST(OptionalTypesTest, ErrorShortCircuiting) {
  RuntimeOptions opts{.enable_qualified_type_identifiers = true};
  google::protobuf::Arena arena;
  auto memory_manager = ProtoMemoryManagerRef(&arena);

  ASSERT_OK_AND_ASSIGN(auto builder, CreateStandardRuntimeBuilder(opts));

  int64_t unreachable_count = 0;

  ASSERT_OK(EnableOptionalTypes(builder));
  ASSERT_OK(
      EnableReferenceResolver(builder, ReferenceResolverEnabled::kAlways));
  ASSERT_OK(builder.function_registry().Register(
      cel::FunctionDescriptor("unreachable", false, {}),
      std::make_unique<UnreachableFunction>(&unreachable_count)));

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(
      ParsedExpr expr,
      Parse("optional.of(1 / 0).orValue(unreachable())", "<input>",
            ParserOptions{.enable_optional_syntax = true}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  common_internal::LegacyValueManager value_factory(memory_manager,
                                                    runtime->GetTypeProvider());

  Activation activation;

  ASSERT_OK_AND_ASSIGN(Value result,
                       program->Evaluate(activation, value_factory));

  EXPECT_EQ(unreachable_count, 0);
  ASSERT_TRUE(result->Is<ErrorValue>()) << result->DebugString();
  EXPECT_THAT(result->As<ErrorValue>().NativeValue(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("divide by zero")));
}

}  // namespace
}  // namespace cel::extensions

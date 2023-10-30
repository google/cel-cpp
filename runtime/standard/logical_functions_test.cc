// Copyright 2022 Google LLC
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

#include "runtime/standard/logical_functions.h"

#include <functional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/builtins.h"
#include "base/function.h"
#include "base/function_descriptor.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "internal/testing.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel {
namespace {

using testing::ElementsAre;
using testing::HasSubstr;
using testing::Matcher;
using testing::Truly;

MATCHER_P3(DescriptorIs, name, arg_kinds, is_receiver, "") {
  const FunctionOverloadReference& ref = arg;
  const FunctionDescriptor& descriptor = ref.descriptor;
  return descriptor.name() == name &&
         descriptor.ShapeMatches(is_receiver, arg_kinds);
}

MATCHER_P(IsBool, expected, "") {
  const Handle<Value>& value = arg;
  return value->Is<BoolValue>() &&
         value->As<BoolValue>().NativeValue() == expected;
}

// TODO(uncreated-issue/48): replace this with a parsed expr when the non-protobuf
// parser is available.
absl::StatusOr<Handle<Value>> TestDispatchToFunction(
    const FunctionRegistry& registry, absl::string_view simple_name,
    absl::Span<const Handle<Value>> args, ValueFactory& value_factory) {
  std::vector<Kind> arg_matcher_;
  arg_matcher_.reserve(args.size());
  for (const auto& value : args) {
    arg_matcher_.push_back(ValueKindToKind(value->kind()));
  }
  std::vector<FunctionOverloadReference> refs = registry.FindStaticOverloads(
      simple_name, /*receiver_style=*/false, arg_matcher_);

  if (refs.size() != 1) {
    return absl::InvalidArgumentError("ambiguous overloads");
  }

  Function::InvokeContext ctx(value_factory);
  return refs[0].implementation.Invoke(ctx, args);
}

TEST(RegisterLogicalFunctions, NotStrictlyFalseRegistered) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterLogicalFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kNotStrictlyFalse,
                                   /*receiver_style=*/false, {Kind::kAny}),
      ElementsAre(DescriptorIs(builtin::kNotStrictlyFalse,
                               std::vector<Kind>{Kind::kBool}, false)));
}

TEST(RegisterLogicalFunctions, LogicalNotRegistered) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterLogicalFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kNot,
                                   /*receiver_style=*/false, {Kind::kAny}),
      ElementsAre(
          DescriptorIs(builtin::kNot, std::vector<Kind>{Kind::kBool}, false)));
}

struct TestCase {
  using ArgumentFactory =
      std::function<std::vector<Handle<Value>>(ValueFactory&)>;

  std::string function;
  ArgumentFactory arguments;
  absl::StatusOr<Matcher<Handle<Value>>> result_matcher;
};

class LogicalFunctionsTest : public testing::TestWithParam<TestCase> {
 public:
  LogicalFunctionsTest()
      : type_factory_(MemoryManager::Global()),
        type_manager_(type_factory_, TypeProvider::Builtin()),
        value_factory_(type_manager_) {}

 protected:
  TypeFactory type_factory_;
  TypeManager type_manager_;
  ValueFactory value_factory_;
};

TEST_P(LogicalFunctionsTest, Runner) {
  const TestCase& test_case = GetParam();
  cel::FunctionRegistry registry;

  ASSERT_OK(RegisterLogicalFunctions(registry, RuntimeOptions()));

  std::vector<Handle<Value>> args = test_case.arguments(value_factory_);

  absl::StatusOr<Handle<Value>> result = TestDispatchToFunction(
      registry, test_case.function, args, value_factory_);

  EXPECT_EQ(result.ok(), test_case.result_matcher.ok());

  if (!test_case.result_matcher.ok()) {
    EXPECT_EQ(result.status().code(), test_case.result_matcher.status().code());
    EXPECT_THAT(result.status().message(),
                HasSubstr(test_case.result_matcher.status().message()));
  } else {
    ASSERT_TRUE(result.ok()) << "unexpected error" << result.status();
    EXPECT_THAT(*result, *test_case.result_matcher);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Cases, LogicalFunctionsTest,
    testing::ValuesIn(std::vector<TestCase>{
        TestCase{builtin::kNot,
                 [](ValueFactory& value_factory) -> std::vector<Handle<Value>> {
                   return {value_factory.CreateBoolValue(true)};
                 },
                 IsBool(false)},
        TestCase{builtin::kNot,
                 [](ValueFactory& value_factory) -> std::vector<Handle<Value>> {
                   return {value_factory.CreateBoolValue(false)};
                 },
                 IsBool(true)},
        TestCase{builtin::kNot,
                 [](ValueFactory& value_factory) -> std::vector<Handle<Value>> {
                   return {value_factory.CreateBoolValue(true),
                           value_factory.CreateBoolValue(false)};
                 },
                 absl::InvalidArgumentError("")},
        TestCase{builtin::kNotStrictlyFalse,
                 [](ValueFactory& value_factory) -> std::vector<Handle<Value>> {
                   return {value_factory.CreateBoolValue(true)};
                 },
                 IsBool(true)},
        TestCase{builtin::kNotStrictlyFalse,
                 [](ValueFactory& value_factory) -> std::vector<Handle<Value>> {
                   return {value_factory.CreateBoolValue(false)};
                 },
                 IsBool(false)},
        TestCase{builtin::kNotStrictlyFalse,
                 [](ValueFactory& value_factory) -> std::vector<Handle<Value>> {
                   return {value_factory.CreateErrorValue(
                       absl::InternalError("test"))};
                 },
                 IsBool(true)},
        TestCase{builtin::kNotStrictlyFalse,
                 [](ValueFactory& value_factory) -> std::vector<Handle<Value>> {
                   return {value_factory.CreateUnknownValue()};
                 },
                 IsBool(true)},
        TestCase{builtin::kNotStrictlyFalse,
                 [](ValueFactory& value_factory) -> std::vector<Handle<Value>> {
                   return {value_factory.CreateIntValue(42)};
                 },
                 Truly([](const Handle<Value>& v) {
                   return v->Is<ErrorValue>() &&
                          absl::StrContains(
                              v->As<ErrorValue>().value().message(),
                              "No matching overloads");
                 })},
    }));

}  // namespace
}  // namespace cel

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

#include "extensions/strings.h"

#include <memory>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/strings/cord.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/values/legacy_value_manager.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "internal/testing.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"

namespace cel::extensions {
namespace {

using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::parser::ParserOptions;

TEST(Strings, SplitWithEmptyDelimiterCord) {
  MemoryManagerRef memory_manager = MemoryManagerRef::ReferenceCounting();
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder, CreateStandardRuntimeBuilder(options));
  EXPECT_OK(RegisterStringsFunctions(builder.function_registry(), options));

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr,
                       Parse("foo.split('') == ['h', 'e', 'l', 'l', 'o', ' ', "
                             "'w', 'o', 'r', 'l', 'd', '!']",
                             "<input>", ParserOptions{}));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  common_internal::LegacyValueManager value_factory(memory_manager,
                                                    runtime->GetTypeProvider());

  Activation activation;
  activation.InsertOrAssignValue("foo",
                                 StringValue{absl::Cord("hello world!")});

  ASSERT_OK_AND_ASSIGN(Value result,
                       program->Evaluate(activation, value_factory));
  ASSERT_TRUE(result.Is<BoolValue>());
  EXPECT_TRUE(static_cast<BoolValue>(result).NativeValue());
}

}  // namespace
}  // namespace cel::extensions

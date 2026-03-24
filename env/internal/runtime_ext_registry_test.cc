// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "env/internal/runtime_ext_registry.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/ast.h"
#include "common/source.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "parser/parser_interface.h"
#include "runtime/activation.h"
#include "runtime/function.h"
#include "runtime/function_adapter.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_builder_factory.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace cel::env_internal {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::cel::test::StringValueIs;

Value Hello1(const StringValue& input, const Function::InvokeContext& context) {
  return StringValue::From("Hello, old " + input.ToString() + "!",
                           context.arena());
}

Value Hello2(const StringValue& input, const Function::InvokeContext& context) {
  return StringValue::From("Hello, new " + input.ToString() + "!",
                           context.arena());
}

RuntimeExtensionRegistry GetRuntimeExtensionRegistry() {
  RuntimeExtensionRegistry registry;
  registry.AddFunctionRegistration(
      "hello_extension", "hello_extension_alias", 1,
      [](RuntimeBuilder& runtime_builder,
         const RuntimeOptions& runtime_options) -> absl::Status {
        return cel::UnaryFunctionAdapter<Value, StringValue>::
            RegisterGlobalOverload("hello", &Hello1,
                                   runtime_builder.function_registry());
      });
  registry.AddFunctionRegistration(
      "hello_extension", "hello_extension_alias", 2,
      [](RuntimeBuilder& runtime_builder,
         const RuntimeOptions& runtime_options) -> absl::Status {
        return cel::UnaryFunctionAdapter<Value, StringValue>::
            RegisterMemberOverload("hello", &Hello2,
                                   runtime_builder.function_registry());
      });
  return registry;
}

class RuntimeExtensionRegistryTest : public testing::Test {
 protected:
  absl::StatusOr<Value> Run(std::string_view extension_name, int version,
                            std::string_view expr) {
    const RuntimeExtensionRegistry registry = GetRuntimeExtensionRegistry();

    CEL_ASSIGN_OR_RETURN(std::unique_ptr<Parser> parser,
                         NewParserBuilder(ParserOptions())->Build());

    CEL_ASSIGN_OR_RETURN(std::unique_ptr<Source> source, NewSource(expr, ""));
    CEL_ASSIGN_OR_RETURN(std::unique_ptr<Ast> ast, parser->Parse(*source));

    auto descriptor_pool = cel::internal::GetSharedTestingDescriptorPool();
    cel::RuntimeOptions runtime_options;
    CEL_ASSIGN_OR_RETURN(
        cel::RuntimeBuilder runtime_builder,
        cel::CreateRuntimeBuilder(descriptor_pool, runtime_options));

    CEL_RETURN_IF_ERROR(registry.RegisterExtensionFunctions(
        runtime_builder, runtime_options, extension_name, version));

    CEL_ASSIGN_OR_RETURN(std::unique_ptr<Runtime> runtime,
                         std::move(runtime_builder).Build());
    CEL_ASSIGN_OR_RETURN(std::unique_ptr<Program> program,
                         runtime->CreateProgram(std::move(ast)));

    Activation activation;
    return program->Evaluate(&arena_, activation);
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_F(RuntimeExtensionRegistryTest, SpecificExtensionVersion) {
  EXPECT_THAT(Run("hello_extension", 1, "hello('world')"),
              IsOkAndHolds(StringValueIs("Hello, old world!")));
}

TEST_F(RuntimeExtensionRegistryTest, LatestExtensionVersion) {
  EXPECT_THAT(Run("hello_extension_alias", RuntimeExtensionRegistry::kLatest,
                  "'world'.hello()"),
              IsOkAndHolds(StringValueIs("Hello, new world!")));
}

}  // namespace
}  // namespace cel::env_internal

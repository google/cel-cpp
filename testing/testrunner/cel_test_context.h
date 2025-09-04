// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_TESTRUNNER_CEL_TEST_CONTEXT_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_TESTRUNNER_CEL_TEST_CONTEXT_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "cel/expr/checked.pb.h"
#include "cel/expr/value.pb.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "compiler/compiler.h"
#include "eval/public/cel_expression.h"
#include "runtime/runtime.h"
#include "testing/testrunner/cel_expression_source.h"
namespace cel::test {

// Struct to hold optional parameters for `CelTestContext`.
struct CelTestContextOptions {
  // The source for the CEL expression to be evaluated in the test.
  std::optional<CelExpressionSource> expression_source;

  // An optional CEL compiler. This is required for test cases where
  // input or output values are themselves CEL expressions that need to be
  // resolved at runtime or cel expression source is raw string or cel file.
  std::unique_ptr<const cel::Compiler> compiler = nullptr;

  // A map of variable names to values that provides default bindings for the
  // evaluation.
  //
  // These bindings can be considered context-wide defaults. If a variable name
  // exists in both these custom bindings and in a specific TestCase's input,
  // the value from the TestCase will take precedence and override this one.
  // This logic is handled by the test runner when it constructs the final
  // activation.
  absl::flat_hash_map<std::string, cel::expr::Value> custom_bindings;
};

// The context class for a CEL test, holding configurations needed to evaluate
// compiled CEL expressions.
class CelTestContext {
 public:
  // Creates a CelTestContext using a `CelExpressionBuilder`.
  //
  // The `CelExpressionBuilder` helps in setting up the environment for
  // building the CEL expression.
  //
  // Example usage:
  //
  // CEL_REGISTER_TEST_CONTEXT_FACTORY(
  //       []() -> absl::StatusOr<std::unique_ptr<CelTestContext>> {
  //     // SAFE: This setup code now runs when the lambda is invoked at
  //     runtime,
  //     // long after all static initializations are complete.
  //     auto cel_expression_builder =
  //         google::api::expr::runtime::CreateCelExpressionBuilder();
  //    CelTestContextOptions options;
  //     return CelTestContext::CreateFromCelExpressionBuilder(
  //         std::move(cel_expression_builder), std::move(options));
  //   });
  static std::unique_ptr<CelTestContext> CreateFromCelExpressionBuilder(
      std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder>
          cel_expression_builder,
      CelTestContextOptions options) {
    return absl::WrapUnique(new CelTestContext(
        std::move(cel_expression_builder), std::move(options)));
  }

  // Creates a CelTestContext using a `cel::Runtime`.
  //
  // The `cel::Runtime` is used to evaluate the CEL expression by managing
  // the state needed to generate Program.
  static std::unique_ptr<CelTestContext> CreateFromRuntime(
      std::unique_ptr<const cel::Runtime> runtime,
      CelTestContextOptions options) {
    return absl::WrapUnique(
        new CelTestContext(std::move(runtime), std::move(options)));
  }

  const cel::Runtime* absl_nullable runtime() const { return runtime_.get(); }

  const google::api::expr::runtime::CelExpressionBuilder* absl_nullable
  cel_expression_builder() const {
    return cel_expression_builder_.get();
  }

  const cel::Compiler* absl_nullable compiler() const {
    return cel_test_context_options_.compiler.get();
  }

  const CelExpressionSource* absl_nullable expression_source() const {
    return cel_test_context_options_.expression_source.has_value()
               ? &cel_test_context_options_.expression_source.value()
               : nullptr;
  }

  const absl::flat_hash_map<std::string, cel::expr::Value>&
  custom_bindings() const {
    return cel_test_context_options_.custom_bindings;
  }

 private:
  // Delete copy and move constructors.
  CelTestContext(const CelTestContext&) = delete;
  CelTestContext& operator=(const CelTestContext&) = delete;
  CelTestContext(CelTestContext&&) = delete;
  CelTestContext& operator=(CelTestContext&&) = delete;

  // Make the constructors private to enforce the use of the factory methods.
  CelTestContext(
      std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder>
          cel_expression_builder,
      CelTestContextOptions options)
      : cel_test_context_options_(std::move(options)),
        cel_expression_builder_(std::move(cel_expression_builder)) {}

  CelTestContext(std::unique_ptr<const cel::Runtime> runtime,
                 CelTestContextOptions options)
      : cel_test_context_options_(std::move(options)),
        runtime_(std::move(runtime)) {}

  // Configuration for the expression to be executed.
  CelTestContextOptions cel_test_context_options_;

  // This helps in setting up the environment for building the CEL
  // expression. Users should either provide a runtime, or the
  // CelExpressionBuilder.
  std::unique_ptr<google::api::expr::runtime::CelExpressionBuilder>
      cel_expression_builder_;

  // The runtime is used to evaluate the CEL expression by managing the state
  // needed to generate Program. Users should either provide a runtime, or the
  // CelExpressionBuilder.
  std::unique_ptr<const cel::Runtime> runtime_;
};

}  // namespace cel::test

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_TESTRUNNER_CEL_TEST_CONTEXT_H_

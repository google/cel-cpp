// Copyright 2021 Google LLC
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

#include "codelab/exercise2.h"

#include <memory>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/value.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "extensions/protobuf/type_reflector.h"
#include "internal/status_macros.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel_codelab {
namespace {

using ::cel::Activation;
using ::cel::BoolValue;
using ::cel::Cast;
using ::cel::CreateStandardRuntimeBuilder;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::Program;
using ::cel::Runtime;
using ::cel::RuntimeBuilder;
using ::cel::RuntimeOptions;
using ::cel::Value;
using ::cel::ValueManager;
using ::cel::extensions::ProtobufRuntimeAdapter;
using ::cel::extensions::ProtoTypeReflector;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::rpc::context::AttributeContext;

// Parse a cel expression and evaluate it against the given runtime and
// activation.
absl::StatusOr<bool> ParseAndEvaluate(const Runtime& runtime,
                                      absl::string_view cel_expr,
                                      const Activation& activation,
                                      ValueManager& value_manager) {
  CEL_ASSIGN_OR_RETURN(ParsedExpr parsed_expr, Parse(cel_expr));

  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<cel::Program> expression_plan,
      ProtobufRuntimeAdapter::CreateProgram(runtime, parsed_expr));

  CEL_ASSIGN_OR_RETURN(Value result,
                       expression_plan->Evaluate(activation, value_manager));

  if (InstanceOf<BoolValue>(result)) {
    return Cast<BoolValue>(result).NativeValue();
  } else if (InstanceOf<ErrorValue>(result)) {
    return Cast<ErrorValue>(result).NativeValue();
  } else {
    return absl::InvalidArgumentError(absl::StrCat(
        "expected 'bool' result got: '", result->DebugString(), "'"));
  }
}

}  // namespace

absl::StatusOr<bool> ParseAndEvaluateWithVariable(absl::string_view cel_expr,
                                                  bool bool_var) {
  RuntimeOptions options;
  CEL_ASSIGN_OR_RETURN(RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(options));

  CEL_ASSIGN_OR_RETURN(std::unique_ptr<const Runtime> runtime,
                       std::move(builder).Build());

  Activation activation;
  google::protobuf::Arena arena;
  // === Start Codelab ===
  // Update the activation to bind the bool argument to 'bool_var'
  return absl::UnimplementedError("todo");
  // === End Codelab ===
}

absl::StatusOr<bool> ParseAndEvaluateWithContext(
    absl::string_view cel_expr, const AttributeContext& context) {
  RuntimeOptions options;
  CEL_ASSIGN_OR_RETURN(RuntimeBuilder builder,
                       CreateStandardRuntimeBuilder(options));

  builder.type_registry().AddTypeProvider(
      std::make_unique<ProtoTypeReflector>());

  CEL_ASSIGN_OR_RETURN(std::unique_ptr<const Runtime> runtime,
                       std::move(builder).Build());

  Activation activation;
  google::protobuf::Arena arena;
  // === Start Codelab ===
  // Update the activation to bind the AttributeContext.
  return absl::UnimplementedError("todo");
  // === End Codelab ===
}

}  // namespace cel_codelab

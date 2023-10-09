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

#include "codelab/exercise1.h"

#include <memory>
#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "internal/status_macros.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "google/protobuf/arena.h"

namespace cel_codelab {
namespace {

using ::cel::Activation;
using ::cel::As;
using ::cel::StringValue;
using ::cel::Value;
using ::cel::ValueKindToString;

// Convert the cel::Value result to a C++ string if it is string typed.
// Otherwise, return invalid argument error. This takes a copy to avoid
// lifecycle concerns (the value may be ref counted or Arena allocated).
absl::StatusOr<std::string> ConvertResult(const Value& value) {
  if (auto string_value = As<StringValue>(value); string_value.has_value()) {
    return string_value->ToString();
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "expected string result got '", ValueKindToString(value.kind()), "'"));
}

}  // namespace

absl::StatusOr<std::string> ParseAndEvaluate(absl::string_view cel_expr) {
  // === Start Codelab ===
  // Setup a default environment for building expressions.
  // RuntimeOptions options;
  // CEL_ASSIGN_OR_RETURN(RuntimeBuilder builder,
  //                      CreateStandardRuntimeBuilder(options));

  // CEL_ASSIGN_OR_RETURN(std::unique_ptr<const Runtime> runtime,
  //                      std::move(builder).Build());

  // Parse the expression using ::google::api::expr::parser::Parse;
  // This will return a google::api::expr::v1alpha1::ParsedExpr message.

  // The evaluator uses a proto Arena for incidental allocations during
  // evaluation. A value factory associates the memory manager with the
  // appropriate type system.
  //
  // Use cel::extensions::ProtoMemoryManagerRef to adapt an Arena to work with
  // CEL's value representation.
  google::protobuf::Arena arena;
  // ManagedValueFactory value_factory(expression_plan->GetTypeProvider(),
  //                                   ProtoMemoryManagerRef(&arena));

  // The activation provides variables and functions that are bound into the
  // expression environment. In this example, there's no context expected, so
  // we just provide an empty one to the evaluator.
  Activation activation;

  // Using the Runtime and the ParseExpr, create an execution plan
  // (cel::Program), evaluate, and return the result. Use the provided helper
  // function ConvertResult to copy the value for return.
  return absl::UnimplementedError("Not yet implemented");
  // === End Codelab ===
}

}  // namespace cel_codelab

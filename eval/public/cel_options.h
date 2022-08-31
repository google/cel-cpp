/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_OPTIONS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_OPTIONS_H_

#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

// Options for unknown processing.
enum class UnknownProcessingOptions {
  // No unknown processing.
  kDisabled,
  // Only attributes supported.
  kAttributeOnly,
  // Attributes and functions supported. Function results are dependent on the
  // logic for handling unknown_attributes, so clients must opt in to both.
  kAttributeAndFunction
};

// Options for handling unset wrapper types on field access.
enum class ProtoWrapperTypeOptions {
  // Default: legacy behavior following proto semantics (unset behaves as though
  // it is set to default value).
  kUnsetProtoDefault,
  // CEL spec behavior, unset wrapper is treated as a null value when accessed.
  kUnsetNull,
};

// Interpreter options for controlling evaluation and builtin functions.
struct InterpreterOptions {
  // Level of unknown support enabled.
  UnknownProcessingOptions unknown_processing =
      UnknownProcessingOptions::kDisabled;

  bool enable_missing_attribute_errors = false;

  // Enable timestamp duration overflow checks.
  //
  // The CEL-Spec indicates that overflow should occur outside the range of
  // string-representable timestamps, and at the limit of durations which can be
  // expressed with a single int64_t value.
  bool enable_timestamp_duration_overflow_errors = false;

  // Enable short-circuiting of the logical operator evaluation. If enabled,
  // AND, OR, and TERNARY do not evaluate the entire expression once the the
  // resulting value is known from the left-hand side.
  bool short_circuiting = true;

  // Enable constant folding during the expression creation. If enabled,
  // an arena must be provided for constant generation.
  // Note that expression tracing applies a modified expression if this option
  // is enabled.
  bool constant_folding = false;
  google::protobuf::Arena* constant_arena = nullptr;

  // Enable comprehension expressions (e.g. exists, all)
  bool enable_comprehension = true;

  // Set maximum number of iterations in the comprehension expressions if
  // comprehensions are enabled. The limit applies globally per an evaluation,
  // including the nested loops as well. Use value 0 to disable the upper bound.
  int comprehension_max_iterations = 10000;

  // Enable list append within comprehensions. Note, this option is not safe
  // with hand-rolled ASTs.
  int enable_comprehension_list_append = false;

  // Enable RE2 match() overload.
  bool enable_regex = true;

  // Set maximum program size for RE2 regex if regex overload is enabled.
  // Evaluates to an error if a regex exceeds it. Use value 0 to disable the
  // upper bound.
  int regex_max_program_size = 0;

  // Enable string() overloads.
  bool enable_string_conversion = true;

  // Enable string concatenation overload.
  bool enable_string_concat = true;

  // Enable list concatenation overload.
  bool enable_list_concat = true;

  // Enable list membership overload.
  bool enable_list_contains = true;

  // Treat builder warnings as fatal errors.
  bool fail_on_warnings = true;

  // Enable the resolution of qualified type identifiers as type values instead
  // of field selections.
  //
  // This toggle may cause certain identifiers which overlap with CEL built-in
  // type or with protobuf message types linked into the binary to be resolved
  // as static type values rather than as per-eval variables.
  bool enable_qualified_type_identifiers = false;

  // Enable a check for memory vulnerabilities within comprehension
  // sub-expressions.
  //
  // Note: This flag is not necessary if you are only using Core CEL macros.
  //
  // Consider enabling this feature when using custom comprehensions, and
  // absolutely enable the feature when using hand-written ASTs for
  // comprehension expressions.
  bool enable_comprehension_vulnerability_check = false;

  // Enable coercing null cel values to messages in function resolution. This
  // allows extension functions that previously depended on representing null
  // values as nullptr messages to function.
  //
  // Note: This will be disabled by default in the future after clients that
  // depend on the legacy function resolution are identified.
  bool enable_null_to_message_coercion = true;

  // Enable heterogeneous comparisons (e.g. support for cross-type comparisons).
  bool enable_heterogeneous_equality = true;

  // Enables unwrapping proto wrapper types to null if unset. e.g. if an
  // expression access a field of type google.protobuf.Int64Value that is unset,
  // that will result in a Null cel value, as opposed to returning the
  // cel representation of the proto defined default int64_t: 0.
  bool enable_empty_wrapper_null_unboxing = false;

  // Enables expression rewrites to disambiguate namespace qualified identifiers
  // from container access for variables and receiver-style calls for functions.
  //
  // Note: This makes an implicit copy of the input expression for lifetime
  // safety.
  bool enable_qualified_identifier_rewrites = false;

  // Historically regular expressions were compiled on each invocation to
  // `matches` and not re-used, even if the regular expression is a constant.
  // Enabling this option causes constant regular expressions to be compiled
  // ahead-of-time and re-used for each invocation to `matches`. A side effect
  // of this is that invalid regular expressions will result in errors when
  // building an expression.
  //
  // It is recommended that this option be enabled in conjunction with
  // enable_constant_folding.
  //
  // Note: In most cases enabling this option is safe, however to perform this
  // optimization overloads are not consulted for applicable calls. If you have
  // overriden the default `matches` function you should not enable this option.
  bool enable_regex_precompilation = false;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_OPTIONS_H_

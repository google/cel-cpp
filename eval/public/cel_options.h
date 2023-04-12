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
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

using UnknownProcessingOptions = cel::UnknownProcessingOptions;

using ProtoWrapperTypeOptions = cel::ProtoWrapperTypeOptions;

struct InterpreterOptions : public cel::RuntimeOptions {
  // Enable constant folding during the expression creation. If enabled,
  // an arena must be provided for constant generation.
  // Note that expression tracing applies a modified expression if this option
  // is enabled.
  bool constant_folding = false;
  google::protobuf::Arena* constant_arena = nullptr;

  // Enable a check for memory vulnerabilities within comprehension
  // sub-expressions.
  //
  // Note: This flag is not necessary if you are only using Core CEL macros.
  //
  // Consider enabling this feature when using custom comprehensions, and
  // absolutely enable the feature when using hand-written ASTs for
  // comprehension expressions.
  bool enable_comprehension_vulnerability_check = false;

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

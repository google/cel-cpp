// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_FUNCTION_H_
#define THIRD_PARTY_CEL_CPP_BASE_FUNCTION_H_

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "base/handle.h"
#include "base/value.h"
#include "base/value_factory.h"

namespace cel {

// Interface for extension functions.
//
// The host for the CEL environment may provide implementations to define custom
// extensions functions.
//
// The interpreter expects functions to be deterministic and side-effect free.
class Function {
 public:
  virtual ~Function() = default;

  // InvokeContext provides access to current evaluator state.
  class InvokeContext final {
   public:
    explicit InvokeContext(ValueFactory& value_factory)
        : value_factory_(value_factory) {}

    // Return the value_factory defined for the evaluation invoking the
    // extension function.
    cel::ValueFactory& value_factory() const { return value_factory_; }

    // TODO(uncreated-issue/24): Add accessors for getting attribute stack and mutable
    // value stack.
   private:
    cel::ValueFactory& value_factory_;
  };

  // Attempt to evaluate an extension function based on the runtime arguments
  // during the evaluation of a CEL expression.
  //
  // A non-ok status is interpreted as an unrecoverable error in evaluation (
  // e.g. data corruption). This stops evaluation and is propagated immediately.
  //
  // A cel::ErrorValue typed result is considered a recoverable error and
  // follows CEL's logical short-circuiting behavior.
  virtual absl::StatusOr<Handle<Value>> Invoke(
      const InvokeContext& context,
      absl::Span<const Handle<Value>> args) const = 0;
};

// Legacy type, aliased to the actual type.
using FunctionEvaluationContext = Function::InvokeContext;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_FUNCTION_H_

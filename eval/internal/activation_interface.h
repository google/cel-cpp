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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_ACTIVATION_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_ACTIVATION_INTERFACE_H_

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "base/memory_manager.h"
#include "base/value.h"

namespace cel::interop_internal {

// Interface for providing runtime with variable lookups.
// TODO(issues/5): Add support for lazily bound / context functions.
// TODO(issues/5): After finalizing, make this public and add instructions
// for clients to migrate.
class ActivationInterface {
 public:
  virtual ~ActivationInterface() = default;

  // Resolve a string (possibly qualified) variable name.
  virtual absl::optional<Handle<Value>> ResolveVariable(
      MemoryManager& manager, absl::string_view name) const = 0;

  // Return a list of unknown attribute patterns.
  //
  // If an attribute (select path) encountered during evaluation matches any of
  // the patterns, the value will be treated as unknown and propagated in an
  // unknown set.
  //
  // The returned span must remain valid for the duration of any evaluation
  // using this this activation.
  virtual absl::Span<const cel::AttributePattern> GetUnknownAttributes()
      const = 0;

  // Return a list of missing attribute patterns.
  //
  // If an attribute (select path) encountered during evaluation matches any of
  // the patterns, the value will be treated as missing and propagated as an
  // error.
  //
  // The returned span must remain valid for the duration of any evaluation
  // using this activation.
  virtual absl::Span<const cel::AttributePattern> GetMissingAttributes()
      const = 0;
};

}  // namespace cel::interop_internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_ACTIVATION_INTERFACE_H_

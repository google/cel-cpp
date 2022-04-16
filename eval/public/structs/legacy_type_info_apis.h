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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_INFO_APIS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_INFO_APIS_H_

#include <string>

#include "eval/public/cel_value_internal.h"

namespace google::api::expr::runtime {

// Forward declared to resolve cyclic dependency.
class LegacyTypeAccessApis;

// Interface for providing type info from a user defined type (represented as a
// message).
//
// Provides ability to obtain field access apis, type info, and debug
// representation of a message.
//
// This is implemented as a separate class from LegacyTypeAccessApis to resolve
// cyclic dependency between CelValue (which needs to access these apis to
// provide DebugString and ObtainCelTypename) and LegacyTypeAccessApis (which
// needs to return CelValue type for field access).
class LegacyTypeInfoApis {
 public:
  virtual ~LegacyTypeInfoApis() = default;

  // Return a debug representation of the wrapped message.
  virtual std::string DebugString(
      const internal::MessageWrapper& wrapped_message) const = 0;

  // Return a const-reference to the typename for the wrapped message's type.
  // The CEL interpreter assumes that the typename is owned externally and will
  // outlive any CelValues created by the interpreter.
  virtual const std::string& GetTypename(
      const internal::MessageWrapper& wrapped_message) const = 0;

  // Return a pointer to the wrapped message's access api implementation.
  // The CEL interpreter assumes that the  is owned externally and will
  // outlive any CelValues created by the interpreter.
  // Nullptr means the value does not provide access apis.
  virtual const LegacyTypeAccessApis* GetAccessApis(
      const internal::MessageWrapper& wrapped_message) const = 0;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_INFO_APIS_H_

// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_INFERENCE_CONTEXT_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_INFERENCE_CONTEXT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/decl.h"
#include "common/type.h"
#include "google/protobuf/arena.h"

namespace cel::checker_internal {

// Class manages context for type inferences in the type checker.
// TODO: for now, just checks assignability for concrete types.
// Support for finding substitutions of type parameters will be added in a
// follow-up CL.
class TypeInferenceContext {
 public:
  // Convenience alias for an instance map for type parameters mapped to type
  // vars in a given context.
  //
  // This should be treated as opaque, the client should not manually modify.
  using InstanceMap = absl::flat_hash_map<std::string, absl::string_view>;

  struct OverloadResolution {
    Type result_type;
    std::vector<OverloadDecl> overloads;
  };

  explicit TypeInferenceContext(google::protobuf::Arena* arena) : arena_(arena) {}

  Type InstantiateTypeParams(const Type& type);

  // Overload for function overload types that need coordination across
  // multiple function parameters.
  Type InstantiateTypeParams(const Type& type, InstanceMap& substitutions);

  // Resolves the applicable overloads for the given function call given the
  // inferred argument types.
  //
  // If found, returns the result type and the list of applicable overloads.
  absl::optional<OverloadResolution> ResolveOverload(
      const FunctionDecl& decl, absl::Span<const Type> argument_types,
      bool is_receiver);

  // Checks if `instance` is assignable to `parameter`.
  bool IsAssignable(const Type& parameter, const Type& instance);

 private:
  absl::string_view NewTypeVar() {
    next_type_parameter_id_++;
    auto inserted = type_parameter_bindings_.insert(
        {absl::StrCat("T%", next_type_parameter_id_), absl::nullopt});
    ABSL_DCHECK(inserted.second);
    return inserted.first->first;
  }

  // Returns true if the two types are equivalent with the current type
  // substitutions.
  bool TypeEquivalent(Type a, Type b);

  // Map from type var parameter name to the type it is bound to.
  // Type var parameters are formatted as "T%<id>".
  // node_hash_map is used to preserve pointer stability for use with
  // TypeParamType.
  // Type parameter instances should be resolved to a concrete type during type
  // checking to remove the lifecycle dependency on the inference context
  // instance.
  // nullopt signifies a free type variable.
  absl::node_hash_map<std::string, absl::optional<Type>>
      type_parameter_bindings_;
  int64_t next_type_parameter_id_ = 0;
  google::protobuf::Arena* arena_;
};

}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_INFERENCE_CONTEXT_H_

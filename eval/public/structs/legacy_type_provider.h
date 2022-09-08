// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_TYPE_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_TYPE_PROVIDER_H_

#include "absl/types/optional.h"
#include "base/type_provider.h"
#include "eval/public/structs/legacy_type_adapter.h"

namespace google::api::expr::runtime {

// An internal extension of cel::TypeProvider that also deals with legacy types.
//
// Note: This API is not finalized. Consult the CEL team before introducing new
// implementations.
class LegacyTypeProvider : public cel::TypeProvider {
 public:
  // Return LegacyTypeAdapter for the fully qualified type name if available.
  //
  // nullopt values are interpreted as not present.
  //
  // Returned non-null pointers from the adapter implemententation must remain
  // valid as long as the type provider.
  // TODO(issues/5): add alternative for new type system.
  virtual absl::optional<LegacyTypeAdapter> ProvideLegacyType(
      absl::string_view name) const = 0;

  // Return LegacyTypeInfoApis for the fully qualified type name if available.
  //
  // nullopt values are interpreted as not present.
  //
  // Since custom type providers should create values compatible with evaluator
  // created ones, the TypeInfoApis returned from this method should be the same
  // as the ones used in value creation.
  virtual absl::optional<const LegacyTypeInfoApis*> ProvideLegacyTypeInfo(
      absl::string_view name) const {
    return absl::nullopt;
  }
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_TYPE_PROVIDER_H_

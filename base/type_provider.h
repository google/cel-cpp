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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPE_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPE_PROVIDER_H_

#include "absl/strings/string_view.h"
#include "eval/public/structs/legacy_type_adapter.h"

namespace cel {

// Interface for a TypeProvider, allowing host applications to inject
// functionality for operating on custom types in the CEL interpreter.
//
// Type providers are registered with a TypeRegistry. When resolving a type,
// the registry will check if it is a well known type, then check against each
// of the registered providers. If the type can't be resolved, the operation
// will result in an error.
//
// Note: This API is not finalized. Consult the CEL team before introducing new
// implementations.
class TypeProvider {
 public:
  virtual ~TypeProvider() = default;

  // Return LegacyTypeAdapter for the fully qualified type name if available.
  //
  // nullopt values are interpreted as not present.
  //
  // Returned non-null pointers from the adapter implemententation must remain
  // valid as long as the type provider.
  // TODO(issues/5): add alternative for new type system.
  virtual absl::optional<google::api::expr::runtime::LegacyTypeAdapter>
  ProvideLegacyType(absl::string_view name) const = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_PROVIDER_H_

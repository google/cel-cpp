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

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/type.h"

namespace cel {

class TypeFactory;

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
  // Returns a TypeProvider which provides all of CEL's builtin types. It is
  // thread safe.
  ABSL_ATTRIBUTE_PURE_FUNCTION static TypeProvider& Builtin();

  virtual ~TypeProvider() = default;

  // Return a persistent handle to a Type for the fully qualified type name, if
  // available.
  //
  // An empty handle is returned if the provider cannot find the requested type.
  virtual absl::StatusOr<Persistent<const Type>> ProvideType(
      TypeFactory&, absl::string_view) const {
    return absl::UnimplementedError("ProvideType is not yet implemented");
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_PROVIDER_H_

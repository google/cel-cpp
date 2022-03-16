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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_ADPATER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_ADPATER_H_

#include "absl/status/status.h"
#include "base/memory_manager.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// Type information about a legacy Struct type.
// Provides methods to the interpreter for interacting with a custom type.
//
// This provides Apis for emulating the behavior of new types working on
// existing cel values.
//
// MutationApis provide equivalent behavior to a cel::Type and cel::ValueFactory
// (resolved from a type name).
//
// AccessApis provide equivalent behavior to cel::StructValue accessors (virtual
// dispatch to a concrete implementation for accessing underlying values).
//
// This class is a simple wrapper around (nullable) pointers to the interface
// implementations. The underlying pointers are expected to be valid as long as
// the type provider that returned this object.
class LegacyTypeAdapter {
 public:
  // Interface for mutation apis.
  // Note: in the new type system, the provider represents this by returning
  // a cel::Type and cel::ValueFactory for the type.
  class MutationApis {
   public:
    virtual ~MutationApis() = default;

    // Return whether the type defines the given field.
    // TODO(issues/5): This is only used to eagerly fail during the planning
    // phase. Check if it's safe to remove this behavior and fail at runtime.
    virtual bool DefinesField(absl::string_view field_name) const = 0;

    // Create a new empty instance of the type.
    // May return a status if the type is not possible to create.
    virtual absl::StatusOr<CelValue> NewInstance(
        cel::MemoryManager& memory_manager) const = 0;

    // Normalize special types to a native CEL value after building.
    // The default implementation is a no-op.
    // The interpreter guarantees that instance is uniquely owned by the
    // interpreter, and can be safely mutated.
    virtual absl::Status AdaptFromWellKnownType(
        cel::MemoryManager& memory_manager, CelValue& instance) const {
      return absl::OkStatus();
    }

    // Set field on instance to value.
    // The interpreter guarantees that instance is uniquely owned by the
    // interpreter, and can be safely mutated.
    virtual absl::Status SetField(absl::string_view field_name,
                                  const CelValue& value,
                                  cel::MemoryManager& memory_manager,
                                  CelValue& instance) const = 0;
  };

  // Interface for access apis.
  // Note: in new type system this is integrated into the StructValue (via
  // dynamic dispatch to concerete implementations).
  class AccessApis {
   public:
    virtual ~AccessApis() = default;

    // Return whether an instance of the type has field set to a non-default
    // value.
    virtual absl::StatusOr<bool> HasField(absl::string_view field_name,
                                          const CelValue& value) const = 0;

    // Access field on instance.
    virtual absl::StatusOr<CelValue> GetField(
        absl::string_view field_name, const CelValue& instance,
        cel::MemoryManager& memory_manager) const = 0;
  };

  LegacyTypeAdapter(const AccessApis* access, const MutationApis* mutation)
      : access_apis_(access), mutation_apis_(mutation) {}

  // Apis for access for the represented type.
  // If null, access is not supported (this is an opaque type).
  const AccessApis* access_apis() { return access_apis_; }

  // Apis for mutation for the represented type.
  // If null, mutation is not supported (this type cannot be created).
  const MutationApis* mutation_apis() { return mutation_apis_; }

 private:
  const AccessApis* access_apis_;
  const MutationApis* mutation_apis_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_TYPE_ADPATER_H_

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

#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "eval/public/structs/legacy_any_packing.h"
#include "eval/public/structs/legacy_type_adapter.h"

namespace google::api::expr::runtime {

// An internal extension of cel::TypeProvider that also deals with legacy types.
//
// Note: This API is not finalized. Consult the CEL team before introducing new
// implementations.
class LegacyTypeProvider : public cel::TypeReflector {
 public:
  virtual ~LegacyTypeProvider() = default;

  // Return LegacyTypeAdapter for the fully qualified type name if available.
  //
  // nullopt values are interpreted as not present.
  //
  // Returned non-null pointers from the adapter implemententation must remain
  // valid as long as the type provider.
  // TODO(uncreated-issue/3): add alternative for new type system.
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
      ABSL_ATTRIBUTE_UNUSED absl::string_view name) const {
    return absl::nullopt;
  }

  // Return LegacyAnyPackingApis for the fully qualified type name if available.
  // It is only used by CreateCelValue/CreateMessageFromValue functions from
  // cel_proto_lite_wrap_util. It is not directly used by the runtime, but may
  // be needed in a TypeProvider implementation.
  //
  // nullopt values are interpreted as not present.
  //
  // Returned non-null pointers must remain valid as long as the type provider.
  // TODO(uncreated-issue/19): Move protobuf-Any API from top level
  // [Legacy]TypeProviders.
  virtual absl::optional<const LegacyAnyPackingApis*>
  ProvideLegacyAnyPackingApis(
      ABSL_ATTRIBUTE_UNUSED absl::string_view name) const {
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<cel::Unique<cel::StructValueBuilder>>>
  NewStructValueBuilder(cel::ValueFactory& value_factory,
                        cel::StructTypeView type) const final;

 protected:
  absl::StatusOr<absl::optional<cel::Value>> DeserializeValueImpl(
      cel::ValueFactory& value_factory, absl::string_view type_url,
      const absl::Cord& value) const final;

  absl::StatusOr<absl::optional<cel::TypeView>> FindTypeImpl(
      cel::TypeFactory& type_factory, absl::string_view name,
      cel::Type& scratch) const final;

  absl::StatusOr<absl::optional<cel::StructTypeFieldView>>
  FindStructTypeFieldByNameImpl(cel::TypeFactory& type_factory,
                                absl::string_view type, absl::string_view name,
                                cel::StructTypeField& scratch) const final;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_TYPE_PROVIDER_H_

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
#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_COMPOSED_TYPE_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_COMPOSED_TYPE_PROVIDER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/type_provider.h"

namespace cel::runtime_internal {

// Type provider implementation managed by the runtime type registry.
//
// Maintains ownership of client provided type provider implementations and
// delegates type resolution to them in order. To meet the requirements for use
// with TypeManager, this should not be updated after any call to ProvideType.
//
// The builtin type provider is implicitly consulted first in a type manager,
// so it is not represented here.
class ComposedTypeProvider : public TypeProvider {
 public:
  // Register an additional type provider.
  void AddTypeProvider(std::unique_ptr<TypeProvider> provider) {
    providers_.push_back(std::move(provider));
  }

  absl::StatusOr<absl::optional<Unique<StructValueBuilder>>>
  NewStructValueBuilder(ValueFactory& value_factory,
                        StructTypeView type) const override;

  absl::StatusOr<absl::optional<ValueView>> FindValue(
      ValueFactory& value_factory, absl::string_view name,
      Value& scratch) const override;

 protected:
  absl::StatusOr<absl::optional<Value>> DeserializeValueImpl(
      ValueFactory& value_factory, absl::string_view type_url,
      const absl::Cord& value) const override;

  absl::StatusOr<absl::optional<TypeView>> FindTypeImpl(
      TypeFactory& type_factory, absl::string_view name,
      Type& scratch) const override;

  absl::StatusOr<absl::optional<StructTypeFieldView>>
  FindStructTypeFieldByNameImpl(TypeFactory& type_factory,
                                absl::string_view type, absl::string_view name,
                                StructTypeField& scratch) const override;

  // Implements TypeProvider
 private:
  std::vector<std::unique_ptr<TypeProvider>> providers_;
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_COMPOSED_TYPE_PROVIDER_H_

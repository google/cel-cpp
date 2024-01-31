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
#include "runtime/internal/composed_type_provider.h"

#include <memory>

#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "base/handle.h"
#include "base/type.h"
#include "internal/status_macros.h"

namespace cel::runtime_internal {

absl::StatusOr<absl::optional<Unique<StructValueBuilder>>>
ComposedTypeProvider::NewStructValueBuilder(ValueFactory& value_factory,
                                            StructTypeView type) const {
  for (const std::unique_ptr<TypeProvider>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(auto builder,
                         provider->NewStructValueBuilder(value_factory, type));
    if (builder.has_value()) {
      return builder;
    }
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<ValueView>> ComposedTypeProvider::FindValue(
    ValueFactory& value_factory, absl::string_view name, Value& scratch) const {
  for (const std::unique_ptr<TypeProvider>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(auto value,
                         provider->FindValue(value_factory, name, scratch));
    if (value.has_value()) {
      return value;
    }
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<Value>>
ComposedTypeProvider::DeserializeValueImpl(ValueFactory& value_factory,
                                           absl::string_view type_url,
                                           const absl::Cord& value) const {
  for (const std::unique_ptr<TypeProvider>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(auto result, provider->DeserializeValue(
                                          value_factory, type_url, value));
    if (result.has_value()) {
      return result;
    }
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<TypeView>> ComposedTypeProvider::FindTypeImpl(
    TypeFactory& type_factory, absl::string_view name, Type& scratch) const {
  for (const std::unique_ptr<TypeProvider>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(auto result,
                         provider->FindType(type_factory, name, scratch));
    if (result.has_value()) {
      return result;
    }
  }
  return absl::nullopt;
}

absl::StatusOr<absl::optional<StructTypeFieldView>>
ComposedTypeProvider::FindStructTypeFieldByNameImpl(
    TypeFactory& type_factory, absl::string_view type, absl::string_view name,
    StructTypeField& scratch) const {
  for (const std::unique_ptr<TypeProvider>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(auto result, provider->FindStructTypeFieldByName(
                                          type_factory, type, name, scratch));
    if (result.has_value()) {
      return result;
    }
  }
  return absl::nullopt;
}

}  // namespace cel::runtime_internal

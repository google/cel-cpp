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

absl::StatusOr<absl::optional<Handle<Type>>> ComposedTypeProvider::ProvideType(
    TypeFactory& factory, absl::string_view name) const {
  for (const std::unique_ptr<TypeProvider>& provider : providers_) {
    CEL_ASSIGN_OR_RETURN(absl::optional<Handle<Type>> type,
                         provider->ProvideType(factory, name));
    if (type.has_value()) {
      return type;
    }
  }
  return absl::nullopt;
}

}  // namespace cel::runtime_internal

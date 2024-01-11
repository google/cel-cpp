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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_MANAGER_H_

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/type_provider.h"

namespace cel {

// `TypeManager` is an additional layer on top of `TypeFactory` and
// `TypeProvider` which combines the two and adds additional functionality.
class TypeManager : public virtual TypeFactory {
 public:
  virtual ~TypeManager() = default;

  // See `TypeProvider::FindType`.
  absl::StatusOr<absl::optional<TypeView>> FindType(
      absl::string_view name, Type& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return GetTypeProvider().FindType(*this, name, scratch);
  }

  // See `TypeProvider::FindStructTypeFieldByName`.
  absl::StatusOr<absl::optional<StructTypeFieldView>> FindStructTypeFieldByName(
      absl::string_view type, absl::string_view name,
      StructTypeField& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return GetTypeProvider().FindStructTypeFieldByName(*this, type, name,
                                                       scratch);
  }

  // See `TypeProvider::FindStructTypeFieldByName`.
  absl::StatusOr<absl::optional<StructTypeFieldView>> FindStructTypeFieldByName(
      StructTypeView type, absl::string_view name,
      StructTypeField& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return GetTypeProvider().FindStructTypeFieldByName(*this, type, name,
                                                       scratch);
  }

 protected:
  virtual TypeProvider& GetTypeProvider() const = 0;
};

// Creates a new `TypeManager` which is thread compatible.
Shared<TypeManager> NewThreadCompatibleTypeManager(
    MemoryManagerRef memory_manager, Shared<TypeProvider> type_provider);

// Creates a new `TypeManager` which is thread safe if and only if the provided
// `TypeFactory` and `TypeProvider` are also thread safe.
Shared<TypeManager> NewThreadSafeTypeManager(
    MemoryManagerRef memory_manager, Shared<TypeProvider> type_provider);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_MANAGER_H_

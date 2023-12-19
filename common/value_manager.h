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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_MANAGER_H_

#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/type_manager.h"
#include "common/type_provider.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_provider.h"

namespace cel {

// `ValueManager` is an additional layer on top of `ValueFactory` and
// `ValueProvider` which combines the two and adds additional functionality.
class ValueManager : public virtual TypeManager {
 public:
  TypeFactory& GetTypeFactory() const final { return GetValueFactory(); }

  TypeProvider& GetTypeProvider() const final { return GetValueProvider(); }

  virtual ValueFactory& GetValueFactory() const = 0;

  virtual ValueProvider& GetValueProvider() const = 0;

  // See `ValueProvider::NewListValueBuilder`.
  absl::StatusOr<Unique<ListValueBuilder>> NewListValueBuilder(ListType type) {
    return GetValueProvider().NewListValueBuilder(GetValueFactory(),
                                                  std::move(type));
  }

  // See `ValueProvider::NewMapValueBuilder`.
  absl::StatusOr<Unique<MapValueBuilder>> NewMapValueBuilder(MapType type) {
    return GetValueProvider().NewMapValueBuilder(GetValueFactory(),
                                                 std::move(type));
  }

  // See `ValueProvider::NewStructValueBuilder`.
  absl::StatusOr<Unique<StructValueBuilder>> NewStructValueBuilder(
      StructType type) {
    return GetValueProvider().NewStructValueBuilder(GetValueFactory(),
                                                    std::move(type));
  }

  // See `ValueProvider::NewValueBuilder`.
  absl::StatusOr<Unique<ValueBuilder>> NewValueBuilder(absl::string_view name) {
    return GetValueProvider().NewValueBuilder(GetValueFactory(), name);
  }

  // See `ValueProvider::FindValue`.
  absl::StatusOr<ValueView> FindValue(
      absl::string_view name, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    return GetValueProvider().FindValue(GetValueFactory(), name, scratch);
  }
};

// Creates a new `ValueManager` which is thread compatible.
Shared<ValueManager> NewThreadCompatibleValueManager(
    MemoryManagerRef memory_manager, Shared<ValueFactory> value_factory,
    Shared<ValueProvider> value_provider);

// Creates a new `ValueManager` which is thread safe if and only if the provided
// `ValueFactory` and `ValueProvider` are also thread safe.
Shared<ValueManager> NewThreadSafeValueManager(
    MemoryManagerRef memory_manager, Shared<ValueFactory> value_factory,
    Shared<ValueProvider> value_provider);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_MANAGER_H_

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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_PROVIDER_H_

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_provider.h"
#include "common/value.h"
#include "common/value_factory.h"

namespace cel {

// `ValueProvider` is an interface for constructing new instances of types are
// runtime. It handles type reflection.
class ValueProvider : public virtual TypeProvider {
 public:
  // Returns a `MemoryManagerRef` which is used to manage memory for the value
  // provider itself, its internal data structures, as well as for builders. The
  // builders are managed by the value provider's memory manager, but the values
  // produced by the builders are managed by the value factory's memory manager.
  using TypeProvider::GetMemoryManager;

  // `NewListValueBuilder` returns a new `ListValueBuilderInterface` for the
  // corresponding `ListType` `type`.
  absl::StatusOr<Unique<ListValueBuilder>> NewListValueBuilder(
      ValueFactory& value_factory, ListTypeView type);

  // `NewMapValueBuilder` returns a new `MapValueBuilderInterface` for the
  // corresponding `MapType` `type`.
  absl::StatusOr<Unique<MapValueBuilder>> NewMapValueBuilder(
      ValueFactory& value_factory, MapTypeView type);

  // `NewStructValueBuilder` returns a new `StructValueBuilder` for the
  // corresponding `StructType` `type`.
  virtual absl::StatusOr<Unique<StructValueBuilder>> NewStructValueBuilder(
      ValueFactory& value_factory, StructTypeView type) = 0;

  // `NewValueBuilder` returns a new `ValueBuilder` for the corresponding type
  // `name`.  It is primarily used to handle wrapper types which sometimes show
  // up literally in expressions.
  virtual absl::StatusOr<Unique<ValueBuilder>> NewValueBuilder(
      ValueFactory& value_factory, absl::string_view name) = 0;

  // `FindValue` returns a new `Value` for the corresponding name `name`. This
  // can be used to translate enum names to numeric values.
  virtual absl::StatusOr<ValueView> FindValue(
      ValueFactory& value_factory, absl::string_view name,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) = 0;
};

Shared<ValueProvider> NewThreadCompatibleValueProvider(
    MemoryManagerRef memory_manager);

Shared<ValueProvider> NewThreadSafeValueProvider(
    MemoryManagerRef memory_manager);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_PROVIDER_H_

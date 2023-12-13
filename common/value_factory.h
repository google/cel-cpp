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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_FACTORY_H_

#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"

namespace cel {

// `ValueFactory` is the preferred way for constructing values.
class ValueFactory {
 public:
  virtual ~ValueFactory() = default;

  // Returns a `MemoryManagerRef` which is used to manage memory for internal
  // data structures as well as created values.
  virtual MemoryManagerRef GetMemoryManager() const = 0;

  // `CreateZeroListValue` returns an empty `ListValue` with the given type
  // `type`.
  ListValue CreateZeroListValue(ListTypeView type);

  // `CreateZeroMapValue` returns an empty `MapTypeView` with the given type
  // `type`.
  MapValue CreateZeroMapValue(MapTypeView type);

  // `CreateZeroOptionalValue` returns an empty `OptionalValue` with the given
  // type `type`.
  OptionalValue CreateZeroOptionalValue(OptionalTypeView type);

  // `GetDynListType` gets a view of the `ListType` type `list(dyn)`.
  ListValueView GetZeroDynListValue();

  // `GetDynDynMapType` gets a view of the `MapType` type `map(dyn, dyn)`.
  MapValueView GetZeroDynDynMapValue();

  // `GetDynDynMapType` gets a view of the `MapType` type `map(string, dyn)`.
  MapValueView GetZeroStringDynMapValue();

  // `GetDynOptionalType` gets a view of the `OptionalType` type
  // `optional(dyn)`.
  OptionalValueView GetZeroDynOptionalValue();

 private:
  virtual ListValue CreateZeroListValueImpl(ListTypeView type) = 0;

  virtual MapValue CreateZeroMapValueImpl(MapTypeView type) = 0;

  virtual OptionalValue CreateZeroOptionalValueImpl(OptionalTypeView type) = 0;
};

// Creates a new `ValueFactory` which is thread compatible. The returned
// `ValueFactory` and all values it creates are managed my `memory_manager`.
Shared<ValueFactory> NewThreadCompatibleValueFactory(
    MemoryManagerRef memory_manager);

// Creates a new `ValueFactory` which is thread safe. The returned
// `ValueFactory` and all values it creates are managed my `memory_manager`.
Shared<ValueFactory> NewThreadSafeValueFactory(MemoryManagerRef memory_manager);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_FACTORY_H_

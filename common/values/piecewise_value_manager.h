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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PIECEWISE_VALUE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PIECEWISE_VALUE_MANAGER_H_

#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/sized_input_view.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_manager.h"

namespace cel::common_internal {

// `PiecewiseValueManager` is an implementation of `ValueManager` which is
// implemented by forwarding to other implementations of `TypeReflector` and
// `ValueFactory`.
class PiecewiseValueManager final : public ValueManager {
 public:
  PiecewiseValueManager(const TypeReflector& type_reflector,
                        ValueFactory& value_factory)
      : type_reflector_(type_reflector), value_factory_(value_factory) {}

  MemoryManagerRef GetMemoryManager() const override {
    return value_factory_.GetMemoryManager();
  }

 protected:
  const TypeIntrospector& GetTypeIntrospector() const override {
    return type_reflector_;
  }

  const TypeReflector& GetTypeReflector() const override {
    return type_reflector_;
  }

 private:
  ListType CreateListTypeImpl(TypeView element) override {
    return value_factory_.CreateListTypeImpl(element);
  }

  MapType CreateMapTypeImpl(TypeView key, TypeView value) override {
    return value_factory_.CreateMapTypeImpl(key, value);
  }

  StructType CreateStructTypeImpl(absl::string_view name) override {
    return value_factory_.CreateStructTypeImpl(name);
  }

  OpaqueType CreateOpaqueTypeImpl(
      absl::string_view name,
      const SizedInputView<TypeView>& parameters) override {
    return value_factory_.CreateOpaqueTypeImpl(name, parameters);
  }

  ListValue CreateZeroListValueImpl(ListTypeView type) override {
    return value_factory_.CreateZeroListValueImpl(type);
  }

  MapValue CreateZeroMapValueImpl(MapTypeView type) override {
    return value_factory_.CreateZeroMapValueImpl(type);
  }

  OptionalValue CreateZeroOptionalValueImpl(OptionalTypeView type) override {
    return value_factory_.CreateZeroOptionalValueImpl(type);
  }

  const TypeReflector& type_reflector_;
  ValueFactory& value_factory_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PIECEWISE_VALUE_MANAGER_H_

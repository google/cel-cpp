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
#include "common/value.h"

namespace cel {

// `ValueProvider` is an interface for constructing new instances of types are
// runtime. It handles type reflection.
class ValueProvider {
 public:
  virtual ~ValueProvider() = default;

  // `NewListValueBuilder` returns a new `ListValueBuilderInterface` for the
  // corresponding `ListType` `type`.
  virtual absl::StatusOr<Unique<ListValueBuilderInterface>> NewListValueBuilder(
      ListType type) = 0;

  // `NewMapValueBuilder` returns a new `MapValueBuilderInterface` for the
  // corresponding `MapType` `type`.
  virtual absl::StatusOr<Unique<MapValueBuilderInterface>> NewMapValueBuilder(
      MapType type) = 0;

  // `NewStructValueBuilder` returns a new `StructValueBuilder` for the
  // corresponding `StructType` `type`.
  virtual absl::StatusOr<Unique<StructValueBuilder>> NewStructValueBuilder(
      StructType type) = 0;

  // `NewValueBuilder` returns a new `ValueBuilder` for the corresponding type
  // `name`.  It is primarily used to handle wrapper types which sometimes show
  // up literally in expressions.
  virtual absl::StatusOr<Unique<ValueBuilder>> NewValueBuilder(
      absl::string_view name) = 0;

  // `FindValue` returns a new `Value` for the corresponding name `name`. This
  // can be used to translate enum names to numeric values.
  virtual absl::StatusOr<ValueView> FindValue(
      absl::string_view name, Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_PROVIDER_H_

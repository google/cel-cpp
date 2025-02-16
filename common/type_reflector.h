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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_REFLECTOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_REFLECTOR_H_

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// `TypeReflector` is an interface for constructing new instances of types are
// runtime. It handles type reflection.
class TypeReflector : public virtual TypeIntrospector {
 public:
  // Legacy type reflector, will prefer builders for legacy value.
  ABSL_DEPRECATED("Is now the same as Builtin()")
  static TypeReflector& LegacyBuiltin() { return Builtin(); }
  // Will prefer builders for modern values.
  ABSL_DEPRECATED("Is now the same as Builtin()")
  static TypeReflector& ModernBuiltin() { return Builtin(); }

  static TypeReflector& Builtin();

  // `NewListValueBuilder` returns a new `ListValueBuilderInterface` for the
  // corresponding `ListType` `type`.
  absl::StatusOr<absl::Nonnull<ListValueBuilderPtr>> NewListValueBuilder(
      ValueFactory& value_factory, const ListType& type) const;

  // `NewMapValueBuilder` returns a new `MapValueBuilderInterface` for the
  // corresponding `MapType` `type`.
  absl::StatusOr<absl::Nonnull<MapValueBuilderPtr>> NewMapValueBuilder(
      ValueFactory& value_factory, const MapType& type) const;

  // `NewStructValueBuilder` returns a new `StructValueBuilder` for the
  // corresponding `StructType` `type`.
  virtual absl::StatusOr<absl::Nullable<StructValueBuilderPtr>>
  NewStructValueBuilder(ValueFactory& value_factory,
                        const StructType& type) const;

  // `NewValueBuilder` returns a new `ValueBuilder` for the corresponding type
  // `name`.  It is primarily used to handle wrapper types which sometimes show
  // up literally in expressions.
  virtual absl::StatusOr<absl::Nullable<ValueBuilderPtr>> NewValueBuilder(
      ValueFactory& value_factory, absl::string_view name) const;

  virtual absl::Nullable<const google::protobuf::DescriptorPool*> descriptor_pool()
      const {
    return nullptr;
  }
};

Shared<TypeReflector> NewThreadCompatibleTypeReflector(
    MemoryManagerRef memory_manager);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_REFLECTOR_H_

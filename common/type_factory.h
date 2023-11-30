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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_FACTORY_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/sized_input_view.h"
#include "common/type.h"

namespace cel {

// `TypeFactory` is the preferred way for constructing compound types such as
// lists, maps, structs, and opaques. It caches types and avoids constructing
// them multiple times.
class TypeFactory {
 public:
  virtual ~TypeFactory() = default;

  virtual absl::StatusOr<ListType> CreateListType(TypeView element) = 0;

  virtual absl::StatusOr<MapType> CreateMapType(TypeView key,
                                                TypeView value) = 0;

  virtual absl::StatusOr<StructType> CreateStructType(
      absl::string_view name) = 0;

  virtual absl::StatusOr<OpaqueType> CreateOpaqueType(
      absl::string_view name, const SizedInputView<TypeView>& parameters) = 0;

  absl::StatusOr<OptionalType> CreateOptionalType(TypeView parameter);
};

// Creates a new `TypeFactory` which is thread compatible. The returned
// `TypeFactory` and all types it creates are managed my `memory_manager`.
Shared<TypeFactory> NewThreadCompatibleTypeFactory(
    MemoryManagerRef memory_manager);

// Creates a new `TypeFactory` which is thread safe. The returned `TypeFactory`
// and all types it creates are managed my `memory_manager`.
Shared<TypeFactory> NewThreadSafeTypeFactory(MemoryManagerRef memory_manager);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_FACTORY_H_

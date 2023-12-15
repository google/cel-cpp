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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_COMPATIBLE_TYPE_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_COMPATIBLE_TYPE_PROVIDER_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_provider.h"

namespace cel::common_internal {

// `ThreadCompatibleTypeProvider` is a basic implementation of `TypeProvider`
// which is thread compatible. By default this implementation just returns
// `NOT_FOUND` for most methods.
class ThreadCompatibleTypeProvider : public virtual TypeProvider {
 public:
  explicit ThreadCompatibleTypeProvider(MemoryManagerRef memory_manager)
      : memory_manager_(memory_manager) {}

  MemoryManagerRef GetMemoryManager() const final { return memory_manager_; }

  absl::StatusOr<TypeView> FindType(TypeFactory& type_factory,
                                    absl::string_view name,
                                    Type& scratch) override;

  absl::StatusOr<StructTypeFieldView> FindStructTypeFieldByName(
      TypeFactory& type_factory, absl::string_view type, absl::string_view name,
      StructTypeField& scratch) override;

  absl::StatusOr<StructTypeFieldView> FindStructTypeFieldByName(
      TypeFactory& type_factory, StructTypeView type, absl::string_view name,
      StructTypeField& scratch) override;

 private:
  MemoryManagerRef memory_manager_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_COMPATIBLE_TYPE_PROVIDER_H_

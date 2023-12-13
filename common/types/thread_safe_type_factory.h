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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_SAFE_TYPE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_SAFE_TYPE_FACTORY_H_

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "common/memory.h"
#include "common/sized_input_view.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/types/type_cache.h"

namespace cel::common_internal {

class ThreadSafeTypeFactory final : public TypeFactory {
 public:
  explicit ThreadSafeTypeFactory(MemoryManagerRef memory_manager)
      : memory_manager_(memory_manager) {}

  MemoryManagerRef GetMemoryManager() const override { return memory_manager_; }

 private:
  ListType CreateListTypeImpl(TypeView element) override;

  MapType CreateMapTypeImpl(TypeView key, TypeView value) override;

  StructType CreateStructTypeImpl(absl::string_view name) override;

  OpaqueType CreateOpaqueTypeImpl(
      absl::string_view name,
      const SizedInputView<TypeView>& parameters) override;

  MemoryManagerRef memory_manager_;
  absl::Mutex list_types_mutex_;
  ListTypeCacheMap list_types_ ABSL_GUARDED_BY(list_types_mutex_);
  absl::Mutex map_types_mutex_;
  MapTypeCacheMap map_types_ ABSL_GUARDED_BY(map_types_mutex_);
  absl::Mutex struct_types_mutex_;
  StructTypeCacheMap struct_types_ ABSL_GUARDED_BY(struct_types_mutex_);
  absl::Mutex opaque_types_mutex_;
  OpaqueTypeCacheMap opaque_types_ ABSL_GUARDED_BY(opaque_types_mutex_);
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_THREAD_SAFE_TYPE_FACTORY_H_

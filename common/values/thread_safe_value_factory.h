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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_THREAD_SAFE_VALUE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_THREAD_SAFE_VALUE_FACTORY_H_

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/types/thread_safe_type_factory.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/values/value_cache.h"

namespace cel::common_internal {

// `ThreadSafeValueFactory` is a thread-safe implementation of `ValueFactory`.
// All methods are safe to call from any thread. It is more efficient than using
// external synchronization with `ThreadCompatibleValueFactory`, but less
// efficient than using `ThreadCompatibleValueFactory` with a single thread.
class ThreadSafeValueFactory : public ThreadSafeTypeFactory,
                               public ValueFactory {
 public:
  explicit ThreadSafeValueFactory(MemoryManagerRef memory_manager)
      : ThreadSafeTypeFactory(memory_manager) {}

  using ThreadSafeTypeFactory::GetMemoryManager;

 private:
  ListValue CreateZeroListValueImpl(ListTypeView type) override;

  MapValue CreateZeroMapValueImpl(MapTypeView type) override;

  OptionalValue CreateZeroOptionalValueImpl(OptionalTypeView type) override;

  absl::Mutex list_values_mutex_;
  ListValueCacheMap list_values_ ABSL_GUARDED_BY(list_values_mutex_);
  absl::Mutex map_values_mutex_;
  MapValueCacheMap map_values_ ABSL_GUARDED_BY(map_values_mutex_);
  absl::Mutex optional_values_mutex_;
  OptionalValueCacheMap optional_values_
      ABSL_GUARDED_BY(optional_values_mutex_);
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_THREAD_SAFE_VALUE_FACTORY_H_

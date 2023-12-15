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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_THREAD_COMPATIBLE_VALUE_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_THREAD_COMPATIBLE_VALUE_PROVIDER_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/types/thread_compatible_type_provider.h"
#include "common/value.h"
#include "common/value_provider.h"

namespace cel::common_internal {

class ThreadCompatibleValueProvider : public ThreadCompatibleTypeProvider,
                                      public ValueProvider {
 public:
  explicit ThreadCompatibleValueProvider(MemoryManagerRef memory_manager)
      : ThreadCompatibleTypeProvider(memory_manager) {}

  using ThreadCompatibleTypeProvider::GetMemoryManager;

  absl::StatusOr<Unique<StructValueBuilder>> NewStructValueBuilder(
      ValueFactory& value_factory, StructType type) override;

  absl::StatusOr<Unique<ValueBuilder>> NewValueBuilder(
      ValueFactory& value_factory, absl::string_view name) override;

  absl::StatusOr<ValueView> FindValue(ValueFactory& value_factory,
                                      absl::string_view name,
                                      Value& scratch) override;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_THREAD_COMPATIBLE_VALUE_PROVIDER_H_

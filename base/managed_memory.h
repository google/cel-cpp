// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_MANAGED_MEMORY_H_
#define THIRD_PARTY_CEL_CPP_BASE_MANAGED_MEMORY_H_

#include <type_traits>

#include "base/internal/managed_memory.h"

namespace cel {

// `ManagedMemory` is a smart pointer which ensures any applicable object
// destructors and deallocation are eventually performed. Copying does not
// actually copy the underlying T, instead a pointer is copied and optionally
// reference counted. Moving does not actually move the underlying T, instead a
// pointer is moved.
//
// TODO(issues/5): consider feature parity with std::unique_ptr<T>
template <typename T>
using ManagedMemory = base_internal::ManagedMemory<T>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_MANAGED_MEMORY_H_

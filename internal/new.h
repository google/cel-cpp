// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_NEW_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_NEW_H_

#include <cstddef>
#include <new>
#include <utility>

namespace cel::internal {

inline std::pair<void*, size_t> SizeReturningNew(size_t size) {
  return std::pair{::operator new(size), size};
}

inline std::pair<void*, size_t> SizeReturningNew(size_t size,
                                                 std::align_val_t alignment) {
  return std::pair{::operator new(size, alignment), size};
}

inline void SizedDelete(void* ptr, size_t size) noexcept {
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
  ::operator delete(ptr, size);
#else
  ::operator delete(ptr);
#endif
}

inline void SizedDelete(void* ptr, size_t size,
                        std::align_val_t alignment) noexcept {
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
  ::operator delete(ptr, size, alignment);
#else
  ::operator delete(ptr, alignment);
#endif
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_NEW_H_

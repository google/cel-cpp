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

// This header provides a cross-platform binding to the dynamic loader.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_DYNAMIC_LOADER_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_DYNAMIC_LOADER_H_

#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "absl/types/optional.h"

namespace cel::internal {

class DynamicallyLoadedSymbol final {
 public:
  explicit DynamicallyLoadedSymbol(void* address) : address_(address) {}

  DynamicallyLoadedSymbol(const DynamicallyLoadedSymbol&) = default;
  DynamicallyLoadedSymbol& operator=(const DynamicallyLoadedSymbol&) = default;

  template <typename T, typename = std::enable_if_t<std::is_pointer_v<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator T() const {
    return reinterpret_cast<T>(address_);
  }

 private:
  void* address_;
};

class DynamicLoader final {
 public:
  DynamicLoader();

  ~DynamicLoader();

  DynamicLoader(const DynamicLoader&) = delete;
  DynamicLoader(DynamicLoader&&) = delete;
  DynamicLoader& operator=(const DynamicLoader&) = delete;
  DynamicLoader& operator=(DynamicLoader&&) = delete;

  DynamicallyLoadedSymbol FindSymbolOrDie(const char* name) const;

  absl::optional<DynamicallyLoadedSymbol> FindSymbol(const char* name) const;

 private:
#ifdef _WIN32
  HMODULE* modules_ = nullptr;
  DWORD modules_size_ = 0;
#else
  void* handle_ = RTLD_DEFAULT;
#endif
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_DYNAMIC_LOADER_H_

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

#include "internal/dynamic_loader.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "absl/log/absl_check.h"

namespace cel::internal {

DynamicLoader::DynamicLoader() {
#ifdef _WIN32
  DWORD modules_capacity = 16;
  modules_ = new HMODULE[modules_capacity];
  while (true) {
    if (!::EnumProcessModulesEx(::GetCurrentProcessHandle(), &modules_,
                                modules_capacity * sizeof(HMODULE),
                                &modules_size_, LIST_MODULES_DEFAULT)) {
      // Give up. Fallback to check fail.
      delete[] modules_;
      modules_size_ = 0;
      break;
    }
    modules_size_ /= sizeof(HMODULE);
    if (modules_size_ <= modules_capacity) {
      break;
    }
    modules_capacity *= 2;
    delete[] modules_;
    modules_ = new HMODULE[modules_capacity];
  }
#endif
}

DynamicLoader::~DynamicLoader() {
#ifdef _WIN32
  delete[] modules_;
#endif
}

DynamicallyLoadedSymbol DynamicLoader::FindSymbolOrDie(const char* name) const {
  auto address = FindSymbol(name);
  ABSL_CHECK(address)  // Crash OK
      << "failed to find dynamic library symbol: " << name;
  return *address;
}

absl::optional<DynamicallyLoadedSymbol> DynamicLoader::FindSymbol(
    const char* name) const {
  void* address = nullptr;
#ifdef _WIN32
  for (DWORD i = 0; address == nullptr && i < modules_size_; ++i) {
    address = ::GetProcAddress(modules_[i], name);
  }
#else
  address = ::dlsym(handle_, name);
#endif
  if (address == nullptr) {
    return absl::nullopt;
  }
  return DynamicallyLoadedSymbol{address};
}

}  // namespace cel::internal

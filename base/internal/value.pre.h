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

// IWYU pragma: private, include "base/value.h"

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_VALUE_PRE_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_VALUE_PRE_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "base/handle.h"
#include "internal/rtti.h"

namespace cel {

class EnumValue;
class StructValue;
class ListValue;
class MapValue;

namespace base_internal {

class ValueHandleBase;
template <HandleType H>
class ValueHandle;

// Convenient aliases.
using TransientValueHandle = ValueHandle<HandleType::kTransient>;
using PersistentValueHandle = ValueHandle<HandleType::kPersistent>;

// As all objects should be aligned to at least 4 bytes, we can use the lower
// two bits for our own purposes.
inline constexpr uintptr_t kValueHandleManaged = 1 << 0;
inline constexpr uintptr_t kValueHandleUnmanaged = 1 << 1;
inline constexpr uintptr_t kValueHandleBits =
    kValueHandleManaged | kValueHandleUnmanaged;
inline constexpr uintptr_t kValueHandleMask = ~kValueHandleBits;

internal::TypeInfo GetEnumValueTypeId(const EnumValue& enum_value);

internal::TypeInfo GetStructValueTypeId(const StructValue& struct_value);

internal::TypeInfo GetListValueTypeId(const ListValue& list_value);

internal::TypeInfo GetMapValueTypeId(const MapValue& map_value);

class InlinedCordBytesValue;
class InlinedStringViewBytesValue;
class StringBytesValue;
class ExternalDataBytesValue;
class InlinedCordStringValue;
class InlinedStringViewStringValue;
class StringStringValue;
class ExternalDataStringValue;

// Type erased state capable of holding a pointer to remote storage or storing
// objects less than two pointers in size inline.
union ExternalDataReleaserState final {
  void* remote;
  alignas(alignof(std::max_align_t)) char local[sizeof(void*) * 2];
};

// Function which deletes the object referenced by ExternalDataReleaserState.
using ExternalDataReleaserDeleter = void(ExternalDataReleaserState* state);

template <typename Releaser>
void LocalExternalDataReleaserDeleter(ExternalDataReleaserState* state) {
  reinterpret_cast<Releaser*>(&state->local)->~Releaser();
}

template <typename Releaser>
void RemoteExternalDataReleaserDeleter(ExternalDataReleaserState* state) {
  ::delete reinterpret_cast<Releaser*>(state->remote);
}

// Function which invokes the object referenced by ExternalDataReleaserState.
using ExternalDataReleaseInvoker =
    void(ExternalDataReleaserState* state) noexcept;

template <typename Releaser>
void LocalExternalDataReleaserInvoker(
    ExternalDataReleaserState* state) noexcept {
  (*reinterpret_cast<Releaser*>(&state->local))();
}

template <typename Releaser>
void RemoteExternalDataReleaserInvoker(
    ExternalDataReleaserState* state) noexcept {
  (*reinterpret_cast<Releaser*>(&state->remote))();
}

struct ExternalDataReleaser final {
  ExternalDataReleaser() = delete;

  template <typename Releaser>
  explicit ExternalDataReleaser(Releaser&& releaser) {
    using DecayedReleaser = std::decay_t<Releaser>;
    if constexpr (sizeof(DecayedReleaser) <= sizeof(void*) * 2 &&
                  alignof(DecayedReleaser) <= alignof(std::max_align_t)) {
      // Object meets size and alignment constraints, will be stored
      // inline in ExternalDataReleaserState.local.
      ::new (static_cast<void*>(&state.local))
          DecayedReleaser(std::forward<Releaser>(releaser));
      invoker = LocalExternalDataReleaserInvoker<DecayedReleaser>;
      if constexpr (std::is_trivially_destructible_v<DecayedReleaser>) {
        // Object is trivially destructable, no need to call destructor at all.
        deleter = nullptr;
      } else {
        deleter = LocalExternalDataReleaserDeleter<DecayedReleaser>;
      }
    } else {
      // Object does not meet size and alignment constraints, allocate on the
      // heap and store pointer in ExternalDataReleaserState::remote. inline in
      // ExternalDataReleaserState::local.
      state.remote = ::new DecayedReleaser(std::forward<Releaser>(releaser));
      invoker = RemoteExternalDataReleaserInvoker<DecayedReleaser>;
      deleter = RemoteExternalDataReleaserDeleter<DecayedReleaser>;
    }
  }

  ExternalDataReleaser(const ExternalDataReleaser&) = delete;

  ExternalDataReleaser(ExternalDataReleaser&&) = delete;

  ~ExternalDataReleaser() {
    (*invoker)(&state);
    if (deleter != nullptr) {
      (*deleter)(&state);
    }
  }

  ExternalDataReleaser& operator=(const ExternalDataReleaser&) = delete;

  ExternalDataReleaser& operator=(ExternalDataReleaser&&) = delete;

  ExternalDataReleaserState state;
  ExternalDataReleaserDeleter* deleter;
  ExternalDataReleaseInvoker* invoker;
};

// Utility class encompassing a contiguous array of data which a function that
// must be called when the data is no longer needed.
struct ExternalData final {
  ExternalData() = delete;

  ExternalData(const void* data, size_t size,
               std::unique_ptr<ExternalDataReleaser> releaser)
      : data(data), size(size), releaser(std::move(releaser)) {}

  ExternalData(const ExternalData&) = delete;

  ExternalData(ExternalData&&) noexcept = default;

  ExternalData& operator=(const ExternalData&) = delete;

  ExternalData& operator=(ExternalData&&) noexcept = default;

  const void* data;
  size_t size;
  std::unique_ptr<ExternalDataReleaser> releaser;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_VALUE_PRE_H_

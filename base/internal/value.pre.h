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
#include <functional>
#include <memory>
#include <utility>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
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

using StringValueRep =
    absl::variant<absl::string_view, std::reference_wrapper<const absl::Cord>>;
using BytesValueRep =
    absl::variant<absl::string_view, std::reference_wrapper<const absl::Cord>>;

}  // namespace base_internal

}  // namespace cel

#define CEL_INTERNAL_VALUE_DECL(name)     \
  extern template class Persistent<name>; \
  extern template class Persistent<const name>

#define CEL_INTERNAL_VALUE_IMPL(name) \
  template class Persistent<name>;    \
  template class Persistent<const name>

// Both are equivalent to std::construct_at implementation from C++20.
#define CEL_INTERNAL_VALUE_COPY_TO(type, src, dest) \
  ::new (const_cast<void*>(                         \
      static_cast<const volatile void*>(std::addressof(dest)))) type(src)
#define CEL_INTERNAL_VALUE_MOVE_TO(type, src, dest)           \
  ::new (const_cast<void*>(static_cast<const volatile void*>( \
      std::addressof(dest)))) type(std::move(src))

#define CEL_INTERNAL_DECLARE_VALUE(base, derived)                              \
 private:                                                                      \
  friend class ::cel::base_internal::ValueHandleBase;                          \
                                                                               \
  static bool Is(const ::cel::Value& value);                                   \
                                                                               \
  ::std::pair<::std::size_t, ::std::size_t> SizeAndAlignment() const override; \
                                                                               \
  ::cel::internal::TypeInfo TypeId() const override;

#define CEL_INTERNAL_IMPLEMENT_VALUE(base, derived)                           \
  static_assert(::std::is_base_of_v<::cel::base##Value, derived>,             \
                #derived " must inherit from cel::" #base "Value");           \
  static_assert(!::std::is_abstract_v<derived>, "this must not be abstract"); \
                                                                              \
  bool derived::Is(const ::cel::Value& value) {                               \
    return value.kind() == ::cel::Kind::k##base &&                            \
           ::cel::base_internal::Get##base##ValueTypeId(                      \
               ::cel::internal::down_cast<const ::cel::base##Value&>(         \
                   value)) == ::cel::internal::TypeId<derived>();             \
  }                                                                           \
                                                                              \
  ::std::pair<::std::size_t, ::std::size_t> derived::SizeAndAlignment()       \
      const {                                                                 \
    static_assert(                                                            \
        ::std::is_same_v<derived,                                             \
                         ::std::remove_const_t<                               \
                             ::std::remove_reference_t<decltype(*this)>>>,    \
        "this must be the same as " #derived);                                \
    return ::std::pair<::std::size_t, ::std::size_t>(sizeof(derived),         \
                                                     alignof(derived));       \
  }                                                                           \
                                                                              \
  ::cel::internal::TypeInfo derived::TypeId() const {                         \
    return ::cel::internal::TypeId<derived>();                                \
  }

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_VALUE_PRE_H_

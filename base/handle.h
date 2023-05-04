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

#ifndef THIRD_PARTY_CEL_CPP_BASE_HANDLE_H_
#define THIRD_PARTY_CEL_CPP_BASE_HANDLE_H_

#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "base/internal/data.h"
#include "base/internal/handle.h"  // IWYU pragma: export

namespace cel {

class MemoryManager;

// `Handle` is a handle that shares ownership of the referenced `T`. It is valid
// so long as there are 1 or more handles pointing to `T` and the
// `AllocationManager` that constructed it is alive.
template <typename T>
class Handle final : private base_internal::HandlePolicy<T> {
 private:
  using Traits = base_internal::HandleTraits<T>;
  using Impl = typename Traits::handle_type;

 public:
  // Default constructs the handle, setting it to an empty state. It is
  // undefined behavior to call any functions that attempt to dereference or
  // access `T` when in an empty state.
  Handle() noexcept = default;

  Handle(const Handle<T>&) noexcept = default;

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  Handle(const Handle<F>& handle) noexcept : impl_(handle.impl_) {}  // NOLINT

  Handle(Handle<T>&&) noexcept = default;

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  Handle(Handle<F>&& handle) noexcept  // NOLINT
      : impl_(std::move(handle.impl_)) {}

  ~Handle() noexcept = default;

  Handle<T>& operator=(const Handle<T>&) noexcept = default;

  Handle<T>& operator=(Handle<T>&&) noexcept = default;

  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, Handle<T>&>  // NOLINT
  operator=(const Handle<F>& handle) noexcept {
    impl_ = handle.impl_;
    return *this;
  }

  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, Handle<T>&>  // NOLINT
  operator=(Handle<F>&& handle) noexcept {
    impl_ = std::move(handle.impl_);
    return *this;
  }

  // Reinterpret the handle of type `T` as type `F`. `T` must be derived from
  // `F`, `F` must be derived from `T`, or `F` must be the same as `T`.
  //
  // Handle<Resource> handle;
  // handle.As<const SubResource>()->SubMethod();
  template <typename F>
  std::enable_if_t<
      std::disjunction_v<std::is_base_of<F, T>, std::is_base_of<T, F>,
                         std::is_same<F, T>>,
      Handle<F>&>
  As() & noexcept ABSL_MUST_USE_RESULT {
    static_assert(std::is_same_v<Impl, typename Handle<F>::Impl>,
                  "Handle<T> and Handle<F> must have the same "
                  "implementation type");
    ABSL_DCHECK(static_cast<bool>(*this)) << "cannot reinterpret empty handle";
#ifndef NDEBUG
    static_cast<void>(static_cast<T&>(*impl_.get()).template As<F>());
#endif
    // Handle<T> and Handle<F> have the same underlying layout
    // representation, as ensured via the first static_assert, and they have
    // compatible types such that F is the base of T or T is the base of F, as
    // ensured via SFINAE on the return value and the second static_assert. Thus
    // we can safely reinterpret_cast.
    return *reinterpret_cast<Handle<F>*>(this);
  }

  // Reinterpret the handle of type `T` as type `F`. `T` must be derived from
  // `F`, `F` must be derived from `T`, or `F` must be the same as `T`.
  //
  // Handle<Resource> handle;
  // handle.As<const SubResource>()->SubMethod();
  template <typename F>
  std::enable_if_t<
      std::disjunction_v<std::is_base_of<F, T>, std::is_base_of<T, F>,
                         std::is_same<F, T>>,
      Handle<F>&&>
  As() && noexcept ABSL_MUST_USE_RESULT {
    static_assert(std::is_same_v<Impl, typename Handle<F>::Impl>,
                  "Handle<T> and Handle<F> must have the same "
                  "implementation type");
    ABSL_DCHECK(static_cast<bool>(*this)) << "cannot reinterpret empty handle";
#ifndef NDEBUG
    static_cast<void>(static_cast<T&>(*impl_.get()).template As<F>());
#endif
    // Handle<T> and Handle<F> have the same underlying layout
    // representation, as ensured via the first static_assert, and they have
    // compatible types such that F is the base of T or T is the base of F, as
    // ensured via SFINAE on the return value and the second static_assert. Thus
    // we can safely reinterpret_cast.
    return std::move(*reinterpret_cast<Handle<F>*>(this));
  }

  // Reinterpret the handle of type `T` as type `F`. `T` must be derived from
  // `F`, `F` must be derived from `T`, or `F` must be the same as `T`.
  //
  // Handle<Resource> handle;
  // handle.As<const SubResource>()->SubMethod();
  template <typename F>
  std::enable_if_t<
      std::disjunction_v<std::is_base_of<F, T>, std::is_base_of<T, F>,
                         std::is_same<F, T>>,
      const Handle<F>&>
  As() const& noexcept ABSL_MUST_USE_RESULT {
    static_assert(std::is_same_v<Impl, typename Handle<F>::Impl>,
                  "Handle<T> and Handle<F> must have the same "
                  "implementation type");
    ABSL_DCHECK(static_cast<bool>(*this)) << "cannot reinterpret empty handle";
#ifndef NDEBUG
    static_cast<void>(static_cast<T&>(*impl_.get()).template As<F>());
#endif
    // Handle<T> and Handle<F> have the same underlying layout
    // representation, as ensured via the first static_assert, and they have
    // compatible types such that F is the base of T or T is the base of F, as
    // ensured via SFINAE on the return value and the second static_assert. Thus
    // we can safely reinterpret_cast.
    return *reinterpret_cast<const Handle<F>*>(this);
  }

  // Reinterpret the handle of type `T` as type `F`. `T` must be derived from
  // `F`, `F` must be derived from `T`, or `F` must be the same as `T`.
  //
  // Handle<Resource> handle;
  // handle.As<const SubResource>()->SubMethod();
  template <typename F>
  std::enable_if_t<
      std::disjunction_v<std::is_base_of<F, T>, std::is_base_of<T, F>,
                         std::is_same<F, T>>,
      const Handle<F>&&>
  As() const&& noexcept ABSL_MUST_USE_RESULT {
    static_assert(std::is_same_v<Impl, typename Handle<F>::Impl>,
                  "Handle<T> and Handle<F> must have the same "
                  "implementation type");
    ABSL_DCHECK(static_cast<bool>(*this)) << "cannot reinterpret empty handle";
#ifndef NDEBUG
    static_cast<void>(static_cast<T&>(*impl_.get()).template As<F>());
#endif
    // Handle<T> and Handle<F> have the same underlying layout
    // representation, as ensured via the first static_assert, and they have
    // compatible types such that F is the base of T or T is the base of F, as
    // ensured via SFINAE on the return value and the second static_assert. Thus
    // we can safely reinterpret_cast.
    return std::move(*reinterpret_cast<const Handle<F>*>(this));
  }

  T& operator*() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(static_cast<bool>(*this)) << "cannot dereference empty handle";
    return static_cast<T&>(*impl_.get());
  }

  T* operator->() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(static_cast<bool>(*this)) << "cannot dereference empty handle";
    return static_cast<T*>(impl_.get());
  }

  // Tests whether the handle is not empty, returning false if it is empty.
  explicit operator bool() const noexcept { return static_cast<bool>(impl_); }

  friend void swap(Handle<T>& lhs, Handle<T>& rhs) noexcept {
    std::swap(lhs.impl_, rhs.impl_);
  }

  // Equality between handles is not the same as the equality defined by the
  // Common Expression Language. Instead it is more of a trivial equality, with
  // some kinds being compared by value and some kinds being compared by
  // pointers.
  //
  // Types:
  //
  // All types are compared via their kinds and then their name.
  //
  // Values:
  //
  // Struct, List, and Map are compared by pointer, thus two independently
  // constructed Struct(s), List(s), or Map(s) will not be equal even if their
  // contents are the same. String and Bytes are compared by their contents. All
  // other kinds are compared by value.

  bool operator==(const Handle<T>& other) const { return impl_ == other.impl_; }

  template <typename F>
  std::enable_if_t<std::disjunction_v<std::is_convertible<F*, T*>,
                                      std::is_convertible<T*, F*>>,
                   bool>
  operator==(const Handle<F>& other) const {
    return impl_ == other.impl_;
  }

  bool operator!=(const Handle<T>& other) const { return !operator==(other); }

  template <typename F>
  std::enable_if_t<std::disjunction_v<std::is_convertible<F*, T*>,
                                      std::is_convertible<T*, F*>>,
                   bool>
  operator!=(const Handle<F>& other) const {
    return !operator==(other);
  }

  template <typename H>
  friend H AbslHashValue(H state, const Handle<T>& handle) {
    return H::combine(std::move(state), handle.impl_);
  }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Handle<T>& handle) {
    if (handle) {
      sink.Append(handle->DebugString());
    }
  }

 private:
  template <typename F>
  friend class Handle;
  template <typename F>
  friend struct base_internal::HandleFactory;
  friend class MemoryManager;

  template <typename F, typename... Args>
  explicit Handle(base_internal::InPlaceStoredInline<F> tag, Args&&... args)
      : impl_(tag, std::forward<Args>(args)...) {}

  Handle(base_internal::InPlaceArenaAllocated tag,
         typename Impl::base_type& arg)
      : impl_(tag, arg) {}

  Handle(base_internal::InPlaceReferenceCounted tag,
         typename Impl::base_type& arg)
      : impl_(tag, arg) {}

  Impl impl_;
};

}  // namespace cel

// -----------------------------------------------------------------------------
// Internal implementation details.

namespace cel::base_internal {

template <typename T>
struct HandleFactory {
  static_assert(IsDerivedDataV<T>);

  // Constructs a handle whose underlying object is stored in the
  // handle itself.
  template <typename F, typename... Args>
  static std::enable_if_t<IsDerivedInlineDataV<F>, Handle<T>> Make(
      Args&&... args) {
    static_assert(std::is_base_of_v<T, F>, "F is not derived from T");
    return Handle<T>(kInPlaceStoredInline<F>, std::forward<Args>(args)...);
  }

  // Constructs a handle whose underlying object is stored in the
  // handle itself.
  template <typename F, typename... Args>
  static std::enable_if_t<IsDerivedInlineDataV<F>, void> MakeAt(
      void* address, Args&&... args) {
    static_assert(std::is_base_of_v<T, F>, "F is not derived from T");
    ::new (address)
        Handle<T>(kInPlaceStoredInline<F>, std::forward<Args>(args)...);
  }

  // Constructs a handle whose underlying object is heap allocated
  // and potentially reference counted, depending on the memory manager
  // implementation.
  template <typename F, typename... Args>
  static std::enable_if_t<IsDerivedHeapDataV<F>, Handle<T>> Make(
      MemoryManager& memory_manager, Args&&... args);
};

}  // namespace cel::base_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_HANDLE_H_

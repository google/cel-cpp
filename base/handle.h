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
#include "absl/base/macros.h"
#include "base/internal/handle.pre.h"  // IWYU pragma: export
#include "internal/casts.h"

namespace cel {

template <typename T>
class Transient;

template <typename T>
class Persistent;

// `Transient` is a handle that is intended to be short lived and may not
// actually own the referenced `T`. It is only valid as long as the handle it
// was created from or the native C++ value it is wrapping is valid. If you need
// to store a handle such that it can escape the current scope use `Persistent`.
template <typename T>
class Transient final : private base_internal::HandlePolicy<T> {
 private:
  using Traits = base_internal::TransientHandleTraits<std::remove_const_t<T>>;
  using Handle = typename Traits::handle_type;

 public:
  // Default constructs the handle, setting it to an empty state. It is
  // undefined behavior to call any functions that attempt to dereference or
  // access `T` when in an empty state.
  Transient() = default;

  Transient(const Transient<T>&) = default;

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  Transient(const Transient<F>& handle) : impl_(handle.impl_) {}  // NOLINT

  Transient(Transient<T>&&) = default;

  // Allow implicit conversion from Persistent to Transient, but not the other
  // way around. This is analogous to implicit conversion from std::string to
  // std::string_view.
  Transient(const Persistent<T>& handle);  // NOLINT

  // Allow implicit conversion from Persistent to Transient, but not the other
  // way around. This is analygous to implicit conversion from std::string to
  // std::string_view.
  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  Transient(const Persistent<F>& handle);  // NOLINT

  Transient<T>& operator=(const Transient<T>&) = default;

  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, Transient<T>&>  // NOLINT
  operator=(const Transient<F>& handle) {
    impl_ = handle.impl_;
    return *this;
  }

  Transient<T>& operator=(Transient<T>&&) = default;

  Transient<T>& operator=(const Persistent<T>& handle);

  // Same as the constructor above, but for the assign operator.
  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, Transient<T>&>  // NOLINT
  operator=(const Persistent<F>& handle);

  // Reinterpret the handle of type `T` as type `F`. `T` must be derived from
  // `F`, `F` must be derived from `T`, or `F` must be the same as `T`.
  //
  // Persistent<const Resource> handle;
  // handle.As<const SubResource>()->SubMethod();
  template <typename F>
  std::enable_if_t<
      std::disjunction_v<std::is_base_of<F, T>, std::is_base_of<T, F>,
                         std::is_same<F, T>>,
      Transient<F>&>
  As() ABSL_MUST_USE_RESULT {
    static_assert(std::is_same_v<Handle, typename Transient<F>::Handle>,
                  "Transient<T> and Transient<F> must have the same "
                  "implementation type");
    static_assert(
        (std::is_const_v<T> == std::is_const_v<F> || std::is_const_v<F>),
        "Constness cannot be removed, only added using As()");
    ABSL_ASSERT(this->template Is<std::remove_const_t<F>>());
    // Persistent<T> and Persistent<F> have the same underlying layout
    // representation, as ensured via the first static_assert, and they have
    // compatible types such that F is the base of T or T is the base of F, as
    // ensured via SFINAE on the return value and the second static_assert. Thus
    // we can saftley reinterpret_cast.
    return *reinterpret_cast<Transient<F>*>(this);
  }

  // Reinterpret the handle of type `T` as type `F`. `T` must be derived from
  // `F`, `F` must be derived from `T`, or `F` must be the same as `T`.
  //
  // Persistent<const Resource> handle;
  // handle.As<const SubResource>()->SubMethod();
  template <typename F>
  std::enable_if_t<
      std::disjunction_v<std::is_base_of<F, T>, std::is_base_of<T, F>,
                         std::is_same<F, T>>,
      const Transient<F>&>
  As() const ABSL_MUST_USE_RESULT {
    static_assert(std::is_same_v<Handle, typename Transient<F>::Handle>,
                  "Transient<T> and Transient<F> must have the same "
                  "implementation type");
    static_assert(
        (std::is_const_v<T> == std::is_const_v<F> || std::is_const_v<F>),
        "Constness cannot be removed, only added using As()");
    ABSL_ASSERT(this->template Is<std::remove_const_t<F>>());
    // Persistent<T> and Persistent<F> have the same underlying layout
    // representation, as ensured via the first static_assert, and they have
    // compatible types such that F is the base of T or T is the base of F, as
    // ensured via SFINAE on the return value and the second static_assert. Thus
    // we can saftley reinterpret_cast.
    return *reinterpret_cast<const Transient<F>*>(this);
  }

  // Is checks wether `T` is an instance of `F`.
  template <typename F>
  bool Is() const {
    return impl_.template Is<F>();
  }

  T& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(static_cast<bool>(*this));
    return internal::down_cast<T&>(*impl_);
  }

  T* operator->() const {
    ABSL_ASSERT(static_cast<bool>(*this));
    return internal::down_cast<T*>(impl_.operator->());
  }

  // Tests whether the handle is not empty, returning false if it is empty.
  explicit operator bool() const { return static_cast<bool>(impl_); }

  friend void swap(Transient<T>& lhs, Transient<T>& rhs) {
    std::swap(lhs.impl_, rhs.impl_);
  }

  friend bool operator==(const Transient<T>& lhs, const Transient<T>& rhs) {
    return lhs.impl_ == rhs.impl_;
  }

  template <typename H>
  friend H AbslHashValue(H state, const Transient<T>& handle) {
    return H::combine(std::move(state), handle.impl_);
  }

 private:
  template <typename F>
  friend class Transient;
  template <typename F>
  friend class Persistent;
  template <base_internal::HandleType H, typename F>
  friend struct base_internal::HandleFactory;
  template <typename F>
  friend bool base_internal::IsManagedHandle(const Transient<F>& handle);
  template <typename F>
  friend bool base_internal::IsUnmanagedHandle(const Transient<F>& handle);
  template <typename F>
  friend bool base_internal::IsInlinedHandle(const Transient<F>& handle);

  template <typename... Args>
  explicit Transient(base_internal::HandleInPlace, Args&&... args)
      : impl_(std::forward<Args>(args)...) {}

  Handle impl_;
};

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<L, R>, bool> operator==(
    const Transient<L>& lhs, const Transient<R>& rhs) {
  return lhs == rhs.template As<L>();
}

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<R, L>, bool> operator==(
    const Transient<L>& lhs, const Transient<R>& rhs) {
  return rhs == lhs.template As<R>();
}

template <typename T>
bool operator!=(const Transient<T>& lhs, const Transient<T>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<L, R>, bool> operator!=(
    const Transient<L>& lhs, const Transient<R>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<R, L>, bool> operator!=(
    const Transient<L>& lhs, const Transient<R>& rhs) {
  return !operator==(lhs, rhs);
}

// `Persistent` is a handle that is intended to be long lived and shares
// ownership of the referenced `T`. It is valid so long as
// there are 1 or more `Persistent` handles pointing to `T` and the
// `AllocationManager` that constructed it is alive.
template <typename T>
class Persistent final : private base_internal::HandlePolicy<T> {
 private:
  using Traits = base_internal::PersistentHandleTraits<std::remove_const_t<T>>;
  using Handle = typename Traits::handle_type;

 public:
  // Default constructs the handle, setting it to an empty state. It is
  // undefined behavior to call any functions that attempt to dereference or
  // access `T` when in an empty state.
  Persistent() = default;

  Persistent(const Persistent<T>&) = default;

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  Persistent(const Persistent<F>& handle) : impl_(handle.impl_) {}  // NOLINT

  Persistent(Persistent<T>&&) = default;

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  Persistent(Persistent<F>&& handle)  // NOLINT
      : impl_(std::move(handle.impl_)) {}

  // Allow Transient handles to be assigned to Persistent handles. This is
  // similar to std::string_view being assignable to std::string.
  explicit Persistent(Transient<T> handle) : impl_(handle.impl_) {}

  // Allow Transient handles to be assigned to Persistent handles. This is
  // similar to std::string_view being assignable to std::string.
  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  explicit Persistent(Transient<F> handle) : impl_(handle.impl_) {}

  Persistent<T>& operator=(const Persistent<T>&) = default;

  // Allow Transient handles to be assigned to Persistent handles. This is
  // similar to std::string_view being assignable to std::string.
  Persistent<T>& operator=(Transient<T> handle) {
    impl_ = handle.impl_;
    return *this;
  }

  // Allow Transient handles to be assigned to Persistent handles. This is
  // similar to std::string_view being assignable to std::string.
  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, Persistent<T>&>  // NOLINT
  operator=(Transient<F> handle) {
    impl_ = handle.impl_;
    return *this;
  }

  Persistent<T>& operator=(Persistent<T>&&) = default;

  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, Persistent<T>&>  // NOLINT
  operator=(const Persistent<F>& handle) {
    impl_ = handle.impl_;
    return *this;
  }

  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, Persistent<T>&>  // NOLINT
  operator=(Persistent<F>&& handle) {
    impl_ = std::move(handle.impl_);
    return *this;
  }

  // Reinterpret the handle of type `T` as type `F`. `T` must be derived from
  // `F`, `F` must be derived from `T`, or `F` must be the same as `T`.
  //
  // Persistent<const Resource> handle;
  // handle.As<const SubResource>()->SubMethod();
  template <typename F>
  std::enable_if_t<
      std::disjunction_v<std::is_base_of<F, T>, std::is_base_of<T, F>,
                         std::is_same<F, T>>,
      Persistent<F>&>
  As() ABSL_MUST_USE_RESULT {
    static_assert(std::is_same_v<Handle, typename Persistent<F>::Handle>,
                  "Persistent<T> and Persistent<F> must have the same "
                  "implementation type");
    static_assert(
        (std::is_const_v<T> == std::is_const_v<F> || std::is_const_v<F>),
        "Constness cannot be removed, only added using As()");
    ABSL_ASSERT(this->template Is<F>());
    // Persistent<T> and Persistent<F> have the same underlying layout
    // representation, as ensured via the first static_assert, and they have
    // compatible types such that F is the base of T or T is the base of F, as
    // ensured via SFINAE on the return value and the second static_assert. Thus
    // we can saftley reinterpret_cast.
    return *reinterpret_cast<Persistent<F>*>(this);
  }

  // Reinterpret the handle of type `T` as type `F`. `T` must be derived from
  // `F`, `F` must be derived from `T`, or `F` must be the same as `T`.
  //
  // Persistent<const Resource> handle;
  // handle.As<const SubResource>()->SubMethod();
  template <typename F>
  std::enable_if_t<
      std::disjunction_v<std::is_base_of<F, T>, std::is_base_of<T, F>,
                         std::is_same<F, T>>,
      const Persistent<F>&>
  As() const ABSL_MUST_USE_RESULT {
    static_assert(std::is_same_v<Handle, typename Persistent<F>::Handle>,
                  "Persistent<T> and Persistent<F> must have the same "
                  "implementation type");
    static_assert(
        (std::is_const_v<T> == std::is_const_v<F> || std::is_const_v<F>),
        "Constness cannot be removed, only added using As()");
    ABSL_ASSERT(this->template Is<std::remove_const_t<F>>());
    // Persistent<T> and Persistent<F> have the same underlying layout
    // representation, as ensured via the first static_assert, and they have
    // compatible types such that F is the base of T or T is the base of F, as
    // ensured via SFINAE on the return value and the second static_assert. Thus
    // we can saftley reinterpret_cast.
    return *reinterpret_cast<const Persistent<F>*>(this);
  }

  // Is checks wether `T` is an instance of `F`.
  template <typename F>
  bool Is() const {
    return impl_.template Is<F>();
  }

  T& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(static_cast<bool>(*this));
    return internal::down_cast<T&>(*impl_);
  }

  T* operator->() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(static_cast<bool>(*this));
    return internal::down_cast<T*>(impl_.operator->());
  }

  // Tests whether the handle is not empty, returning false if it is empty.
  explicit operator bool() const { return static_cast<bool>(impl_); }

  friend void swap(Persistent<T>& lhs, Persistent<T>& rhs) {
    std::swap(lhs.impl_, rhs.impl_);
  }

  friend bool operator==(const Persistent<T>& lhs, const Persistent<T>& rhs) {
    return lhs.impl_ == rhs.impl_;
  }

  friend bool operator==(const Transient<T>& lhs, const Persistent<T>& rhs) {
    return lhs.impl_ == rhs.impl_;
  }

  friend bool operator==(const Persistent<T>& lhs, const Transient<T>& rhs) {
    return lhs.impl_ == rhs.impl_;
  }

  template <typename H>
  friend H AbslHashValue(H state, const Persistent<T>& handle) {
    return H::combine(std::move(state), handle.impl_);
  }

 private:
  template <typename F>
  friend class Transient;
  template <typename F>
  friend class Persistent;
  template <base_internal::HandleType H, typename F>
  friend struct base_internal::HandleFactory;
  template <typename F>
  friend bool base_internal::IsManagedHandle(const Persistent<F>& handle);
  template <typename F>
  friend bool base_internal::IsUnmanagedHandle(const Persistent<F>& handle);
  template <typename F>
  friend bool base_internal::IsInlinedHandle(const Persistent<F>& handle);

  template <typename... Args>
  explicit Persistent(base_internal::HandleInPlace, Args&&... args)
      : impl_(std::forward<Args>(args)...) {}

  Handle impl_;
};

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<L, R>, bool> operator==(
    const Persistent<L>& lhs, const Persistent<R>& rhs) {
  return lhs == rhs.template As<L>();
}

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<R, L>, bool> operator==(
    const Persistent<L>& lhs, const Persistent<R>& rhs) {
  return rhs == lhs.template As<R>();
}

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<L, R>, bool> operator==(
    const Transient<L>& lhs, const Persistent<R>& rhs) {
  return lhs == rhs.template As<L>();
}

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<R, L>, bool> operator==(
    const Persistent<L>& lhs, const Transient<R>& rhs) {
  return rhs == lhs.template As<R>();
}

template <typename T>
bool operator!=(const Persistent<T>& lhs, const Persistent<T>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename T>
bool operator!=(const Transient<T>& lhs, const Persistent<T>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename T>
bool operator!=(const Persistent<T>& lhs, const Transient<T>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<L, R>, bool> operator!=(
    const Persistent<L>& lhs, const Persistent<R>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<R, L>, bool> operator!=(
    const Persistent<L>& lhs, const Persistent<R>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<L, R>, bool> operator!=(
    const Transient<L>& lhs, const Persistent<R>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename L, typename R>
std::enable_if_t<std::is_base_of_v<R, L>, bool> operator!=(
    const Persistent<L>& lhs, const Transient<R>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename T>
Transient<T>::Transient(const Persistent<T>& handle) : impl_(handle.impl_) {}

template <typename T>
template <typename F, typename>
Transient<T>::Transient(const Persistent<F>& handle) : impl_(handle.impl_) {}

template <typename T>
Transient<T>& Transient<T>::operator=(const Persistent<T>& handle) {
  impl_ = handle.impl_;
  return *this;
}

template <typename T>  // NOLINT
template <typename F>
std::enable_if_t<std::is_convertible_v<F*, T*>, Transient<T>&>
Transient<T>::operator=(const Persistent<F>& handle) {
  impl_ = handle.impl_;
  return *this;
}

}  // namespace cel

#include "base/internal/handle.post.h"  // IWYU pragma: export

#endif  // THIRD_PARTY_CEL_CPP_BASE_HANDLE_H_

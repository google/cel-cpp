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

#ifndef THIRD_PARTY_CEL_CPP_BASE_OWNER_H_
#define THIRD_PARTY_CEL_CPP_BASE_OWNER_H_

#include <type_traits>

#include "base/internal/data.h"

namespace cel {

class Type;
class Value;
class TypeFactory;
class ValueFactory;

template <typename T>
class EnableOwnerFromThis;

// Owner is a special type used for creating borrowed types and values. It
// represents the actual owner of the data. The created borrowed value will
// ensure that the Owner is alive for as long as it is alive.
template <typename T>
class Owner {
 private:
  using metadata_type = base_internal::SelectMetadata<T>;

 public:
  static_assert(!std::is_base_of_v<base_internal::InlineData, T>);

  Owner() = delete;

  Owner(const Owner<T>& other) noexcept : owner_(other.owner_) {
    if (owner_ != nullptr) {
      metadata_type::Ref(*owner_);
    }
  }

  Owner(Owner<T>&& other) noexcept : owner_(other.owner_) {
    other.owner_ = nullptr;
  }

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  Owner(const Owner<F>& other) noexcept : owner_(other.owner_) {  // NOLINT
    if (owner_ != nullptr) {
      metadata_type::Ref(*owner_);
    }
  }

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  Owner(Owner<F>&& other) : owner_(other.owner_) {  // NOLINT
    other.owner_ = nullptr;
  }

  Owner<T>& operator=(const Owner<T>& other) noexcept {
    if (this != &other) {
      if (static_cast<bool>(other)) {
        metadata_type::Ref(*other.owner_);
      }
      if (static_cast<bool>(*this)) {
        metadata_type::Unref(*owner_);
      }
      owner_ = other.owner_;
    }
    return *this;
  }

  Owner<T>& operator=(Owner<T>&& other) noexcept {
    if (this != &other) {
      if (static_cast<bool>(*this)) {
        metadata_type::Unref(*owner_);
      }
      owner_ = other.owner_;
      other.owner_ = nullptr;
    }
    return *this;
  }

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  Owner<T>& operator=(const Owner<F>& other) noexcept {
    if (this != &other) {
      if (static_cast<bool>(other)) {
        metadata_type::Ref(*other.owner_);
      }
      if (static_cast<bool>(*this)) {
        metadata_type::Unref(*owner_);
      }
      owner_ = other.owner_;
    }
    return *this;
  }

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  Owner<T>& operator=(Owner<F>&& other) {  // NOLINT
    if (this != &other) {
      if (static_cast<bool>(*this)) {
        metadata_type::Unref(*owner_);
      }
      owner_ = other.owner_;
      other.owner_ = nullptr;
    }
    return *this;
  }

  ~Owner() {
    if (static_cast<bool>(*this)) {
      metadata_type::Unref(*owner_);
    }
  }

  explicit operator bool() const { return owner_ != nullptr; }

 private:
  template <typename F>
  friend class Owner;
  template <typename F>
  friend class EnableOwnerFromThis;
  friend class TypeFactory;
  friend class ValueFactory;

  explicit Owner(const T* owner) : owner_(owner) {
    static_assert(std::is_base_of_v<base_internal::HeapData, T>);
  }

  const T* release() {
    const T* owner = owner_;
    owner_ = nullptr;
    return owner;
  }

  const T* owner_;
};

template <typename T>
class EnableOwnerFromThis {
 protected:
  Owner<T> owner_from_this() const {
    static_assert(std::is_base_of_v<EnableOwnerFromThis<T>, T>);
    static_assert(std::is_base_of_v<base_internal::HeapData, T>);
    using metadata_type = base_internal::SelectMetadata<T>;
    const T* owner = reinterpret_cast<const T*>(this);
    if (metadata_type::IsReferenceCounted(*owner)) {
      metadata_type::Ref(*owner);
    } else {
      owner = nullptr;
    }
    return Owner<T>(owner);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_OWNER_H_

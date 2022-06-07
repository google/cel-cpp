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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_NO_DESTRUCTOR_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_NO_DESTRUCTOR_H_

#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace cel::internal {

// `NoDestructor<T>` is primarily useful in optimizing the pattern of safe
// on-demand construction of an object with a non-trivial destructor in static
// storage without ever having the destructor called. By using `NoDestructor<T>`
// there is no need to involve a heap allocation.
template <typename T>
class NoDestructor final {
 public:
  template <typename... Args>
  explicit constexpr NoDestructor(Args&&... args)
      : impl_(std::in_place, std::forward<Args>(args)...) {}

  NoDestructor(const NoDestructor<T>&) = delete;
  NoDestructor(NoDestructor<T>&&) = delete;
  NoDestructor<T>& operator=(const NoDestructor<T>&) = delete;
  NoDestructor<T>& operator=(NoDestructor<T>&&) = delete;

  T& get() { return impl_.get(); }

  const T& get() const { return impl_.get(); }

  T& operator*() { return get(); }

  const T& operator*() const { return get(); }

  T* operator->() { return std::addressof(get()); }

  const T* operator->() const { return std::addressof(get()); }

 private:
  class TrivialImpl final {
   public:
    template <typename... Args>
    explicit constexpr TrivialImpl(std::in_place_t, Args&&... args)
        : value_(std::forward<Args>(args)...) {}

    T& get() { return value_; }

    const T& get() const { return value_; }

   private:
    T value_;
  };

  class PlacementImpl final {
   public:
    template <typename... Args>
    explicit PlacementImpl(std::in_place_t, Args&&... args) {
      ::new (static_cast<void*>(&value_)) T(std::forward<Args>(args)...);
    }

    T& get() { return *std::launder(reinterpret_cast<T*>(&value_)); }

    const T& get() const {
      return *std::launder(reinterpret_cast<const T*>(&value_));
    }

   private:
    alignas(T) uint8_t value_[sizeof(T)];
  };

  std::conditional_t<std::is_trivially_destructible_v<T>, TrivialImpl,
                     PlacementImpl>
      impl_;
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_NO_DESTRUCTOR_H_

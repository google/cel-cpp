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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_JSON_H_
#define THIRD_PARTY_CEL_CPP_COMMON_JSON_H_

#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/cord.h"
#include "absl/types/variant.h"
#include "internal/copy_on_write.h"

namespace cel {

// Maximum `int64_t` value that can be represented as `double` without losing
// data.
inline constexpr int64_t kJsonMaxInt = (int64_t{1} << 53) - 1;
// Minimum `int64_t` value that can be represented as `double` without losing
// data.
inline constexpr int64_t kJsonMinInt = -kJsonMaxInt;

// Maximum `uint64_t` value that can be represented as `double` without losing
// data.
inline constexpr uint64_t kJsonMaxUint = (uint64_t{1} << 53) - 1;

// `cel::JsonNull` is a strong type representing a parsed JSON `null`.
struct ABSL_ATTRIBUTE_TRIVIAL_ABI JsonNull final {
  explicit JsonNull() = default;
};

inline constexpr JsonNull kJsonNull{};

constexpr bool operator==(JsonNull, JsonNull) noexcept { return true; }

constexpr bool operator!=(JsonNull, JsonNull) noexcept { return false; }

constexpr bool operator<(JsonNull, JsonNull) noexcept { return false; }

constexpr bool operator<=(JsonNull, JsonNull) noexcept { return true; }

constexpr bool operator>(JsonNull, JsonNull) noexcept { return false; }

constexpr bool operator>=(JsonNull, JsonNull) noexcept { return true; }

template <typename H>
H AbslHashValue(H state, JsonNull) {
  return H::combine(std::move(state), uintptr_t{0});
}

// We cannot use type aliases to the containers because that would make `Json`
// a recursive template. So we need to forward declare array and object
// representations as another class.
class ABSL_ATTRIBUTE_TRIVIAL_ABI JsonArray;
class ABSL_ATTRIBUTE_TRIVIAL_ABI JsonObject;
class JsonArrayBuilder;
class JsonObjectBuilder;

// `cel::JsonBool` is a convenient alias to `bool` for the purpose of
// readability, it represents a parsed JSON `false` or `true`.
using JsonBool = bool;

// `cel::JsonNumber` is a convenient alias to `double` for the purpose of
// readability, it represents a parsed JSON number.
using JsonNumber = double;

// `cel::JsonString` is a convenient alias to `absl::Cord` for the purpose of
// readability, it represents a parsed JSON string.
using JsonString = absl::Cord;

// `cel::Json` is a variant which holds parsed JSON data. It is either
// `cel::JsonNull`, `cel::JsonBool`, `cel::JsonNumber`, `cel::JsonString`,
// `cel::JsonArray,` or `cel::JsonObject`.
using Json = absl::variant<JsonNull, JsonBool, JsonNumber, JsonString,
                           JsonArray, JsonObject>;

// `cel::JsonArray` uses copy-on-write semantics. Whenever a non-const method is
// called, it would have to assume a mutation is occurring potentially
// performing a copy. To avoid this subtly, `cel::JsonArray` is read-only. To
// perform mutations you must use `cel::JsonArrayBuilder`.
class JsonArrayBuilder {
 private:
  using Container = std::vector<Json>;

 public:
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  using difference_type = typename Container::difference_type;
  using reference = typename Container::reference;
  using const_reference = typename Container::const_reference;
  using pointer = typename Container::pointer;
  using const_pointer = typename Container::const_pointer;
  using iterator = typename Container::iterator;
  using const_iterator = typename Container::const_iterator;
  using reverse_iterator = typename Container::reverse_iterator;
  using const_reverse_iterator = typename Container::const_reverse_iterator;

  JsonArrayBuilder() = default;

  explicit JsonArrayBuilder(JsonArray array);

  JsonArrayBuilder(const JsonArrayBuilder&) = delete;
  JsonArrayBuilder(JsonArrayBuilder&&) = default;

  JsonArrayBuilder& operator=(const JsonArrayBuilder&) = delete;
  JsonArrayBuilder& operator=(JsonArrayBuilder&&) = default;

  bool empty() const { return impl_.get().empty(); }

  size_type size() const { return impl_.get().size(); }

  iterator begin() { return impl_.mutable_get().begin(); }

  const_iterator begin() const { return impl_.get().begin(); }

  iterator end() { return impl_.mutable_get().end(); }

  const_iterator end() const { return impl_.get().end(); }

  reverse_iterator rbegin() { return impl_.mutable_get().rbegin(); }

  reverse_iterator rend() { return impl_.mutable_get().rend(); }

  reference at(size_type index) { return impl_.mutable_get().at(index); }

  reference operator[](size_type index) { return (impl_.mutable_get())[index]; }

  void reserve(size_type n) {
    if (n != 0) {
      impl_.mutable_get().reserve(n);
    }
  }

  void clear() { impl_.mutable_get().clear(); }

  void push_back(Json json);

  void pop_back() { impl_.mutable_get().pop_back(); }

  JsonArray Build() &&;

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator JsonArray() &&;

 private:
  internal::CopyOnWrite<Container> impl_;
};

// `cel::JsonArray` is a read-only sequence of `cel::Json` elements.
class ABSL_ATTRIBUTE_TRIVIAL_ABI JsonArray final {
 private:
  using Container = std::vector<Json>;

 public:
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  using difference_type = typename Container::difference_type;
  using reference = typename Container::const_reference;
  using const_reference = typename Container::const_reference;
  using pointer = typename Container::const_pointer;
  using const_pointer = typename Container::const_pointer;
  using iterator = typename Container::const_iterator;
  using const_iterator = typename Container::const_iterator;
  using reverse_iterator = typename Container::const_reverse_iterator;
  using const_reverse_iterator = typename Container::const_reverse_iterator;

  JsonArray() : impl_(Empty()) {}

  JsonArray(const JsonArray&) = default;
  JsonArray(JsonArray&&) = default;

  JsonArray& operator=(const JsonArray&) = default;
  JsonArray& operator=(JsonArray&&) = default;

  bool empty() const { return impl_.get().empty(); }

  size_type size() const { return impl_.get().size(); }

  const_iterator begin() const { return impl_.get().begin(); }

  const_iterator cbegin() const { return impl_.get().cbegin(); }

  const_iterator end() const { return impl_.get().end(); }

  const_iterator cend() const { return impl_.get().cend(); }

  const_reverse_iterator rbegin() const { return impl_.get().rbegin(); }

  const_reverse_iterator crbegin() const { return impl_.get().crbegin(); }

  const_reverse_iterator rend() const { return impl_.get().rend(); }

  const_reverse_iterator crend() const { return impl_.get().crend(); }

  const_reference at(size_type index) const { return impl_.get().at(index); }

  const_reference operator[](size_type index) const {
    return (impl_.get())[index];
  }

  friend bool operator==(const JsonArray& lhs, const JsonArray& rhs);

  friend bool operator!=(const JsonArray& lhs, const JsonArray& rhs);

  template <typename H>
  friend H AbslHashValue(H state, const JsonArray& json_array);

 private:
  friend class JsonArrayBuilder;

  static internal::CopyOnWrite<Container> Empty();

  explicit JsonArray(internal::CopyOnWrite<Container> impl)
      : impl_(std::move(impl)) {
    if (impl_.get().empty()) {
      impl_ = Empty();
    }
  }

  internal::CopyOnWrite<Container> impl_;
};

// `cel::JsonObject` uses copy-on-write semantics. Whenever a non-const method
// is called, it would have to assume a mutation is occurring potentially
// performing a copy. To avoid this subtly, `cel::JsonObject` is read-only. To
// perform mutations you must use `cel::JsonObjectBuilder`.
class JsonObjectBuilder final {
 private:
  using Container = absl::flat_hash_map<JsonString, Json>;

 public:
  using key_type = typename Container::key_type;
  using mapped_type = typename Container::mapped_type;
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  using difference_type = typename Container::difference_type;
  using reference = typename Container::reference;
  using const_reference = typename Container::const_reference;
  using pointer = typename Container::pointer;
  using const_pointer = typename Container::const_pointer;
  using iterator = typename Container::iterator;
  using const_iterator = typename Container::const_iterator;

  JsonObjectBuilder() = default;

  explicit JsonObjectBuilder(JsonObject object);

  JsonObjectBuilder(const JsonObjectBuilder&) = delete;
  JsonObjectBuilder(JsonObjectBuilder&&) = default;

  JsonObjectBuilder& operator=(const JsonObjectBuilder&) = delete;
  JsonObjectBuilder& operator=(JsonObjectBuilder&&) = default;

  bool empty() const { return impl_.get().empty(); }

  size_type size() const { return impl_.get().size(); }

  iterator begin() { return impl_.mutable_get().begin(); }

  const_iterator begin() const { return impl_.get().begin(); }

  iterator end() { return impl_.mutable_get().end(); }

  const_iterator end() const { return impl_.get().end(); }

  void clear() { impl_.mutable_get().clear(); }

  template <typename K>
  iterator find(const K& key) {
    return impl_.mutable_get().find(key);
  }

  template <typename K>
  bool contains(const K& key) {
    return impl_.mutable_get().contains(key);
  }

  template <typename P>
  std::pair<iterator, bool> insert(P&& value) {
    return impl_.mutable_get().insert(std::forward<P>(value));
  }

  template <typename InputIterator>
  void insert(InputIterator first, InputIterator last) {
    impl_.mutable_get().insert(std::move(first), std::move(last));
  }

  void insert(std::initializer_list<value_type> il);

  template <typename M>
  std::pair<iterator, bool> insert_or_assign(const key_type& k, M&& obj) {
    return impl_.mutable_get().insert_or_assign(k, std::forward<M>(obj));
  }

  template <typename M>
  std::pair<iterator, bool> insert_or_assign(key_type&& k, M&& obj) {
    return impl_.mutable_get().insert_or_assign(std::move(k),
                                                std::forward<M>(obj));
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
    return impl_.mutable_get().try_emplace(key, std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
    return impl_.mutable_get().try_emplace(std::move(key),
                                           std::forward<Args>(args)...);
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    return impl_.mutable_get().emplace(std::forward<Args>(args)...);
  }

  template <typename K>
  size_type erase(const K& k) {
    return impl_.mutable_get().erase(k);
  }

  void erase(const_iterator pos) { impl_.mutable_get().erase(std::move(pos)); }

  iterator erase(const_iterator first, const_iterator last) {
    return impl_.mutable_get().erase(std::move(first), std::move(last));
  }

  void reserve(size_type n) {
    if (n != 0) {
      impl_.mutable_get().reserve(n);
    }
  }

  JsonObject Build() &&;

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator JsonObject() &&;

 private:
  internal::CopyOnWrite<Container> impl_;
};

// `cel::JsonObject` is a read-only mapping of `cel::JsonString` to `cel::Json`.
class ABSL_ATTRIBUTE_TRIVIAL_ABI JsonObject final {
 private:
  using Container = absl::flat_hash_map<JsonString, Json>;

 public:
  using key_type = typename Container::key_type;
  using mapped_type = typename Container::mapped_type;
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  using difference_type = typename Container::difference_type;
  using reference = typename Container::reference;
  using const_reference = typename Container::const_reference;
  using pointer = typename Container::pointer;
  using const_pointer = typename Container::const_pointer;
  using iterator = typename Container::iterator;
  using const_iterator = typename Container::const_iterator;

  JsonObject() : impl_(Empty()) {}

  JsonObject(const JsonObject&) = default;
  JsonObject(JsonObject&&) = default;

  JsonObject& operator=(const JsonObject&) = default;
  JsonObject& operator=(JsonObject&&) = default;

  bool empty() const { return impl_.get().empty(); }

  size_type size() const { return impl_.get().size(); }

  const_iterator begin() const { return impl_.get().begin(); }

  const_iterator cbegin() const { return impl_.get().cbegin(); }

  const_iterator end() const { return impl_.get().end(); }

  const_iterator cend() const { return impl_.get().cend(); }

  template <typename K>
  const_iterator find(const K& key) const {
    return impl_.get().find(key);
  }

  template <typename K>
  bool contains(const K& key) const {
    return impl_.get().contains(key);
  }

  friend bool operator==(const JsonObject& lhs, const JsonObject& rhs);

  friend bool operator!=(const JsonObject& lhs, const JsonObject& rhs);

  template <typename H>
  friend H AbslHashValue(H state, const JsonObject& json_object);

 private:
  friend class JsonObjectBuilder;

  static internal::CopyOnWrite<Container> Empty();

  explicit JsonObject(internal::CopyOnWrite<Container> impl)
      : impl_(std::move(impl)) {
    if (impl_.get().empty()) {
      impl_ = Empty();
    }
  }

  internal::CopyOnWrite<Container> impl_;
};

// Json is now fully declared.

inline JsonArrayBuilder::JsonArrayBuilder(JsonArray array)
    : impl_(std::move(array.impl_)) {}

inline JsonObjectBuilder::JsonObjectBuilder(JsonObject object)
    : impl_(std::move(object.impl_)) {}

inline void JsonObjectBuilder::insert(std::initializer_list<value_type> il) {
  impl_.mutable_get().insert(il);
}

inline void JsonArrayBuilder::push_back(Json json) {
  impl_.mutable_get().push_back(std::move(json));
}

inline JsonArray JsonArrayBuilder::Build() && {
  return JsonArray(std::move(impl_));
}

inline JsonArrayBuilder::operator JsonArray() && {
  return std::move(*this).Build();
}

inline JsonObject JsonObjectBuilder::Build() && {
  return JsonObject(std::move(impl_));
}

inline JsonObjectBuilder::operator JsonObject() && {
  return std::move(*this).Build();
}

inline bool operator==(const JsonArray& lhs, const JsonArray& rhs) {
  return lhs.impl_.get() == rhs.impl_.get();
}

inline bool operator!=(const JsonArray& lhs, const JsonArray& rhs) {
  return lhs.impl_.get() != rhs.impl_.get();
}

template <typename H>
inline H AbslHashValue(H state, const JsonArray& json_array) {
  return H::combine(std::move(state), json_array.impl_.get());
}

inline bool operator==(const JsonObject& lhs, const JsonObject& rhs) {
  return lhs.impl_.get() == rhs.impl_.get();
}

inline bool operator!=(const JsonObject& lhs, const JsonObject& rhs) {
  return lhs.impl_.get() != rhs.impl_.get();
}

template <typename H>
inline H AbslHashValue(H state, const JsonObject& json_object) {
  return H::combine(std::move(state), json_object.impl_.get());
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_JSON_H_

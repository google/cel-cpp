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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_LINKED_HASH_MAP_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_LINKED_HASH_MAP_H_

#include <list>
#include <utility>

#include "absl/container/flat_hash_set.h"

namespace cel::internal {

// Implementation of a hashmap which preserves insertion order. Currently it
// uses std::list and absl::flat_hash_set. It does not implement the entire
// specification, only what we need. Additionally it doesn't perform fancy
// SFINAE for overloads.
template <typename Key, typename Value,
          typename Hasher = typename absl::flat_hash_set<Key>::hasher,
          typename KeyEqual =
              typename absl::flat_hash_set<Key, Hasher>::key_equal,
          typename Allocator = std::allocator<std::pair<const Key, Value>>>
class LinkedHashMap final {
 public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using hasher = Hasher;
  using key_equal = KeyEqual;
  using allocator_type = Allocator;
  using difference_type = ptrdiff_t;

 private:
  using list_type = std::list<value_type, allocator_type>;

  class WrappedHasher {
   public:
    using is_transparent = void;

    WrappedHasher() = default;
    WrappedHasher(const WrappedHasher&) = default;
    WrappedHasher& operator=(const WrappedHasher&) = default;
    WrappedHasher(WrappedHasher&&) = default;
    WrappedHasher& operator=(WrappedHasher&&) = default;

    explicit WrappedHasher(Hasher hasher) : hasher_(std::move(hasher)) {}

    template <typename... Args>
    inline size_t operator()(Args&&... args) const {
      return hasher_(ToKey(args)...);
    }

   private:
    template <typename K>
    static const K& ToKey(const K& key) {
      return key;
    }

    static const key_type& ToKey(typename list_type::const_iterator it) {
      return it->first;
    }

    static const key_type& ToKey(typename list_type::iterator it) {
      return it->first;
    }

    Hasher hasher_;
  };

  class WrappedKeyEqual {
   public:
    using is_transparent = void;

    WrappedKeyEqual() = default;
    WrappedKeyEqual(const WrappedKeyEqual&) = default;
    WrappedKeyEqual& operator=(const WrappedKeyEqual&) = default;
    WrappedKeyEqual(WrappedKeyEqual&&) = default;
    WrappedKeyEqual& operator=(WrappedKeyEqual&&) = default;

    explicit WrappedKeyEqual(KeyEqual key_equal)
        : key_equal_(std::move(key_equal)) {}

    template <typename... Args>
    inline bool operator()(Args&&... args) const {
      return key_equal_(ToKey(args)...);
    }

   private:
    template <typename K>
    static const K& ToKey(const K& key) {
      return key;
    }

    static const key_type& ToKey(typename list_type::const_iterator it) {
      return it->first;
    }

    static const key_type& ToKey(typename list_type::iterator it) {
      return it->first;
    }

    KeyEqual key_equal_;
  };

  using set_type =
      absl::flat_hash_set<typename list_type::iterator, WrappedHasher,
                          WrappedKeyEqual, Allocator>;

 public:
  using iterator = typename list_type::iterator;
  using const_iterator = typename list_type::const_iterator;
  using reverse_iterator = typename list_type::reverse_iterator;
  using const_reverse_iterator = typename list_type::const_reverse_iterator;
  using reference = typename list_type::reference;
  using const_reference = typename list_type::const_reference;
  using size_type = typename list_type::size_type;

  LinkedHashMap() = default;
  LinkedHashMap(const LinkedHashMap&) = default;
  LinkedHashMap& operator=(const LinkedHashMap&) = default;
  LinkedHashMap(LinkedHashMap&&) = default;
  LinkedHashMap& operator=(LinkedHashMap&&) = default;

  explicit LinkedHashMap(const Allocator& allocator)
      : set_(allocator), list_(allocator) {}

  iterator begin() { return list_.begin(); }

  const_iterator begin() const { return list_.begin(); }

  const_iterator cbegin() const { return list_.cbegin(); }

  iterator end() { return list_.end(); }

  const_iterator end() const { return list_.end(); }

  const_iterator cend() const { return list_.cend(); }

  reverse_iterator rbegin() { return list_.rbegin(); }

  const_reverse_iterator rbegin() const { return list_.rbegin(); }

  const_reverse_iterator crbegin() const { return list_.crbegin(); }

  reverse_iterator rend() { return list_.rend(); }

  const_reverse_iterator rend() const { return list_.rend(); }

  const_reverse_iterator crend() const { return list_.crend(); }

  std::pair<iterator, bool> insert(const value_type& value) {
    auto existing = set_.find(value.first);
    if (existing != set_.end()) {
      return std::make_pair(*existing, false);
    }
    auto wrapped = list_.insert(list_.end(), value);
    set_.insert(wrapped);
    return std::make_pair(wrapped, true);
  }

  std::pair<iterator, bool> insert(value_type&& value) {
    auto existing = set_.find(value.first);
    if (existing != set_.end()) {
      return std::make_pair(*existing, false);
    }
    auto wrapped = list_.insert(list_.end(), std::move(value));
    set_.insert(wrapped);
    return std::make_pair(wrapped, true);
  }

  template <typename M>
  std::pair<iterator, bool> insert_or_assign(const key_type& key, M&& value) {
    auto existing = set_.find(key);
    if (existing != set_.end()) {
      (*existing)->second = std::forward<M>(value);
      return std::make_pair(*existing, false);
    }
    auto wrapped =
        list_.insert(list_.end(), value_type(key, std::forward<M>(value)));
    set_.insert(wrapped);
    return std::make_pair(wrapped, true);
  }

  template <typename M>
  std::pair<iterator, bool> insert_or_assign(key_type&& key, M&& value) {
    auto existing = set_.find(key);
    if (existing != set_.end()) {
      (*existing)->second = std::forward<M>(value);
      return std::make_pair(*existing, false);
    }
    auto wrapped = list_.insert(
        list_.end(),
        value_type(std::forward<key_type>(key), std::forward<M>(value)));
    set_.insert(wrapped);
    return std::make_pair(wrapped, true);
  }

  iterator find(const key_type& key) {
    auto existing = set_.find(key);
    if (existing == set_.end()) {
      return end();
    }
    return *existing;
  }

  template <typename K>
  iterator find(K&& key) {
    auto existing = set_.find(std::forward<K>(key));
    if (existing == set_.end()) {
      return end();
    }
    return *existing;
  }

  const_iterator find(const key_type& key) const {
    auto existing = set_.find(key);
    if (existing == set_.end()) {
      return end();
    }
    return *existing;
  }

  template <typename K>
  const_iterator find(K&& key) const {
    auto existing = set_.find(std::forward<K>(key));
    if (existing == set_.end()) {
      return end();
    }
    return *existing;
  }

  size_type size() const { return list_.size(); }

  bool empty() const { return list_.empty(); }

 private:
  set_type set_;
  list_type list_;
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_LINKED_HASH_MAP_H_

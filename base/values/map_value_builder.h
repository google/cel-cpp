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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_MAP_VALUE_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_MAP_VALUE_BUILDER_H_

#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/types/variant.h"
#include "base/memory.h"
#include "base/value_factory.h"
#include "base/values/list_value_builder.h"
#include "base/values/map_value.h"
#include "internal/overloaded.h"
#include "internal/status_macros.h"

namespace cel {

// Abstract interface for building MapValue.
//
// MapValueBuilderInterface is not reusable, once Build() is called the state
// of MapValueBuilderInterface is undefined.
class MapValueBuilderInterface {
 public:
  virtual ~MapValueBuilderInterface() = default;

  virtual std::string DebugString() const = 0;

  // Insert a new entry. Returns true if the key did not already exist and the
  // insertion was performed, false otherwise.
  virtual absl::StatusOr<bool> Insert(Handle<Value> key,
                                      Handle<Value> value) = 0;

  // Update an already existing entry. Returns true if the key already existed
  // and the update was performed, false otherwise.
  virtual absl::StatusOr<bool> Update(const Handle<Value>& key,
                                      Handle<Value> value) = 0;

  // A combination of Insert and Update, where the entry is inserted if it
  // doesn't already exist or it is updated. Returns true if insertion occurred,
  // false otherwise.
  virtual absl::StatusOr<bool> InsertOrAssign(Handle<Value> key,
                                              Handle<Value> value) = 0;

  // Returns whether the given key has been inserted.
  virtual bool Has(const Handle<Value>& key) const = 0;

  virtual size_t size() const = 0;

  virtual bool empty() const { return size() == 0; }

  virtual void reserve(size_t size) {}

  virtual absl::StatusOr<Handle<MapValue>> Build() && = 0;

 protected:
  explicit MapValueBuilderInterface(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  ValueFactory& value_factory() const { return value_factory_; }

 private:
  ValueFactory& value_factory_;
};

// MapValueBuilder implements MapValueBuilderInterface, but is specialized for
// some types which have underlying C++ representations. When K and V is Value,
// MapValueBuilder has exactly the same methods as MapValueBuilderInterface.
// When K or V is not Value itself, each function that accepts Handle<Value>
// above also accepts Handle<K> or Handle<V> variants. When K or V has some
// underlying C++ representation, each function that accepts Handle<K> or
// Handle<V> above also accepts the underlying C++ representation.
//
// For example, MapValueBuilder<IntValue, StructValue>::Insert accepts
// Handle<Value>, Handle<IntValue> and int64_t as keys.
template <typename K, typename V>
class MapValueBuilder;

namespace base_internal {

// TODO(uncreated-issue/21): add checks ensuring keys and values match their expected
// types for all operations.

template <typename T>
struct MapKeyHasher {
  inline size_t operator()(const T& key) const { return absl::Hash<T>{}(key); }
};

template <typename T>
struct MapKeyHasher<Handle<T>> {
  inline size_t operator()(const Handle<T>& key) const {
    return absl::Hash<T>{}(*key);
  }
};

template <>
struct MapKeyHasher<Handle<Value>> {
  inline size_t operator()(const Handle<Value>& key) const {
    switch (key->kind()) {
      case ValueKind::kBool:
        return absl::Hash<BoolValue>{}(*key.As<BoolValue>());
      case ValueKind::kInt:
        return absl::Hash<IntValue>{}(*key.As<IntValue>());
      case ValueKind::kUint:
        return absl::Hash<UintValue>{}(*key.As<UintValue>());
      case ValueKind::kString:
        return absl::Hash<StringValue>{}(*key.As<StringValue>());
      default:
        ABSL_UNREACHABLE();
    }
  }
};

template <typename T>
struct MapKeyEqualer {
  inline bool operator()(const T& lhs, const T& rhs) const {
    return lhs == rhs;
  }
};

template <typename T>
struct MapKeyEqualer<Handle<T>> {
  inline bool operator()(const T& lhs, const T& rhs) const {
    return *lhs == *rhs;
  }
};

template <>
struct MapKeyEqualer<Handle<Value>> {
  inline bool operator()(const Handle<Value>& lhs,
                         const Handle<Value>& rhs) const {
    ABSL_ASSERT(lhs->kind() == rhs->kind());
    switch (lhs->kind()) {
      case ValueKind::kBool:
        return *lhs.As<BoolValue>() == *rhs.As<BoolValue>();
      case ValueKind::kInt:
        return *lhs.As<IntValue>() == *rhs.As<IntValue>();
      case ValueKind::kUint:
        return *lhs.As<UintValue>() == *rhs.As<UintValue>();
      case ValueKind::kString:
        return *lhs.As<StringValue>() == *rhs.As<StringValue>();
      default:
        ABSL_UNREACHABLE();
    }
  }
};

template <typename Map, typename KeyDebugStringer, typename ValueDebugStringer>
std::string ComposeMapValueDebugString(
    const Map& map, const KeyDebugStringer& key_debug_stringer,
    const ValueDebugStringer& value_debug_stringer) {
  std::string out;
  out.push_back('{');
  auto current = map.begin();
  if (current != map.end()) {
    out.append(key_debug_stringer(current->first));
    out.append(": ");
    out.append(value_debug_stringer(current->second));
    ++current;
    for (; current != map.end(); ++current) {
      out.append(", ");
      out.append(key_debug_stringer(current->first));
      out.append(": ");
      out.append(value_debug_stringer(current->second));
    }
  }
  out.push_back('}');
  return out;
}

// For MapValueBuilder we use a linked hash map to preserve insertion order.
// This mimics protobuf and ensures some reproducibility, making testing easier.

// Implementation used by MapValueBuilder when both the key and value are
// represented as Value and not some C++ primitive.
class DynamicMapValue final : public AbstractMapValue {
 public:
  using storage_type = absl::flat_hash_map<
      Handle<Value>, Handle<Value>, MapKeyHasher<Handle<Value>>,
      MapKeyEqualer<Handle<Value>>,
      Allocator<std::pair<const Handle<Value>, Handle<Value>>>>;

  DynamicMapValue(Handle<MapType> type, storage_type storage)
      : AbstractMapValue(std::move(type)), storage_(std::move(storage)) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const Handle<Value>& value) { return value->DebugString(); },
        [](const Handle<Value>& value) { return value->DebugString(); });
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  absl::StatusOr<absl::optional<Handle<Value>>> Get(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return absl::nullopt;
    }
    return existing->second;
  }

  absl::StatusOr<bool> Has(const Handle<Value>& key) const override {
    return storage_.find(key) != storage_.end();
  }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    ListValueBuilder<Value> keys(value_factory, type()->key());
    keys.reserve(size());
    for (const auto& current : storage_) {
      CEL_RETURN_IF_ERROR(keys.Add(current.first));
    }
    return std::move(keys).Build();
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<DynamicMapValue>();
  }

 private:
  storage_type storage_;
};

// Implementation used by MapValueBuilder when either the key, value, or both
// are represented as some C++ primitive.
template <typename K, typename V>
class StaticMapValue;

// Specialization for the key type being some C++ primitive.
template <typename K>
class StaticMapValue<K, void> final : public AbstractMapValue {
 public:
  using underlying_key_type = typename ValueTraits<K>::underlying_type;
  using key_type = std::conditional_t<std::is_void_v<underlying_key_type>,
                                      Handle<Value>, underlying_key_type>;
  using hash_map_type =
      absl::flat_hash_map<key_type, Handle<Value>, MapKeyHasher<key_type>,
                          MapKeyEqualer<key_type>,
                          Allocator<std::pair<const key_type, Handle<Value>>>>;

  StaticMapValue(Handle<MapType> type, hash_map_type storage)
      : AbstractMapValue(std::move(type)), storage_(std::move(storage)) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const underlying_key_type& value) {
          return ValueTraits<K>::DebugString(value);
        },
        [](const Handle<Value>& value) { return value->DebugString(); });
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  absl::StatusOr<absl::optional<Handle<Value>>> Get(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    auto existing = storage_.find(key.As<K>()->value());
    if (existing == storage_.end()) {
      return absl::nullopt;
    }
    return existing->second;
  }

  absl::StatusOr<bool> Has(const Handle<Value>& key) const override {
    return storage_.find(key.As<K>()->value()) != storage_.end();
  }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    ListValueBuilder<K> keys(
        value_factory,
        type()->key().template As<typename ValueTraits<K>::type_type>());
    keys.reserve(size());
    for (const auto& current : storage_) {
      CEL_RETURN_IF_ERROR(keys.Add(current.first));
    }
    return std::move(keys).Build();
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<StaticMapValue<K, void>>();
  }

 private:
  hash_map_type storage_;
};

// Specialization for the value type being some C++ primitive.
template <typename V>
class StaticMapValue<void, V> final : public AbstractMapValue {
 public:
  using underlying_value_type = typename ValueTraits<V>::underlying_type;
  using value_type = std::conditional_t<std::is_void_v<underlying_value_type>,
                                        Handle<Value>, underlying_value_type>;
  using hash_map_type = absl::flat_hash_map<
      Handle<Value>, value_type, MapKeyHasher<Handle<Value>>,
      MapKeyEqualer<Handle<Value>>,
      Allocator<std::pair<const Handle<Value>, value_type>>>;

  StaticMapValue(Handle<MapType> type, hash_map_type storage)
      : AbstractMapValue(std::move(type)), storage_(std::move(storage)) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const Handle<Value>& value) { return value->DebugString(); },
        [](const underlying_value_type& value) {
          return ValueTraits<V>::DebugString(value);
        });
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  absl::StatusOr<absl::optional<Handle<Value>>> Get(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return absl::nullopt;
    }
    return ValueTraits<V>::Wrap(value_factory, existing->second);
  }

  absl::StatusOr<bool> Has(const Handle<Value>& key) const override {
    return storage_.find(key) != storage_.end();
  }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    ListValueBuilder<Value> keys(value_factory, type()->key());
    keys.reserve(size());
    for (const auto& current : storage_) {
      CEL_RETURN_IF_ERROR(keys.Add(current.first));
    }
    return std::move(keys).Build();
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<StaticMapValue<void, V>>();
  }

 private:
  hash_map_type storage_;
};

// Specialization for the key and value types being some C++ primitive.
template <typename K, typename V>
class StaticMapValue final : public AbstractMapValue {
 public:
  using underlying_key_type = typename ValueTraits<K>::underlying_type;
  using key_type = std::conditional_t<std::is_void_v<underlying_key_type>,
                                      Handle<Value>, underlying_key_type>;
  using underlying_value_type = typename ValueTraits<V>::underlying_type;
  using value_type = std::conditional_t<std::is_void_v<underlying_value_type>,
                                        Handle<Value>, underlying_value_type>;
  using hash_map_type =
      absl::flat_hash_map<key_type, value_type, MapKeyHasher<key_type>,
                          MapKeyEqualer<key_type>,
                          Allocator<std::pair<const key_type, value_type>>>;

  StaticMapValue(Handle<MapType> type, hash_map_type storage)
      : AbstractMapValue(std::move(type)), storage_(std::move(storage)) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const underlying_key_type& value) {
          return ValueTraits<K>::DebugString(value);
        },
        [](const underlying_value_type& value) {
          return ValueTraits<V>::DebugString(value);
        });
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  absl::StatusOr<absl::optional<Handle<Value>>> Get(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    auto existing = storage_.find(key.As<K>()->value());
    if (existing == storage_.end()) {
      return absl::nullopt;
    }
    return ValueTraits<V>::Wrap(value_factory, existing->second);
  }

  absl::StatusOr<bool> Has(const Handle<Value>& key) const override {
    return storage_.find(key.As<K>()->value()) != storage_.end();
  }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    ListValueBuilder<K> keys(
        value_factory,
        type()->key().template As<typename ValueTraits<K>::type_type>());
    keys.reserve(size());
    for (const auto& current : storage_) {
      CEL_RETURN_IF_ERROR(keys.Add(current.first));
    }
    return std::move(keys).Build();
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<StaticMapValue<K, V>>();
  }

 private:
  hash_map_type storage_;
};

// ComposableMapType is a variant which represents either the MapType or the
// key and value Type for creating a MapType.
template <typename K, typename V>
using ComposableMapType =
    absl::variant<std::pair<Handle<K>, Handle<V>>, Handle<MapType>>;

// Create a MapType from ComposableMapType.
template <typename K, typename V>
absl::StatusOr<Handle<MapType>> ComposeMapType(
    ValueFactory& value_factory, ComposableMapType<K, V>&& composable) {
  return absl::visit(
      internal::Overloaded{
          [&value_factory](std::pair<Handle<K>, Handle<V>>&& key_value)
              -> absl::StatusOr<Handle<MapType>> {
            return value_factory.type_factory().CreateMapType(
                std::move(key_value).first, std::move(key_value).second);
          },
          [](Handle<MapType>&& map) -> absl::StatusOr<Handle<MapType>> {
            return std::move(map);
          },
      },
      std::move(composable));
}

// Implementation of MapValueBuilder. Specialized to store some value types are
// C++ primitives, avoiding Handle overhead. Anything that does not have a C++
// primitive is stored as Handle<Value>.
template <typename K, typename V, typename UK = void, typename UV = void>
class MapValueBuilderImpl;

// Specialization for the key and value types neither of which are Value itself
// and have no C++ primitive types.
template <typename K, typename V>
class MapValueBuilderImpl<K, V, void, void> : public MapValueBuilderInterface {
 public:
  static_assert(std::is_base_of_v<Value, K>);
  static_assert(std::is_base_of_v<Value, V>);

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<typename ValueTraits<K>::type_type> key,
                      Handle<typename ValueTraits<V>::type_type> value)
      : MapValueBuilderInterface(value_factory),
        type_(std::make_pair(std::move(key), std::move(value))),
        storage_(Allocator<std::pair<const Handle<Value>, Handle<Value>>>{
            value_factory.memory_manager()}) {}

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<MapType> type)
      : MapValueBuilderInterface(value_factory),
        type_(std::move(type)),
        storage_(Allocator<std::pair<const Handle<Value>, Handle<Value>>>{
            value_factory.memory_manager()}) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const Handle<Value>& value) { return value->DebugString(); },
        [](const Handle<Value>& value) { return value->DebugString(); });
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, Handle<Value> value) override {
    return Insert(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> Insert(Handle<K> key, Handle<V> value) {
    return storage_.insert(std::make_pair(std::move(key), std::move(value)))
        .second;
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key,
                              Handle<Value> value) override {
    return Update(key.As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> Update(const Handle<K>& key, Handle<V> value) {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return false;
    }
    existing->second = std::move(value);
    return true;
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key,
                                      Handle<Value> value) override {
    return InsertOrAssign(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<K> key, Handle<V> value) {
    return storage_.insert_or_assign(std::move(key), std::move(value)).second;
  }

  bool Has(const Handle<Value>& key) const override { return Has(key.As<K>()); }

  bool Has(const Handle<K>& key) const {
    return storage_.find(key) != storage_.end();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<MapValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         ComposeMapType(value_factory(), std::move(type_)));
    return value_factory().template CreateMapValue<DynamicMapValue>(
        std::move(type), std::move(storage_));
  }

 private:
  ComposableMapType<typename ValueTraits<K>::type_type,
                    typename ValueTraits<V>::type_type>
      type_;
  absl::flat_hash_map<Handle<Value>, Handle<Value>, MapKeyHasher<Handle<Value>>,
                      MapKeyEqualer<Handle<Value>>,
                      Allocator<std::pair<const Handle<Value>, Handle<Value>>>>
      storage_;
};

// Specialization for key type being something derived from Value with no C++
// primitive representation and value type being Value itself.
template <typename K>
class MapValueBuilderImpl<K, Value, void, void>
    : public MapValueBuilderInterface {
 public:
  static_assert(std::is_base_of_v<Value, K>);

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<typename ValueTraits<K>::type_type> key,
                      Handle<Type> value)
      : MapValueBuilderInterface(value_factory),
        key_(std::move(key)),
        value_(std::move(value)),
        storage_(Allocator<std::pair<const Handle<Value>, Handle<Value>>>{
            value_factory.memory_manager()}) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const Handle<Value>& value) { return value->DebugString(); },
        [](const Handle<Value>& value) { return value->DebugString(); });
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, Handle<Value> value) override {
    return Insert(std::move(key).As<K>(), std::move(value));
  }

  absl::StatusOr<bool> Insert(Handle<K> key, Handle<Value> value) {
    return storage_.insert(std::make_pair(std::move(key), std::move(value)))
        .second;
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key,
                              Handle<Value> value) override {
    return Update(key.As<K>(), std::move(value));
  }

  absl::StatusOr<bool> Update(const Handle<K>& key, Handle<Value> value) {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return false;
    }
    existing->second = std::move(value);
    return true;
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key,
                                      Handle<Value> value) override {
    return InsertOrAssign(std::move(key).As<K>(), std::move(value));
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<K> key, Handle<Value> value) {
    return storage_.insert_or_assign(std::move(key), std::move(value)).second;
  }

  bool Has(const Handle<Value>& key) const override { return Has(key.As<K>()); }

  bool Has(const Handle<K>& key) const {
    return storage_.find(key) != storage_.end();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<MapValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(
        auto type, value_factory().type_factory().CreateMapType(key_, value_));
    return value_factory().template CreateMapValue<DynamicMapValue>(
        std::move(type), std::move(storage_));
  }

 private:
  Handle<typename ValueTraits<K>::type_type> key_;
  Handle<Type> value_;
  absl::flat_hash_map<Handle<Value>, Handle<Value>, MapKeyHasher<Handle<Value>>,
                      MapKeyEqualer<Handle<Value>>,
                      Allocator<std::pair<const Handle<Value>, Handle<Value>>>>
      storage_;
};

// Specialization for key type being Value itself and value type being something
// derived from Value with no C++ primitive representation.
template <typename V>
class MapValueBuilderImpl<Value, V, void, void>
    : public MapValueBuilderInterface {
 public:
  static_assert(std::is_base_of_v<Value, V>);

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<Type> key,
                      Handle<typename ValueTraits<V>::type_type> value)
      : MapValueBuilderInterface(value_factory),
        key_(std::move(key)),
        value_(std::move(value)),
        storage_(Allocator<std::pair<const Handle<Value>, Handle<Value>>>{
            value_factory.memory_manager()}) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const Handle<Value>& value) { return value->DebugString(); },
        [](const Handle<Value>& value) { return value->DebugString(); });
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, Handle<Value> value) override {
    return Insert(std::move(key), std::move(value).As<V>());
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, Handle<V> value) {
    return storage_.insert(std::make_pair(std::move(key), std::move(value)))
        .second;
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key,
                              Handle<Value> value) override {
    return Update(key, std::move(value).As<V>());
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key, Handle<V> value) {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return false;
    }
    existing->second = std::move(value);
    return true;
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key,
                                      Handle<Value> value) override {
    return InsertOrAssign(std::move(key), std::move(value).As<V>());
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key, Handle<V> value) {
    return storage_.insert_or_assign(std::move(key), std::move(value)).second;
  }

  bool Has(const Handle<Value>& key) const override {
    return storage_.find(key) != storage_.end();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<MapValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(
        auto type, value_factory().type_factory().CreateMapType(key_, value_));
    return value_factory().template CreateMapValue<DynamicMapValue>(
        std::move(type), std::move(storage_));
  }

 private:
  Handle<Type> key_;
  Handle<typename ValueTraits<V>::type_type> value_;
  absl::flat_hash_map<Handle<Value>, Handle<Value>, MapKeyHasher<Handle<Value>>,
                      MapKeyEqualer<Handle<Value>>,
                      Allocator<std::pair<const Handle<Value>, Handle<Value>>>>
      storage_;
};

// Specialization for key type being Value itself and value type has some C++
// primitive representation.
template <typename V, typename UV>
class MapValueBuilderImpl<Value, V, void, UV>
    : public MapValueBuilderInterface {
 public:
  static_assert(std::is_base_of_v<Value, V>);

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<Type> key,
                      Handle<typename ValueTraits<V>::type_type> value)
      : MapValueBuilderInterface(value_factory),
        type_(std::make_pair(std::move(key), std::move(value))),
        storage_(Allocator<std::pair<const Handle<Value>, Handle<Value>>>{
            value_factory.memory_manager()}) {}

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<MapType> type)
      : MapValueBuilderInterface(value_factory),
        type_(std::move(type)),
        storage_(Allocator<std::pair<const Handle<Value>, Handle<Value>>>{
            value_factory.memory_manager()}) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const Handle<Value>& value) { return value->DebugString(); },
        [](const UV& value) { return ValueTraits<V>::DebugString(value); });
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, Handle<Value> value) override {
    return Insert(std::move(key), std::move(value).As<V>());
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, Handle<V> value) {
    return Insert(std::move(key), value->value());
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, UV value) {
    return storage_.insert(std::make_pair(std::move(key), std::move(value)))
        .second;
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key,
                              Handle<Value> value) override {
    return Update(key, std::move(value).As<V>());
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key, Handle<V> value) {
    return Update(key, value->value());
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key, UV value) {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return false;
    }
    existing->second = std::move(value);
    return true;
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key,
                                      Handle<Value> value) override {
    return InsertOrAssign(std::move(key), std::move(value).As<V>());
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key, Handle<V> value) {
    return InsertOrAssign(std::move(key), value->value());
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key, UV value) {
    return storage_.insert_or_assign(std::move(key), std::move(value)).second;
  }

  bool Has(const Handle<Value>& key) const override {
    return storage_.find(key) != storage_.end();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<MapValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         ComposeMapType(value_factory(), std::move(type_)));
    return value_factory().template CreateMapValue<StaticMapValue<void, V>>(
        std::move(type), std::move(storage_));
  }

 private:
  ComposableMapType<Type, typename ValueTraits<V>::type_type> type_;
  absl::flat_hash_map<Handle<Value>, UV, MapKeyHasher<Handle<Value>>,
                      MapKeyEqualer<Handle<Value>>,
                      Allocator<std::pair<const Handle<Value>, UV>>>
      storage_;
};

// Specialization for key type and value type being Value itself.
template <>
class MapValueBuilderImpl<Value, Value, void, void>
    : public MapValueBuilderInterface {
 public:
  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<Type> key, Handle<Type> value)
      : MapValueBuilderInterface(value_factory),
        type_(std::make_pair(std::move(key), std::move(value))),
        storage_(Allocator<std::pair<const Handle<Value>, Handle<Value>>>{
            value_factory.memory_manager()}) {}

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<MapType> type)
      : MapValueBuilderInterface(value_factory),
        type_(std::move(type)),
        storage_(Allocator<std::pair<const Handle<Value>, Handle<Value>>>{
            value_factory.memory_manager()}) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const Handle<Value>& value) { return value->DebugString(); },
        [](const Handle<Value>& value) { return value->DebugString(); });
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, Handle<Value> value) override {
    return storage_.insert(std::make_pair(std::move(key), std::move(value)))
        .second;
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key,
                              Handle<Value> value) override {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return false;
    }
    existing->second = std::move(value);
    return true;
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key,
                                      Handle<Value> value) override {
    return storage_.insert_or_assign(std::move(key), std::move(value)).second;
  }

  bool Has(const Handle<Value>& key) const override {
    return storage_.find(key) != storage_.end();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<MapValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         ComposeMapType(value_factory(), std::move(type_)));
    return value_factory().template CreateMapValue<DynamicMapValue>(
        std::move(type), std::move(storage_));
  }

 private:
  ComposableMapType<Type, Type> type_;
  absl::flat_hash_map<Handle<Value>, Handle<Value>, MapKeyHasher<Handle<Value>>,
                      MapKeyEqualer<Handle<Value>>,
                      Allocator<std::pair<const Handle<Value>, Handle<Value>>>>
      storage_;
};

// Specialization for key type having some C++ primitive representation and
// value type not being Value itself.
template <typename K, typename V, typename UK>
class MapValueBuilderImpl<K, V, UK, void> : public MapValueBuilderInterface {
 public:
  static_assert(std::is_base_of_v<Value, K>);
  static_assert(std::is_same_v<UK, typename ValueTraits<K>::underlying_type>);
  static_assert(std::is_base_of_v<Value, V>);

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<typename ValueTraits<K>::type_type> key,
                      Handle<typename ValueTraits<V>::type_type> value)
      : MapValueBuilderInterface(value_factory),
        type_(std::make_pair(std::move(key), std::move(value))),
        storage_(Allocator<std::pair<const UK, Handle<Value>>>{
            value_factory.memory_manager()}) {}

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<MapType> type)
      : MapValueBuilderInterface(value_factory),
        type_(std::move(type)),
        storage_(Allocator<std::pair<const UK, Handle<Value>>>{
            value_factory.memory_manager()}) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const UK& value) { return ValueTraits<K>::DebugString(value); },
        [](const Handle<Value>& value) { return value->DebugString(); });
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, Handle<Value> value) override {
    return Insert(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> Insert(Handle<K> key, Handle<V> value) {
    return Insert(key->value(), std::move(value));
  }

  absl::StatusOr<bool> Insert(UK key, Handle<V> value) {
    return storage_.insert(std::make_pair(std::move(key), std::move(value)))
        .second;
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key,
                              Handle<Value> value) override {
    return Update(key.As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> Update(const Handle<K>& key, Handle<V> value) {
    return Update(key->value(), std::move(value));
  }

  absl::StatusOr<bool> Update(const UK& key, Handle<V> value) {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return false;
    }
    existing->second = std::move(value);
    return true;
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key,
                                      Handle<Value> value) override {
    return InsertOrAssign(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<K> key, Handle<V> value) {
    return InsertOrAssign(key->value(), std::move(value));
  }

  absl::StatusOr<bool> InsertOrAssign(UK key, Handle<V> value) {
    return storage_.insert_or_assign(std::move(key), std::move(value)).second;
  }

  bool Has(const Handle<Value>& key) const override { return Has(key.As<K>()); }

  bool Has(const Handle<K>& key) const { return Has(key->value()); }

  bool Has(const UK& key) const { return storage_.find(key) != storage_.end(); }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<MapValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         ComposeMapType(value_factory(), std::move(type_)));
    return value_factory().template CreateMapValue<StaticMapValue<K, void>>(
        std::move(type), std::move(storage_));
  }

 private:
  ComposableMapType<typename ValueTraits<K>::type_type,
                    typename ValueTraits<V>::type_type>
      type_;
  absl::flat_hash_map<UK, Handle<Value>, MapKeyHasher<UK>, MapKeyEqualer<UK>,
                      Allocator<std::pair<const UK, Handle<Value>>>>
      storage_;
};

// Specialization for key type not being Value itself and value type has some
// C++ primitive representation.
template <typename K, typename V, typename UV>
class MapValueBuilderImpl<K, V, void, UV> : public MapValueBuilderInterface {
 public:
  static_assert(std::is_base_of_v<Value, K>);
  static_assert(std::is_base_of_v<Value, V>);
  static_assert(std::is_same_v<UV, typename ValueTraits<V>::underlying_type>);

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<typename ValueTraits<K>::type_type> key,
                      Handle<typename ValueTraits<V>::type_type> value)
      : MapValueBuilderInterface(value_factory),
        type_(std::make_pair(std::move(key), std::move(value))),
        storage_(Allocator<std::pair<const Handle<Value>, UV>>{
            value_factory.memory_manager()}) {}

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<MapType> type)
      : MapValueBuilderInterface(value_factory),
        type_(std::move(type)),
        storage_(Allocator<std::pair<const Handle<Value>, UV>>{
            value_factory.memory_manager()}) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const Handle<Value>& value) { return value->DebugString(); },
        [](const UV& value) { return ValueTraits<V>::DebugString(value); });
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, Handle<Value> value) override {
    return Insert(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> Insert(Handle<K> key, Handle<V> value) {
    return Insert(std::move(key), value->value());
  }

  absl::StatusOr<bool> Insert(Handle<K> key, UV value) {
    return storage_.insert(std::make_pair(std::move(key), std::move(value)))
        .second;
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key,
                              Handle<Value> value) override {
    return Update(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> Update(const Handle<K>& key, Handle<V> value) {
    return Update(key, value->value());
  }

  absl::StatusOr<bool> Update(const Handle<K>& key, UV value) {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return false;
    }
    existing->second = std::move(value);
    return true;
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key,
                                      Handle<Value> value) override {
    return InsertOrAssign(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<K> key, Handle<V> value) {
    return InsertOrAssign(std::move(key), value->value());
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<K> key, UV value) {
    return storage_.insert_or_assign(std::move(key), std::move(value)).second;
  }

  bool Has(const Handle<Value>& key) const override { return Has(key.As<K>()); }

  bool Has(const Handle<K>& key) const {
    return storage_.find(key) != storage_.end();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<MapValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         ComposeMapType(value_factory(), std::move(type_)));
    return value_factory().template CreateMapValue<StaticMapValue<void, V>>(
        std::move(type), std::move(storage_));
  }

 private:
  ComposableMapType<typename ValueTraits<K>::type_type,
                    typename ValueTraits<V>::type_type>
      type_;
  absl::flat_hash_map<Handle<Value>, UV, MapKeyHasher<Handle<Value>>,
                      MapKeyEqualer<Handle<Value>>,
                      Allocator<std::pair<const Handle<Value>, UV>>>
      storage_;
};

// Specialization for key and value types having some C++ primitive
// representation.
template <typename K, typename V, typename UK, typename UV>
class MapValueBuilderImpl : public MapValueBuilderInterface {
 public:
  static_assert(std::is_base_of_v<Value, K>);
  static_assert(std::is_same_v<UK, typename ValueTraits<K>::underlying_type>);
  static_assert(std::is_base_of_v<Value, V>);
  static_assert(std::is_same_v<UV, typename ValueTraits<V>::underlying_type>);

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<typename ValueTraits<K>::type_type> key,
                      Handle<typename ValueTraits<V>::type_type> value)
      : MapValueBuilderInterface(value_factory),
        type_(std::make_pair(std::move(key), std::move(value))),
        storage_(Allocator<std::pair<const UK, UV>>{
            value_factory.memory_manager()}) {}

  MapValueBuilderImpl(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                      Handle<MapType> type)
      : MapValueBuilderInterface(value_factory),
        type_(std::move(type)),
        storage_(Allocator<std::pair<const UK, UV>>{
            value_factory.memory_manager()}) {}

  std::string DebugString() const override {
    return ComposeMapValueDebugString(
        storage_,
        [](const UK& value) { return ValueTraits<K>::DebugString(value); },
        [](const UV& value) { return ValueTraits<V>::DebugString(value); });
  }

  absl::StatusOr<bool> Insert(Handle<Value> key, Handle<Value> value) override {
    return Insert(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> Insert(Handle<K> key, Handle<V> value) {
    return Insert(key->value(), value->value());
  }

  absl::StatusOr<bool> Insert(Handle<K> key, UV value) {
    return Insert(key->value(), std::move(value));
  }

  absl::StatusOr<bool> Insert(UK key, Handle<V> value) {
    return Insert(std::move(key), value->value());
  }

  absl::StatusOr<bool> Insert(UK key, UV value) {
    return storage_.insert(std::make_pair(std::move(key), std::move(value)))
        .second;
  }

  absl::StatusOr<bool> Update(const Handle<Value>& key,
                              Handle<Value> value) override {
    return Update(key.As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> Update(const Handle<K>& key, Handle<V> value) {
    return Update(key->value(), value->value());
  }

  absl::StatusOr<bool> Update(const Handle<K>& key, V value) {
    return Update(key->value(), std::move(value));
  }

  absl::StatusOr<bool> Update(const UK& key, Handle<V> value) {
    return Update(key, value->value());
  }

  absl::StatusOr<bool> Update(const UK& key, UV value) {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return false;
    }
    existing->second = std::move(value);
    return true;
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<Value> key,
                                      Handle<Value> value) override {
    return InsertOrAssign(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<K> key, Handle<V> value) {
    return InsertOrAssign(key->value(), value->value());
  }

  absl::StatusOr<bool> InsertOrAssign(Handle<K> key, UV value) {
    return InsertOrAssign(key->value(), std::move(value));
  }

  absl::StatusOr<bool> InsertOrAssign(UK key, Handle<V> value) {
    return InsertOrAssign(std::move(key), value->value());
  }

  absl::StatusOr<bool> InsertOrAssign(UK key, UV value) {
    return storage_.insert_or_assign(std::move(key), std::move(value)).second;
  }

  bool Has(const Handle<Value>& key) const override { return Has(key.As<K>()); }

  bool Has(const Handle<K>& key) const { return Has(key->value()); }

  bool Has(const UK& key) const { return storage_.find(key) != storage_.end(); }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<MapValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         ComposeMapType(value_factory(), std::move(type_)));
    return value_factory().template CreateMapValue<StaticMapValue<K, V>>(
        std::move(type), std::move(storage_));
  }

 private:
  ComposableMapType<typename ValueTraits<K>::type_type,
                    typename ValueTraits<V>::type_type>
      type_;
  absl::flat_hash_map<UK, UV, MapKeyHasher<UK>, MapKeyEqualer<UK>,
                      Allocator<std::pair<const UK, UV>>>
      storage_;
};

}  // namespace base_internal

template <typename K, typename V>
class MapValueBuilder final
    : public base_internal::MapValueBuilderImpl<
          K, V, typename base_internal::ValueTraits<K>::underlying_type,
          typename base_internal::ValueTraits<V>::underlying_type> {
 private:
  using Impl = base_internal::MapValueBuilderImpl<
      K, V, typename base_internal::ValueTraits<K>::underlying_type,
      typename base_internal::ValueTraits<V>::underlying_type>;

 public:
  using Impl::Impl;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_MAP_VALUE_BUILDER_H_

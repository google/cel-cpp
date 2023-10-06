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

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
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

  // Inserts the key and value as an entry, increasing its size by 1. Returns OK
  // if the entry was added successfully, an error otherwise. Errors occur when
  // the type of the key does not match the expected key type of the map
  // being built or the type of the value does not match the expected value type
  // of the map being built. The types match if the expected type is the same or
  // is dyn.
  //
  // IMPORTANT: Attempting to add an entry for which an
  // heterogeneously equivalent key already exists is an error.
  //
  // NOTE: Any error returned should be treated as fatal to any ongoing
  // evaluation, that is the evaluation should stop. The returned error should
  // not be used for short-circuiting.
  virtual absl::Status Put(Handle<Value> key, Handle<Value> value) = 0;

  virtual size_t Size() const = 0;

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual void Reserve(size_t size) {}

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

  absl::StatusOr<std::pair<Handle<Value>, bool>> FindImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return std::make_pair(Handle<Value>(), false);
    }
    return std::make_pair(existing->second, true);
  }

  absl::StatusOr<Handle<Value>> HasImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    return value_factory.CreateBoolValue(storage_.find(key) != storage_.end());
  }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    ListValueBuilder<Value> keys(value_factory, type()->key());
    keys.Reserve(size());
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

  absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    ListValueBuilder<K> keys(
        value_factory,
        type()->key().template As<typename ValueTraits<K>::type_type>());
    keys.Reserve(size());
    for (const auto& current : storage_) {
      CEL_RETURN_IF_ERROR(keys.Add(current.first));
    }
    return std::move(keys).Build();
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<StaticMapValue<K, void>>();
  }

 private:
  absl::StatusOr<std::pair<Handle<Value>, bool>> FindImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    auto existing = storage_.find(key.As<K>()->value());
    if (existing == storage_.end()) {
      return std::make_pair(Handle<Value>(), false);
    }
    return std::make_pair(existing->second, true);
  }

  absl::StatusOr<Handle<Value>> HasImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    return value_factory.CreateBoolValue(storage_.find(key.As<K>()->value()) !=
                                         storage_.end());
  }

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

  absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    ListValueBuilder<Value> keys(value_factory, type()->key());
    keys.Reserve(size());
    for (const auto& current : storage_) {
      CEL_RETURN_IF_ERROR(keys.Add(current.first));
    }
    return std::move(keys).Build();
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<StaticMapValue<void, V>>();
  }

 private:
  absl::StatusOr<std::pair<Handle<Value>, bool>> FindImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    auto existing = storage_.find(key);
    if (existing == storage_.end()) {
      return std::make_pair(Handle<Value>(), false);
    }
    return std::make_pair(ValueTraits<V>::Wrap(value_factory, existing->second),
                          true);
  }

  absl::StatusOr<Handle<Value>> HasImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    return value_factory.CreateBoolValue(storage_.find(key) != storage_.end());
  }

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

  absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const override {
    ListValueBuilder<K> keys(
        value_factory,
        type()->key().template As<typename ValueTraits<K>::type_type>());
    keys.Reserve(size());
    for (const auto& current : storage_) {
      CEL_RETURN_IF_ERROR(keys.Add(current.first));
    }
    return std::move(keys).Build();
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<StaticMapValue<K, V>>();
  }

 private:
  absl::StatusOr<std::pair<Handle<Value>, bool>> FindImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    auto existing = storage_.find(key.As<K>()->value());
    if (existing == storage_.end()) {
      return std::make_pair(Handle<Value>(), false);
    }
    return std::make_pair(ValueTraits<V>::Wrap(value_factory, existing->second),
                          true);
  }

  absl::StatusOr<Handle<Value>> HasImpl(
      ValueFactory& value_factory, const Handle<Value>& key) const override {
    return value_factory.CreateBoolValue(storage_.find(key.As<K>()->value()) !=
                                         storage_.end());
  }

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

template <typename K, typename V>
const Type& ComposableMapTypeKey(const ComposableMapType<K, V>& composable) {
  return absl::visit(
      internal::Overloaded{
          [](const std::pair<Handle<K>, Handle<V>>& key_value) -> const Type& {
            return *key_value.first;
          },
          [](const Handle<MapType>& map) -> const Type& { return *map->key(); },
      },
      composable);
}

template <typename K, typename V>
const Type& ComposableMapTypeValue(const ComposableMapType<K, V>& composable) {
  return absl::visit(
      internal::Overloaded{
          [](const std::pair<Handle<K>, Handle<V>>& key_value) -> const Type& {
            return *key_value.second;
          },
          [](const Handle<MapType>& map) -> const Type& {
            return *map->value();
          },
      },
      composable);
}

absl::Status CheckMapKey(const Type& expected_type, const Value& value);

absl::Status CheckMapValue(const Type& expected_type, const Value& value);

absl::Status CheckMapKeyAndValue(const Type& expected_key_type,
                                 const Type& expected_value_type,
                                 const Value& key_value,
                                 const Value& value_value);

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

  absl::Status Put(Handle<Value> key, Handle<Value> value) override {
    CEL_RETURN_IF_ERROR(CheckMapKeyAndValue(ComposableMapTypeKey(type_),
                                            ComposableMapTypeValue(type_), *key,
                                            *value));
    return Put(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::Status Put(Handle<K> key, Handle<V> value) {
    if (ABSL_PREDICT_TRUE(
            storage_.insert(std::make_pair(std::move(key), std::move(value)))
                .second)) {
      return absl::OkStatus();
    }
    return DuplicateKeyError();
  }

  size_t Size() const override { return storage_.size(); }

  bool IsEmpty() const override { return storage_.empty(); }

  void Reserve(size_t size) override { storage_.reserve(size); }

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

  absl::Status Put(Handle<Value> key, Handle<Value> value) override {
    CEL_RETURN_IF_ERROR(CheckMapKeyAndValue(*key_, *value_, *key, *value));
    return Put(std::move(key).As<K>(), std::move(value));
  }

  absl::Status Put(Handle<K> key, Handle<Value> value) {
    if (ABSL_PREDICT_TRUE(
            storage_.insert(std::make_pair(std::move(key), std::move(value)))
                .second)) {
      return absl::OkStatus();
    }
    return DuplicateKeyError();
  }

  size_t Size() const override { return storage_.size(); }

  bool IsEmpty() const override { return storage_.empty(); }

  void Reserve(size_t size) override { storage_.reserve(size); }

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

  absl::Status Put(Handle<Value> key, Handle<Value> value) override {
    CEL_RETURN_IF_ERROR(CheckMapKeyAndValue(*key_, *value_, *key, *value));
    return Put(std::move(key), std::move(value).As<V>());
  }

  absl::Status Put(Handle<Value> key, Handle<V> value) {
    if (ABSL_PREDICT_TRUE(
            storage_.insert(std::make_pair(std::move(key), std::move(value)))
                .second)) {
      return absl::OkStatus();
    }
    return DuplicateKeyError();
  }

  size_t Size() const override { return storage_.size(); }

  bool IsEmpty() const override { return storage_.empty(); }

  void Reserve(size_t size) override { storage_.reserve(size); }

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

  absl::Status Put(Handle<Value> key, Handle<Value> value) override {
    CEL_RETURN_IF_ERROR(CheckMapKeyAndValue(ComposableMapTypeKey(type_),
                                            ComposableMapTypeValue(type_), *key,
                                            *value));
    return Put(std::move(key), std::move(value).As<V>());
  }

  absl::Status Put(Handle<Value> key, Handle<V> value) {
    return Put(std::move(key), value->value());
  }

  absl::Status Put(Handle<Value> key, UV value) {
    if (ABSL_PREDICT_TRUE(
            storage_.insert(std::make_pair(std::move(key), std::move(value)))
                .second)) {
      return absl::OkStatus();
    }
    return DuplicateKeyError();
  }

  size_t Size() const override { return storage_.size(); }

  bool IsEmpty() const override { return storage_.empty(); }

  void Reserve(size_t size) override { storage_.reserve(size); }

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

  absl::Status Put(Handle<Value> key, Handle<Value> value) override {
    CEL_RETURN_IF_ERROR(CheckMapKeyAndValue(ComposableMapTypeKey(type_),
                                            ComposableMapTypeValue(type_), *key,
                                            *value));
    if (ABSL_PREDICT_TRUE(
            storage_.insert(std::make_pair(std::move(key), std::move(value)))
                .second)) {
      return absl::OkStatus();
    }
    return DuplicateKeyError();
  }

  size_t Size() const override { return storage_.size(); }

  bool IsEmpty() const override { return storage_.empty(); }

  void Reserve(size_t size) override { storage_.reserve(size); }

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

  absl::Status Put(Handle<Value> key, Handle<Value> value) override {
    CEL_RETURN_IF_ERROR(CheckMapKeyAndValue(ComposableMapTypeKey(type_),
                                            ComposableMapTypeValue(type_), *key,
                                            *value));
    return Put(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::Status Put(Handle<K> key, Handle<V> value) {
    return Put(key->value(), std::move(value));
  }

  absl::Status Put(UK key, Handle<V> value) {
    if (ABSL_PREDICT_TRUE(
            storage_.insert(std::make_pair(std::move(key), std::move(value)))
                .second)) {
      return absl::OkStatus();
    }
    return DuplicateKeyError();
  }

  size_t Size() const override { return storage_.size(); }

  bool IsEmpty() const override { return storage_.empty(); }

  void Reserve(size_t size) override { storage_.reserve(size); }

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

  absl::Status Put(Handle<Value> key, Handle<Value> value) override {
    CEL_RETURN_IF_ERROR(CheckMapKeyAndValue(ComposableMapTypeKey(type_),
                                            ComposableMapTypeValue(type_), *key,
                                            *value));
    return Put(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::Status Put(Handle<K> key, Handle<V> value) {
    return Put(std::move(key), value->value());
  }

  absl::Status Put(Handle<K> key, UV value) {
    if (ABSL_PREDICT_TRUE(
            storage_.insert(std::make_pair(std::move(key), std::move(value)))
                .second)) {
      return absl::OkStatus();
    }
    return DuplicateKeyError();
  }

  size_t Size() const override { return storage_.size(); }

  bool IsEmpty() const override { return storage_.empty(); }

  void Reserve(size_t size) override { storage_.reserve(size); }

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

  absl::Status Put(Handle<Value> key, Handle<Value> value) override {
    CEL_RETURN_IF_ERROR(CheckMapKeyAndValue(ComposableMapTypeKey(type_),
                                            ComposableMapTypeValue(type_), *key,
                                            *value));
    return Put(std::move(key).As<K>(), std::move(value).As<V>());
  }

  absl::Status Put(Handle<K> key, Handle<V> value) {
    return Put(key->value(), value->value());
  }

  absl::Status Put(Handle<K> key, UV value) {
    return Put(key->value(), std::move(value));
  }

  absl::Status Put(UK key, Handle<V> value) {
    return Put(std::move(key), value->value());
  }

  absl::Status Put(UK key, UV value) {
    if (ABSL_PREDICT_TRUE(
            storage_.insert(std::make_pair(std::move(key), std::move(value)))
                .second)) {
      return absl::OkStatus();
    }
    return DuplicateKeyError();
  }

  size_t Size() const override { return storage_.size(); }

  bool IsEmpty() const override { return storage_.empty(); }

  void Reserve(size_t size) override { storage_.reserve(size); }

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

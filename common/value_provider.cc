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

#include "common/value_provider.h"

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/values/thread_compatible_value_provider.h"
#include "common/values/thread_safe_value_provider.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

template <typename T>
struct MapValueKeyHash;
template <typename T>
struct MapValueKeyEqualTo;
template <typename K, typename V>
using ValueFlatHashMapFor =
    absl::flat_hash_map<K, V, MapValueKeyHash<K>, MapValueKeyEqualTo<K>>;
template <typename T>
struct MapValueKeyJson;

template <typename T>
struct MapValueKeyHash {
  // Used to enable heterogeneous operations in supporting containers.
  using is_transparent = void;

  size_t operator()(typename T::view_alternative_type value) const {
    return absl::HashOf(value);
  }
};

template <typename T>
struct MapValueKeyEqualTo {
  // Used to enable heterogeneous operations in supporting containers.
  using is_transparent = void;

  bool operator()(typename T::view_alternative_type lhs,
                  typename T::view_alternative_type rhs) const {
    return lhs == rhs;
  }
};

template <typename T>
struct MapValueKeyLess {
  bool operator()(typename T::view_alternative_type lhs,
                  typename T::view_alternative_type rhs) const {
    return lhs < rhs;
  }
};

template <>
struct MapValueKeyHash<Value> {
  // Used to enable heterogeneous operations in supporting containers.
  using is_transparent = void;

  size_t operator()(ValueView value) const {
    switch (value.kind()) {
      case ValueKind::kBool:
        return absl::HashOf(ValueKind::kBool, Cast<BoolValueView>(value));
      case ValueKind::kInt:
        return absl::HashOf(ValueKind::kInt, Cast<IntValueView>(value));
      case ValueKind::kUint:
        return absl::HashOf(ValueKind::kUint, Cast<UintValueView>(value));
      case ValueKind::kString:
        return absl::HashOf(ValueKind::kString, Cast<StringValueView>(value));
      default:
        ABSL_DLOG(FATAL) << "Invalid map key value: " << value;
        return 0;
    }
  }
};

template <>
struct MapValueKeyEqualTo<Value> {
  // Used to enable heterogeneous operations in supporting containers.
  using is_transparent = void;

  bool operator()(ValueView lhs, ValueView rhs) const {
    switch (lhs.kind()) {
      case ValueKind::kBool:
        switch (rhs.kind()) {
          case ValueKind::kBool:
            return Cast<BoolValueView>(lhs) == Cast<BoolValueView>(rhs);
          case ValueKind::kInt:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kUint:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kString:
            return false;
          default:
            ABSL_DLOG(FATAL) << "Invalid map key value: " << rhs;
            return false;
        }
      case ValueKind::kInt:
        switch (rhs.kind()) {
          case ValueKind::kInt:
            return Cast<IntValueView>(lhs) == Cast<IntValueView>(rhs);
          case ValueKind::kBool:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kUint:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kString:
            return false;
          default:
            ABSL_DLOG(FATAL) << "Invalid map key value: " << rhs;
            return false;
        }
      case ValueKind::kUint:
        switch (rhs.kind()) {
          case ValueKind::kUint:
            return Cast<UintValueView>(lhs) == Cast<UintValueView>(rhs);
          case ValueKind::kBool:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kInt:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kString:
            return false;
          default:
            ABSL_DLOG(FATAL) << "Invalid map key value: " << rhs;
            return false;
        }
      case ValueKind::kString:
        switch (rhs.kind()) {
          case ValueKind::kString:
            return Cast<StringValueView>(lhs) == Cast<StringValueView>(rhs);
          case ValueKind::kBool:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kInt:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kUint:
            return false;
          default:
            ABSL_DLOG(FATAL) << "Invalid map key value: " << rhs;
            return false;
        }
      default:
        ABSL_DLOG(FATAL) << "Invalid map key value: " << lhs;
        return false;
    }
  }
};

template <>
struct MapValueKeyLess<Value> {
  bool operator()(ValueView lhs, ValueView rhs) const {
    switch (lhs.kind()) {
      case ValueKind::kBool:
        switch (rhs.kind()) {
          case ValueKind::kBool:
            return Cast<BoolValueView>(lhs) < Cast<BoolValueView>(rhs);
          case ValueKind::kInt:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kUint:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kString:
            return true;
          default:
            ABSL_DLOG(FATAL) << "Invalid map key value: " << rhs;
            return false;
        }
      case ValueKind::kInt:
        switch (rhs.kind()) {
          case ValueKind::kInt:
            return Cast<IntValueView>(lhs) < Cast<IntValueView>(rhs);
          case ValueKind::kBool:
            return false;
          case ValueKind::kUint:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kString:
            return true;
          default:
            ABSL_DLOG(FATAL) << "Invalid map key value: " << rhs;
            return false;
        }
      case ValueKind::kUint:
        switch (rhs.kind()) {
          case ValueKind::kUint:
            return Cast<UintValueView>(lhs) < Cast<UintValueView>(rhs);
          case ValueKind::kBool:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kInt:
            return false;
          case ValueKind::kString:
            return true;
          default:
            ABSL_DLOG(FATAL) << "Invalid map key value: " << rhs;
            return false;
        }
      case ValueKind::kString:
        switch (rhs.kind()) {
          case ValueKind::kString:
            return Cast<StringValueView>(lhs) < Cast<StringValueView>(rhs);
          case ValueKind::kBool:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kInt:
            ABSL_FALLTHROUGH_INTENDED;
          case ValueKind::kUint:
            return false;
          default:
            ABSL_DLOG(FATAL) << "Invalid map key value: " << rhs;
            return false;
        }
      default:
        ABSL_DLOG(FATAL) << "Invalid map key value: " << lhs;
        return false;
    }
  }
};

template <>
struct MapValueKeyJson<BoolValue> {
  absl::Cord operator()(BoolValue value) const {
    return value.NativeValue() ? absl::Cord("true") : absl::Cord("false");
  }
};

template <>
struct MapValueKeyJson<IntValue> {
  absl::Cord operator()(IntValue value) const {
    return absl::Cord(absl::StrCat(value.NativeValue()));
  }
};

template <>
struct MapValueKeyJson<UintValue> {
  absl::Cord operator()(UintValue value) const {
    return absl::Cord(absl::StrCat(value.NativeValue()));
  }
};

template <>
struct MapValueKeyJson<StringValue> {
  absl::Cord operator()(const StringValue& value) const {
    return value.NativeCord();
  }
};

template <>
struct MapValueKeyJson<Value> {
  absl::Cord operator()(const Value& value) const {
    switch (value.kind()) {
      case ValueKind::kBool:
        return MapValueKeyJson<BoolValue>{}(Cast<BoolValue>(value));
      case ValueKind::kInt:
        return MapValueKeyJson<IntValue>{}(Cast<IntValue>(value));
      case ValueKind::kUint:
        return MapValueKeyJson<UintValue>{}(Cast<UintValue>(value));
      case ValueKind::kString:
        return MapValueKeyJson<StringValue>{}(Cast<StringValue>(value));
      default:
        ABSL_DLOG(FATAL) << "Invalid map key value: " << value;
        return absl::Cord();
    }
  }
};

template <typename K, typename V>
class TypedMapValueKeyIterator final : public ValueIterator {
 public:
  explicit TypedMapValueKeyIterator(
      const ValueFlatHashMapFor<K, V>& entries ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : begin_(entries.begin()), end_(entries.end()) {}

  bool HasNext() override { return begin_ != end_; }

  absl::StatusOr<ValueView> Next(Value&) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when "
          "ValueIterator::HasNext() returns false");
    }
    auto key = ValueView(begin_->first);
    ++begin_;
    return key;
  }

 private:
  typename ValueFlatHashMapFor<K, V>::const_iterator begin_;
  const typename ValueFlatHashMapFor<K, V>::const_iterator end_;
};

template <typename T>
class TypedListValue final : public ListValueInterface {
 public:
  TypedListValue(ListType type, std::vector<T>&& elements)
      : type_(std::move(type)), elements_(std::move(elements)) {}

  std::string DebugString() const override {
    return absl::StrCat(
        "[", absl::StrJoin(elements_, ", ", absl::StreamFormatter()), "]");
  }

  bool IsEmpty() const override { return elements_.empty(); }

  size_t Size() const override { return elements_.size(); }

  absl::StatusOr<JsonArray> ConvertToJsonArray() const override {
    JsonArrayBuilder builder;
    builder.reserve(Size());
    for (const auto& element : elements_) {
      CEL_ASSIGN_OR_RETURN(auto json_element, element.ConvertToJson());
      builder.push_back(std::move(json_element));
    }
    return std::move(builder).Build();
  }

 protected:
  TypeView get_type() const override { return type_; }

 private:
  absl::StatusOr<ValueView> GetImpl(ValueFactory&, size_t index,
                                    Value&) const override {
    return elements_[index];
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<TypedListValue<T>>();
  }

  const ListType type_;
  const std::vector<T> elements_;
};

template <typename T>
class ListValueBuilderImpl final : public ListValueBuilder {
 public:
  using element_view_type = std::decay_t<decltype(std::declval<T>().type())>;
  using element_type = typename element_view_type::alternative_type;

  static_assert(common_internal::IsValueAlternativeV<T>,
                "T must be Value or one of the Value alternatives");

  ListValueBuilderImpl(MemoryManagerRef memory_manager, ListType type)
      : memory_manager_(memory_manager), type_(std::move(type)) {}

  ListValueBuilderImpl(const ListValueBuilderImpl&) = delete;
  ListValueBuilderImpl(ListValueBuilderImpl&&) = delete;
  ListValueBuilderImpl& operator=(const ListValueBuilderImpl&) = delete;
  ListValueBuilderImpl& operator=(ListValueBuilderImpl&&) = delete;

  absl::Status Add(Value value) override;

  absl::Status Add(T value);

  bool IsEmpty() const override { return elements_.empty(); }

  size_t Size() const override { return elements_.size(); }

  void Reserve(size_t capacity) override { elements_.reserve(capacity); }

  ListValue Build() && override;

 private:
  MemoryManagerRef memory_manager_;
  ListType type_;
  std::vector<T> elements_;
};

template <>
class ListValueBuilderImpl<Value> final : public ListValueBuilder {
 public:
  ListValueBuilderImpl(MemoryManagerRef memory_manager, ListType type)
      : memory_manager_(memory_manager), type_(std::move(type)) {}

  ListValueBuilderImpl(const ListValueBuilderImpl&) = delete;
  ListValueBuilderImpl(ListValueBuilderImpl&&) = delete;
  ListValueBuilderImpl& operator=(const ListValueBuilderImpl&) = delete;
  ListValueBuilderImpl& operator=(ListValueBuilderImpl&&) = delete;

  absl::Status Add(Value value) override {
    elements_.push_back(std::move(value));
    return absl::OkStatus();
  }

  bool IsEmpty() const override { return elements_.empty(); }

  size_t Size() const override { return elements_.size(); }

  void Reserve(size_t capacity) override { elements_.reserve(capacity); }

  ListValue Build() && override {
    return ListValue(memory_manager_.MakeShared<TypedListValue<Value>>(
        std::move(type_), std::move(elements_)));
  }

 private:
  MemoryManagerRef memory_manager_;
  ListType type_;
  std::vector<Value> elements_;
};

template <typename T>
absl::Status ListValueBuilderImpl<T>::Add(Value value) {
  return Add(Cast<T>(std::move(value)));
}

template <typename T>
absl::Status ListValueBuilderImpl<T>::Add(T value) {
  elements_.push_back(std::move(value));
  return absl::OkStatus();
}

template <typename T>
ListValue ListValueBuilderImpl<T>::Build() && {
  return ListValue(memory_manager_.template MakeShared<TypedListValue<T>>(
      std::move(type_), std::move(elements_)));
}

template <typename K, typename V>
class TypedMapValue final : public MapValueInterface {
 public:
  using key_view_type = std::decay_t<decltype(std::declval<K>().type())>;

  TypedMapValue(MapType type,
                absl::flat_hash_map<K, V, MapValueKeyHash<K>,
                                    MapValueKeyEqualTo<K>>&& entries)
      : type_(std::move(type)), entries_(std::move(entries)) {}

  std::string DebugString() const override {
    using KeyViewType = typename K::view_alternative_type;
    using ValueViewType = typename V::view_alternative_type;
    using KeyViewValueViewPair = std::pair<KeyViewType, ValueViewType>;
    std::vector<KeyViewValueViewPair> entries;
    entries.reserve(Size());
    for (const auto& entry : entries_) {
      entries.push_back(
          std::pair{KeyViewType{entry.first}, ValueViewType{entry.second}});
    }
    std::stable_sort(entries.begin(), entries.end(),
                     [](const KeyViewValueViewPair& lhs,
                        const KeyViewValueViewPair& rhs) -> bool {
                       return MapValueKeyLess<K>{}(lhs.first, rhs.first);
                     });
    return absl::StrCat(
        "{",
        absl::StrJoin(entries, ", ",
                      absl::PairFormatter(absl::StreamFormatter(), ": ",
                                          absl::StreamFormatter())),
        "}");
  }

  bool IsEmpty() const override { return entries_.empty(); }

  size_t Size() const override { return entries_.size(); }

  absl::StatusOr<JsonObject> ConvertToJsonObject() const override {
    JsonObjectBuilder builder;
    builder.reserve(Size());
    for (const auto& entry : entries_) {
      absl::Cord json_key = MapValueKeyJson<K>{}(entry.first);
      CEL_ASSIGN_OR_RETURN(auto json_value, entry.second.ConvertToJson());
      if (!builder.insert(std::pair{std::move(json_key), std::move(json_value)})
               .second) {
        return absl::FailedPreconditionError(
            "cannot convert map with duplicate keys to JSON");
      }
    }
    return std::move(builder).Build();
  }

  absl::StatusOr<ListValueView> ListKeys(TypeFactory& type_factory,
                                         ValueFactory&,
                                         ListValue& scratch) const override {
    // FIXME: Update ListKeys to just take ValueProvider, or move list/map
    // builders to ValueFactory.
    ListValueBuilderImpl<K> keys(
        type_factory.GetMemoryManager(),
        type_factory.CreateListType(Cast<key_view_type>(type_.key())));
    keys.Reserve(Size());
    for (const auto& entry : entries_) {
      CEL_RETURN_IF_ERROR(keys.Add(entry.first));
    }
    scratch = std::move(keys).Build();
    return scratch;
  }

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueFactory&) const override {
    return std::make_unique<TypedMapValueKeyIterator<K, V>>(entries_);
  }

 protected:
  TypeView get_type() const override { return type_; }

 private:
  absl::StatusOr<absl::optional<ValueView>> FindImpl(ValueFactory&,
                                                     ValueView key,
                                                     Value&) const override {
    if (auto entry =
            entries_.find(Cast<typename K::view_alternative_type>(key));
        entry != entries_.end()) {
      return ValueView{entry->second};
    }
    return absl::nullopt;
  }

  absl::StatusOr<bool> HasImpl(ValueView key) const override {
    if (auto entry =
            entries_.find(Cast<typename K::view_alternative_type>(key));
        entry != entries_.end()) {
      return true;
    }
    return false;
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<TypedMapValue<K, V>>();
  }

  const MapType type_;
  const absl::flat_hash_map<K, V, MapValueKeyHash<K>, MapValueKeyEqualTo<K>>
      entries_;
};

template <typename K, typename V>
class MapValueBuilderImpl final : public MapValueBuilder {
 public:
  using key_view_type = std::decay_t<decltype(std::declval<K>().type())>;
  using key_type = typename key_view_type::alternative_type;
  using value_view_type = std::decay_t<decltype(std::declval<V>().type())>;
  using value_type = typename value_view_type::alternative_type;

  static_assert(common_internal::IsValueAlternativeV<K>,
                "K must be Value or one of the Value alternatives");
  static_assert(common_internal::IsValueAlternativeV<V>,
                "V must be Value or one of the Value alternatives");

  MapValueBuilderImpl(MemoryManagerRef memory_manager, MapType type)
      : memory_manager_(memory_manager), type_(std::move(type)) {}

  MapValueBuilderImpl(const MapValueBuilderImpl&) = delete;
  MapValueBuilderImpl(MapValueBuilderImpl&&) = delete;
  MapValueBuilderImpl& operator=(const MapValueBuilderImpl&) = delete;
  MapValueBuilderImpl& operator=(MapValueBuilderImpl&&) = delete;

  absl::Status Put(Value key, Value value) override {
    auto inserted =
        entries_.insert({Cast<K>(std::move(key)), Cast<V>(std::move(value))})
            .second;
    ABSL_DCHECK(inserted) << "inserting duplicate keys is undefined behavior";
    return absl::OkStatus();
  }

  bool IsEmpty() const override { return entries_.empty(); }

  size_t Size() const override { return entries_.size(); }

  void Reserve(size_t capacity) override { entries_.reserve(capacity); }

  MapValue Build() && override {
    return MapValue(memory_manager_.MakeShared<TypedMapValue<K, V>>(
        std::move(type_), std::move(entries_)));
  }

 private:
  MemoryManagerRef memory_manager_;
  MapType type_;
  ValueFlatHashMapFor<K, V> entries_;
};

template <typename V>
class MapValueBuilderImpl<Value, V> final : public MapValueBuilder {
 public:
  using value_view_type = std::decay_t<decltype(std::declval<V>().type())>;
  using value_type = typename value_view_type::alternative_type;

  static_assert(common_internal::IsValueAlternativeV<V>,
                "V must be Value or one of the Value alternatives");

  MapValueBuilderImpl(MemoryManagerRef memory_manager, MapType type)
      : memory_manager_(memory_manager), type_(std::move(type)) {}

  absl::Status Put(Value key, Value value) override {
    auto inserted =
        entries_.insert({std::move(key), Cast<V>(std::move(value))}).second;
    ABSL_DCHECK(inserted) << "inserting duplicate keys is undefined behavior";
    return absl::OkStatus();
  }

  bool IsEmpty() const override { return entries_.empty(); }

  size_t Size() const override { return entries_.size(); }

  void Reserve(size_t capacity) override { entries_.reserve(capacity); }

  MapValue Build() && override {
    return MapValue(memory_manager_.MakeShared<TypedMapValue<Value, V>>(
        std::move(type_), std::move(entries_)));
  }

 private:
  MemoryManagerRef memory_manager_;
  MapType type_;
  absl::flat_hash_map<Value, V, MapValueKeyHash<Value>,
                      MapValueKeyEqualTo<Value>>
      entries_;
};

template <typename K>
class MapValueBuilderImpl<K, Value> final : public MapValueBuilder {
 public:
  using key_view_type = std::decay_t<decltype(std::declval<K>().type())>;
  using key_type = typename key_view_type::alternative_type;

  static_assert(common_internal::IsValueAlternativeV<K>,
                "K must be Value or one of the Value alternatives");

  MapValueBuilderImpl(MemoryManagerRef memory_manager, MapType type)
      : memory_manager_(memory_manager), type_(std::move(type)) {}

  absl::Status Put(Value key, Value value) override {
    auto inserted =
        entries_.insert({Cast<K>(std::move(key)), std::move(value)}).second;
    ABSL_DCHECK(inserted) << "inserting duplicate keys is undefined behavior";
    return absl::OkStatus();
  }

  bool IsEmpty() const override { return entries_.empty(); }

  size_t Size() const override { return entries_.size(); }

  void Reserve(size_t capacity) override { entries_.reserve(capacity); }

  MapValue Build() && override {
    return MapValue(memory_manager_.MakeShared<TypedMapValue<K, Value>>(
        std::move(type_), std::move(entries_)));
  }

 private:
  MemoryManagerRef memory_manager_;
  MapType type_;
  absl::flat_hash_map<K, Value, MapValueKeyHash<K>, MapValueKeyEqualTo<K>>
      entries_;
};

template <>
class MapValueBuilderImpl<Value, Value> final : public MapValueBuilder {
 public:
  MapValueBuilderImpl(MemoryManagerRef memory_manager, MapType type)
      : memory_manager_(memory_manager), type_(std::move(type)) {}

  absl::Status Put(Value key, Value value) override {
    auto inserted = entries_.insert({std::move(key), std::move(value)}).second;
    ABSL_DCHECK(inserted) << "inserting duplicate keys is undefined behavior";
    return absl::OkStatus();
  }

  bool IsEmpty() const override { return entries_.empty(); }

  size_t Size() const override { return entries_.size(); }

  void Reserve(size_t capacity) override { entries_.reserve(capacity); }

  MapValue Build() && override {
    return MapValue(memory_manager_.MakeShared<TypedMapValue<Value, Value>>(
        std::move(type_), std::move(entries_)));
  }

 private:
  MemoryManagerRef memory_manager_;
  MapType type_;
  absl::flat_hash_map<Value, Value, MapValueKeyHash<Value>,
                      MapValueKeyEqualTo<Value>>
      entries_;
};

}  // namespace

absl::StatusOr<Unique<ListValueBuilder>> ValueProvider::NewListValueBuilder(
    ValueFactory& value_factory, ListType type) {
  auto memory_manager = GetMemoryManager();
  switch (type.element().kind()) {
    case TypeKind::kBool:
      return memory_manager.MakeUnique<ListValueBuilderImpl<UintValue>>(
          memory_manager, std::move(type));
    case TypeKind::kBytes:
      return memory_manager.MakeUnique<ListValueBuilderImpl<BytesValue>>(
          memory_manager, std::move(type));
    case TypeKind::kDouble:
      return memory_manager.MakeUnique<ListValueBuilderImpl<DoubleValue>>(
          memory_manager, std::move(type));
    case TypeKind::kDuration:
      return memory_manager.MakeUnique<ListValueBuilderImpl<DurationValue>>(
          memory_manager, std::move(type));
    case TypeKind::kInt:
      return memory_manager.MakeUnique<ListValueBuilderImpl<IntValue>>(
          memory_manager, std::move(type));
    case TypeKind::kList:
      return memory_manager.MakeUnique<ListValueBuilderImpl<ListValue>>(
          memory_manager, std::move(type));
    case TypeKind::kMap:
      return memory_manager.MakeUnique<ListValueBuilderImpl<MapValue>>(
          memory_manager, std::move(type));
    case TypeKind::kNull:
      return memory_manager.MakeUnique<ListValueBuilderImpl<NullValue>>(
          memory_manager, std::move(type));
    case TypeKind::kOpaque:
      return memory_manager.MakeUnique<ListValueBuilderImpl<OpaqueValue>>(
          memory_manager, std::move(type));
    case TypeKind::kString:
      return memory_manager.MakeUnique<ListValueBuilderImpl<StringValue>>(
          memory_manager, std::move(type));
    case TypeKind::kTimestamp:
      return memory_manager.MakeUnique<ListValueBuilderImpl<TimestampValue>>(
          memory_manager, std::move(type));
    case TypeKind::kType:
      return memory_manager.MakeUnique<ListValueBuilderImpl<TypeValue>>(
          memory_manager, std::move(type));
    case TypeKind::kUint:
      return memory_manager.MakeUnique<ListValueBuilderImpl<UintValue>>(
          memory_manager, std::move(type));
    case TypeKind::kDyn:
      return memory_manager.MakeUnique<ListValueBuilderImpl<Value>>(
          memory_manager, std::move(type));
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "invalid list element type: ", type.element().DebugString()));
  }
}

absl::StatusOr<Unique<MapValueBuilder>> ValueProvider::NewMapValueBuilder(
    ValueFactory& value_factory, MapType type) {
  auto memory_manager = GetMemoryManager();
  switch (type.key().kind()) {
    case TypeKind::kBool:
      switch (type.value().kind()) {
        case TypeKind::kBool:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, BoolValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kBytes:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, BytesValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDouble:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, DoubleValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDuration:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, DurationValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kInt:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, IntValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kList:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, ListValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kMap:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, MapValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kNull:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, NullValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kOpaque:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, OpaqueValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kString:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, StringValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kTimestamp:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, TimestampValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kType:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, TypeValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kUint:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, UintValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDyn:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<BoolValue, Value>>(
                  memory_manager, std::move(type));
        default:
          return absl::InvalidArgumentError(absl::StrCat(
              "invalid map value type: ", type.value().DebugString()));
      }
    case TypeKind::kInt:
      switch (type.value().kind()) {
        case TypeKind::kBool:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, BoolValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kBytes:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, BytesValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDouble:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, DoubleValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDuration:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, DurationValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kInt:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, IntValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kList:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, ListValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kMap:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, MapValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kNull:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, NullValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kOpaque:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, OpaqueValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kString:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, StringValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kTimestamp:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, TimestampValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kType:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, TypeValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kUint:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, UintValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDyn:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<IntValue, Value>>(
                  memory_manager, std::move(type));
        default:
          return absl::InvalidArgumentError(absl::StrCat(
              "invalid map value type: ", type.value().DebugString()));
      }
    case TypeKind::kUint:
      switch (type.value().kind()) {
        case TypeKind::kBool:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, BoolValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kBytes:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, BytesValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDouble:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, DoubleValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDuration:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, DurationValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kInt:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, IntValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kList:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, ListValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kMap:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, MapValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kNull:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, NullValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kOpaque:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, OpaqueValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kString:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, StringValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kTimestamp:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, TimestampValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kType:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, TypeValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kUint:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, UintValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDyn:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<UintValue, Value>>(
                  memory_manager, std::move(type));
        default:
          return absl::InvalidArgumentError(absl::StrCat(
              "invalid map value type: ", type.value().DebugString()));
      }
    case TypeKind::kString:
      switch (type.value().kind()) {
        case TypeKind::kBool:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, BoolValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kBytes:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, BytesValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDouble:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, DoubleValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDuration:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, DurationValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kInt:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, IntValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kList:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, ListValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kMap:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, MapValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kNull:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, NullValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kOpaque:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, OpaqueValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kString:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, StringValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kTimestamp:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, TimestampValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kType:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, TypeValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kUint:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, UintValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDyn:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<StringValue, Value>>(
                  memory_manager, std::move(type));
        default:
          return absl::InvalidArgumentError(absl::StrCat(
              "invalid map value type: ", type.value().DebugString()));
      }
    case TypeKind::kDyn:
      switch (type.value().kind()) {
        case TypeKind::kBool:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, BoolValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kBytes:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, BytesValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDouble:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, DoubleValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDuration:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, DurationValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kInt:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, IntValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kList:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, ListValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kMap:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, MapValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kNull:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, NullValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kOpaque:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, OpaqueValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kString:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, StringValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kTimestamp:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, TimestampValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kType:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, TypeValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kUint:
          return memory_manager
              .MakeUnique<MapValueBuilderImpl<Value, UintValue>>(
                  memory_manager, std::move(type));
        case TypeKind::kDyn:
          return memory_manager.MakeUnique<MapValueBuilderImpl<Value, Value>>(
              memory_manager, std::move(type));
        default:
          return absl::InvalidArgumentError(absl::StrCat(
              "invalid map value type: ", type.value().DebugString()));
      }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("invalid map key type: ", type.key().DebugString()));
  }
}

Shared<ValueProvider> NewThreadCompatibleValueProvider(
    MemoryManagerRef memory_manager) {
  return memory_manager
      .MakeShared<common_internal::ThreadCompatibleValueProvider>(
          memory_manager);
}

Shared<ValueProvider> NewThreadSafeValueProvider(
    MemoryManagerRef memory_manager) {
  return memory_manager.MakeShared<common_internal::ThreadSafeValueProvider>(
      memory_manager);
}

}  // namespace cel

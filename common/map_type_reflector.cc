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

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "internal/dynamic_loader.h"  // IWYU pragma: keep
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

  size_t operator()(const T& value) const { return absl::HashOf(value); }
};

template <typename T>
struct MapValueKeyEqualTo {
  // Used to enable heterogeneous operations in supporting containers.
  using is_transparent = void;

  bool operator()(const T& lhs, const T& rhs) const { return lhs == rhs; }
};

template <typename T>
struct MapValueKeyLess {
  bool operator()(const T& lhs, const T& rhs) const { return lhs < rhs; }
};

template <>
struct MapValueKeyHash<Value> {
  // Used to enable heterogeneous operations in supporting containers.
  using is_transparent = void;

  size_t operator()(const Value& value) const {
    switch (value.kind()) {
      case ValueKind::kBool:
        return absl::HashOf(ValueKind::kBool, Cast<BoolValue>(value));
      case ValueKind::kInt:
        return absl::HashOf(ValueKind::kInt, Cast<IntValue>(value));
      case ValueKind::kUint:
        return absl::HashOf(ValueKind::kUint, Cast<UintValue>(value));
      case ValueKind::kString:
        return absl::HashOf(ValueKind::kString, Cast<StringValue>(value));
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

  bool operator()(const Value& lhs, const Value& rhs) const {
    switch (lhs.kind()) {
      case ValueKind::kBool:
        switch (rhs.kind()) {
          case ValueKind::kBool:
            return Cast<BoolValue>(lhs) == Cast<BoolValue>(rhs);
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
            return Cast<IntValue>(lhs) == Cast<IntValue>(rhs);
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
            return Cast<UintValue>(lhs) == Cast<UintValue>(rhs);
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
            return Cast<StringValue>(lhs) == Cast<StringValue>(rhs);
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
  bool operator()(const Value& lhs, const Value& rhs) const {
    switch (lhs.kind()) {
      case ValueKind::kBool:
        switch (rhs.kind()) {
          case ValueKind::kBool:
            return Cast<BoolValue>(lhs) < Cast<BoolValue>(rhs);
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
            return Cast<IntValue>(lhs) < Cast<IntValue>(rhs);
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
            return Cast<UintValue>(lhs) < Cast<UintValue>(rhs);
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
            return Cast<StringValue>(lhs) < Cast<StringValue>(rhs);
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
  absl::StatusOr<absl::Cord> operator()(BoolValue value) const {
    return TypeConversionError("map<bool, ?>", "google.protobuf.Struct")
        .NativeValue();
  }
};

template <>
struct MapValueKeyJson<IntValue> {
  absl::StatusOr<absl::Cord> operator()(IntValue value) const {
    return TypeConversionError("map<int, ?>", "google.protobuf.Struct")
        .NativeValue();
  }
};

template <>
struct MapValueKeyJson<UintValue> {
  absl::StatusOr<absl::Cord> operator()(UintValue value) const {
    return TypeConversionError("map<uint, ?>", "google.protobuf.Struct")
        .NativeValue();
  }
};

template <>
struct MapValueKeyJson<StringValue> {
  absl::StatusOr<absl::Cord> operator()(const StringValue& value) const {
    return value.NativeCord();
  }
};

template <>
struct MapValueKeyJson<Value> {
  absl::StatusOr<absl::Cord> operator()(const Value& value) const {
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
        return absl::InternalError(
            absl::StrCat("unexpected map key type: ", value.GetTypeName()));
    }
  }
};

class MapValueKeyIteratorImpl final : public ValueIterator {
 public:
  explicit MapValueKeyIteratorImpl(const ValueFlatHashMapFor<Value, Value>&
                                       entries ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : begin_(entries.begin()), end_(entries.end()) {}

  bool HasNext() override { return begin_ != end_; }

  absl::Status Next(ValueManager&, Value& result) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when "
          "ValueIterator::HasNext() returns false");
    }
    auto key = Value(begin_->first);
    ++begin_;
    result = std::move(key);
    return absl::OkStatus();
  }

 private:
  typename ValueFlatHashMapFor<Value, Value>::const_iterator begin_;
  const typename ValueFlatHashMapFor<Value, Value>::const_iterator end_;
};

class MapValueImpl final : public ParsedMapValueInterface {
 public:
  explicit MapValueImpl(
      absl::flat_hash_map<Value, Value, MapValueKeyHash<Value>,
                          MapValueKeyEqualTo<Value>>&& entries)
      : entries_(std::move(entries)) {}

  std::string DebugString() const override {
    std::vector<std::pair<Value, Value>> entries;
    entries.reserve(Size());
    for (const auto& entry : entries_) {
      entries.push_back(std::pair{entry.first, entry.second});
    }
    std::stable_sort(entries.begin(), entries.end(),
                     [](const std::pair<Value, Value>& lhs,
                        const std::pair<Value, Value>& rhs) -> bool {
                       return MapValueKeyLess<Value>{}(lhs.first, rhs.first);
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

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter& value_manager) const override {
    JsonObjectBuilder builder;
    builder.reserve(Size());
    for (const auto& entry : entries_) {
      CEL_ASSIGN_OR_RETURN(auto json_key,
                           MapValueKeyJson<Value>{}(entry.first));
      CEL_ASSIGN_OR_RETURN(auto json_value,
                           entry.second.ConvertToJson(value_manager));
      if (!builder.insert(std::pair{std::move(json_key), std::move(json_value)})
               .second) {
        return absl::FailedPreconditionError(
            "cannot convert map with duplicate keys to JSON");
      }
    }
    return std::move(builder).Build();
  }

  absl::Status ListKeys(ValueManager& value_manager,
                        ListValue& result) const override {
    CEL_ASSIGN_OR_RETURN(auto keys,
                         value_manager.NewListValueBuilder(ListType()));
    keys->Reserve(Size());
    for (const auto& entry : entries_) {
      CEL_RETURN_IF_ERROR(keys->Add(entry.first));
    }
    result = std::move(*keys).Build();
    return absl::OkStatus();
  }

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const override {
    for (const auto& entry : entries_) {
      CEL_ASSIGN_OR_RETURN(auto ok, callback(entry.first, entry.second));
      if (!ok) {
        break;
      }
    }
    return absl::OkStatus();
  }

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager&) const override {
    return std::make_unique<MapValueKeyIteratorImpl>(entries_);
  }

 private:
  absl::StatusOr<bool> FindImpl(ValueManager&, const Value& key,
                                Value& result) const override {
    if (auto entry = entries_.find(key); entry != entries_.end()) {
      result = entry->second;
      return true;
    }
    return false;
  }

  absl::StatusOr<bool> HasImpl(ValueManager&, const Value& key) const override {
    if (auto entry = entries_.find(key); entry != entries_.end()) {
      return true;
    }
    return false;
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<MapValueImpl>();
  }

  const absl::flat_hash_map<Value, Value, MapValueKeyHash<Value>,
                            MapValueKeyEqualTo<Value>>
      entries_;
};

class MapValueBuilderImpl final : public MapValueBuilder {
 public:
  explicit MapValueBuilderImpl(MemoryManagerRef memory_manager)
      : memory_manager_(memory_manager) {}

  absl::Status Put(Value key, Value value) override {
    if (key.Is<ErrorValue>()) {
      return std::move(key).Get<ErrorValue>().NativeValue();
    }
    if (value.Is<ErrorValue>()) {
      return std::move(value).Get<ErrorValue>().NativeValue();
    }
    auto inserted = entries_.insert({std::move(key), std::move(value)}).second;
    if (!inserted) {
      return DuplicateKeyError().NativeValue();
    }
    return absl::OkStatus();
  }

  bool IsEmpty() const override { return entries_.empty(); }

  size_t Size() const override { return entries_.size(); }

  void Reserve(size_t capacity) override { entries_.reserve(capacity); }

  MapValue Build() && override {
    return ParsedMapValue(
        memory_manager_.MakeShared<MapValueImpl>(std::move(entries_)));
  }

 private:
  MemoryManagerRef memory_manager_;
  absl::flat_hash_map<Value, Value, MapValueKeyHash<Value>,
                      MapValueKeyEqualTo<Value>>
      entries_;
};

using LegacyTypeReflector_NewMapValueBuilder =
    absl::StatusOr<Unique<MapValueBuilder>> (*)(ValueFactory&, const MapType&);

ABSL_CONST_INIT struct {
  absl::once_flag init_once;
  LegacyTypeReflector_NewMapValueBuilder new_map_value_builder = nullptr;
} legacy_type_reflector_vtable;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"
#endif

#if ABSL_HAVE_ATTRIBUTE_WEAK
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<Unique<MapValueBuilder>>
cel_common_internal_LegacyTypeReflector_NewMapValueBuilder(
    ValueFactory& value_factory, const MapType& type);
#endif

void InitializeLegacyTypeReflector() {
  absl::call_once(legacy_type_reflector_vtable.init_once, []() -> void {
#if ABSL_HAVE_ATTRIBUTE_WEAK
    legacy_type_reflector_vtable.new_map_value_builder =
        cel_common_internal_LegacyTypeReflector_NewMapValueBuilder;
#else
    internal::DynamicLoader dynamic_loader;
    if (auto new_map_value_builder = dynamic_loader.FindSymbol(
            "cel_common_internal_LegacyTypeReflector_NewMapValueBuilder");
        new_map_value_builder) {
      legacy_type_reflector_vtable.new_map_value_builder =
          *new_map_value_builder;
    }
#endif
  });
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

}  // namespace

absl::StatusOr<Unique<MapValueBuilder>> TypeReflector::NewMapValueBuilder(
    ValueFactory& value_factory, const MapType& type) const {
  InitializeLegacyTypeReflector();
  auto memory_manager = value_factory.GetMemoryManager();
  if (memory_manager.memory_management() == MemoryManagement::kPooling &&
      legacy_type_reflector_vtable.new_map_value_builder != nullptr) {
    auto status_or_builder =
        (*legacy_type_reflector_vtable.new_map_value_builder)(value_factory,
                                                              type);
    if (status_or_builder.ok()) {
      return std::move(status_or_builder).value();
    }
    if (!absl::IsUnimplemented(status_or_builder.status())) {
      return status_or_builder;
    }
  }
  return memory_manager.MakeUnique<MapValueBuilderImpl>(memory_manager);
}

namespace common_internal {

absl::StatusOr<Unique<MapValueBuilder>> LegacyTypeReflector::NewMapValueBuilder(
    ValueFactory& value_factory, const MapType& type) const {
  InitializeLegacyTypeReflector();
  auto memory_manager = value_factory.GetMemoryManager();
  if (memory_manager.memory_management() == MemoryManagement::kPooling &&
      legacy_type_reflector_vtable.new_map_value_builder != nullptr) {
    auto status_or_builder =
        (*legacy_type_reflector_vtable.new_map_value_builder)(value_factory,
                                                              type);
    if (status_or_builder.ok()) {
      return std::move(status_or_builder).value();
    }
    if (!absl::IsUnimplemented(status_or_builder.status())) {
      return status_or_builder;
    }
  }
  return TypeReflector::NewMapValueBuilder(value_factory, type);
}

}  // namespace common_internal

}  // namespace cel

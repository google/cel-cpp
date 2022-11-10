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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_MAP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_MAP_VALUE_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/map_type.h"
#include "base/value.h"
#include "internal/rtti.h"

namespace cel {

class ListValue;
class ValueFactory;
class MemoryManager;

// MapValue represents an instance of cel::MapType.
class MapValue : public Value {
 public:
  static constexpr Kind kKind = MapType::kKind;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  constexpr Kind kind() const { return kKind; }

  Handle<MapType> type() const;

  std::string DebugString() const;

  size_t size() const;

  bool empty() const;

  bool Equals(const Value& other) const;

  void HashValue(absl::HashState state) const;

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    const Handle<Value>& key) const;

  absl::StatusOr<bool> Has(const Handle<Value>& key) const;

  absl::StatusOr<Handle<ListValue>> ListKeys(ValueFactory& value_factory) const;

  ABSL_DEPRECATED("Use ListKeys(ValueFactory&) instead")
  absl::StatusOr<Handle<ListValue>> ListKeys(
      MemoryManager& memory_manager) const;

 private:
  friend internal::TypeInfo base_internal::GetMapValueTypeId(
      const MapValue& map_value);
  friend class base_internal::ValueHandle;
  friend class base_internal::LegacyMapValue;
  friend class base_internal::AbstractMapValue;

  MapValue() = default;

  // Called by CEL_IMPLEMENT_MAP_VALUE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const;
};

CEL_INTERNAL_VALUE_DECL(MapValue);

namespace base_internal {

ABSL_ATTRIBUTE_WEAK size_t LegacyMapValueSize(uintptr_t impl);
ABSL_ATTRIBUTE_WEAK bool LegacyMapValueEmpty(uintptr_t impl);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<Handle<Value>> LegacyMapValueGet(
    uintptr_t impl, ValueFactory& value_factory, const Handle<Value>& key);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<bool> LegacyMapValueHas(
    uintptr_t impl, const Handle<Value>& key);
ABSL_ATTRIBUTE_WEAK absl::StatusOr<Handle<ListValue>> LegacyMapValueListKeys(
    uintptr_t impl, ValueFactory& value_factory);

class LegacyMapValue final : public MapValue, public InlineData {
 public:
  static bool Is(const Value& value) {
    return value.kind() == kKind &&
           static_cast<const MapValue&>(value).TypeId() ==
               internal::TypeId<LegacyMapValue>();
  }

  Handle<MapType> type() const;

  std::string DebugString() const;

  size_t size() const;

  bool empty() const;

  bool Equals(const Value& other) const;

  void HashValue(absl::HashState state) const;

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    const Handle<Value>& key) const;

  absl::StatusOr<bool> Has(const Handle<Value>& key) const;

  absl::StatusOr<Handle<ListValue>> ListKeys(ValueFactory& value_factory) const;

  constexpr uintptr_t value() const { return impl_; }

 private:
  friend class base_internal::ValueHandle;
  friend class cel::MapValue;
  template <size_t Size, size_t Align>
  friend class AnyData;

  static constexpr uintptr_t kMetadata =
      kStoredInline | kTrivial | (static_cast<uintptr_t>(kKind) << kKindShift);

  explicit LegacyMapValue(uintptr_t impl)
      : MapValue(), InlineData(kMetadata), impl_(impl) {}

  internal::TypeInfo TypeId() const {
    return internal::TypeId<LegacyMapValue>();
  }
  uintptr_t impl_;
};

class AbstractMapValue : public MapValue, public HeapData {
 public:
  static bool Is(const Value& value) {
    return value.kind() == kKind &&
           static_cast<const MapValue&>(value).TypeId() !=
               internal::TypeId<LegacyMapValue>();
  }

  Handle<MapType> type() const { return type_; }

  virtual std::string DebugString() const = 0;

  virtual size_t size() const = 0;

  virtual bool empty() const { return size() == 0; }

  virtual bool Equals(const Value& other) const = 0;

  virtual void HashValue(absl::HashState state) const = 0;

  virtual absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                            const Handle<Value>& key) const = 0;

  virtual absl::StatusOr<bool> Has(const Handle<Value>& key) const = 0;

  virtual absl::StatusOr<Handle<ListValue>> ListKeys(
      ValueFactory& value_factory) const = 0;

 protected:
  explicit AbstractMapValue(Handle<MapType> type);

 private:
  friend class cel::MapValue;
  friend class base_internal::ValueHandle;

  // Called by CEL_IMPLEMENT_MAP_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  const Handle<MapType> type_;
};

inline internal::TypeInfo GetMapValueTypeId(const MapValue& map_value) {
  return map_value.TypeId();
}

}  // namespace base_internal

#define CEL_MAP_VALUE_CLASS ::cel::base_internal::AbstractMapValue

// CEL_DECLARE_MAP_VALUE declares `map_value` as an map value. It must
// be part of the class definition of `map_value`.
//
// class MyMapValue : public CEL_MAP_VALUE_CLASS {
//  ...
// private:
//   CEL_DECLARE_MAP_VALUE(MyMapValue);
// };
#define CEL_DECLARE_MAP_VALUE(map_value) \
  CEL_INTERNAL_DECLARE_VALUE(Map, map_value)

// CEL_IMPLEMENT_MAP_VALUE implements `map_value` as an map
// value. It must be called after the class definition of `map_value`.
//
// class MyMapValue : public CEL_MAP_VALUE_CLASS {
//  ...
// private:
//   CEL_DECLARE_MAP_VALUE(MyMapValue);
// };
//
// CEL_IMPLEMENT_MAP_VALUE(MyMapValue);
#define CEL_IMPLEMENT_MAP_VALUE(map_value) \
  CEL_INTERNAL_IMPLEMENT_VALUE(Map, map_value)

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_MAP_VALUE_H_

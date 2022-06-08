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

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/map_type.h"
#include "base/value.h"
#include "internal/rtti.h"

namespace cel {

class ValueFactory;

// MapValue represents an instance of cel::MapType.
class MapValue : public Value {
 public:
  Persistent<const Type> type() const final { return type_; }

  Kind kind() const final { return Kind::kMap; }

  virtual size_t size() const = 0;

  virtual bool empty() const { return size() == 0; }

  virtual absl::StatusOr<Persistent<const Value>> Get(
      ValueFactory& value_factory,
      const Persistent<const Value>& key) const = 0;

  virtual absl::StatusOr<bool> Has(
      const Persistent<const Value>& key) const = 0;

 protected:
  explicit MapValue(const Persistent<const MapType>& type) : type_(type) {}

 private:
  friend internal::TypeInfo base_internal::GetMapValueTypeId(
      const MapValue& map_value);
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kMap; }

  MapValue(const MapValue&) = delete;
  MapValue(MapValue&&) = delete;

  bool Equals(const Value& other) const override = 0;
  void HashValue(absl::HashState state) const override = 0;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by CEL_IMPLEMENT_MAP_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  // Set lazily, by EnumValue::New.
  Persistent<const MapType> type_;
};

// CEL_DECLARE_MAP_VALUE declares `map_value` as an map value. It must
// be part of the class definition of `map_value`.
//
// class MyMapValue : public cel::MapValue {
//  ...
// private:
//   CEL_DECLARE_MAP_VALUE(MyMapValue);
// };
#define CEL_DECLARE_MAP_VALUE(map_value) \
  CEL_INTERNAL_DECLARE_VALUE(Map, map_value)

// CEL_IMPLEMENT_MAP_VALUE implements `map_value` as an map
// value. It must be called after the class definition of `map_value`.
//
// class MyMapValue : public cel::MapValue {
//  ...
// private:
//   CEL_DECLARE_MAP_VALUE(MyMapValue);
// };
//
// CEL_IMPLEMENT_MAP_VALUE(MyMapValue);
#define CEL_IMPLEMENT_MAP_VALUE(map_value) \
  CEL_INTERNAL_IMPLEMENT_VALUE(Map, map_value)

namespace base_internal {

inline internal::TypeInfo GetMapValueTypeId(const MapValue& map_value) {
  return map_value.TypeId();
}

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_MAP_VALUE_H_

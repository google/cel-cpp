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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPES_MAP_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPES_MAP_TYPE_H_

#include <cstddef>
#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "base/kind.h"
#include "base/type.h"

namespace cel {

class TypeFactory;

// MapType represents a map type. A map is container of key and value pairs
// where each key appears at most once.
class MapType : public Type {
  // I would have liked to make this class final, but we cannot instantiate
  // Persistent<const Type> or Transient<const Type> at this point. It must be
  // done after the post include below. Maybe we should separate out the post
  // includes on a per type basis so we can do that?
 public:
  Kind kind() const final { return Kind::kMap; }

  absl::string_view name() const final { return "map"; }

  std::string DebugString() const final;

  // Returns the type of the keys in the map.
  virtual Persistent<const Type> key() const = 0;

  // Returns the type of the values in the map.
  virtual Persistent<const Type> value() const = 0;

 private:
  friend class TypeFactory;
  friend class base_internal::TypeHandleBase;
  friend class base_internal::MapTypeImpl;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kMap; }

  MapType() = default;

  MapType(const MapType&) = delete;
  MapType(MapType&&) = delete;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by base_internal::TypeHandleBase.
  bool Equals(const Type& other) const final;

  // Called by base_internal::TypeHandleBase.
  void HashValue(absl::HashState state) const final;
};

CEL_INTERNAL_TYPE_DECL(MapType);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_MAP_TYPE_H_

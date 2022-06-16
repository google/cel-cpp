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

#include <atomic>
#include <cstddef>
#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"

namespace cel {

class MemoryManager;
class TypeFactory;

// MapType represents a map type. A map is container of key and value pairs
// where each key appears at most once.
class MapType final : public Type, public base_internal::HeapData {
  // I would have liked to make this class final, but we cannot instantiate
  // Persistent<const Type> or Transient<const Type> at this point. It must be
  // done after the post include below. Maybe we should separate out the post
  // includes on a per type basis so we can do that?
 public:
  static constexpr Kind kKind = Kind::kMap;

  static bool Is(const Type& type) { return type.kind() == kKind; }

  Kind kind() const { return kKind; }

  absl::string_view name() const { return KindToString(kind()); }

  std::string DebugString() const;

  void HashValue(absl::HashState state) const;

  bool Equals(const Type& other) const;

  // Returns the type of the keys in the map.
  const Persistent<const Type>& key() const { return key_; }

  // Returns the type of the values in the map.
  const Persistent<const Type>& value() const { return value_; }

 private:
  friend class MemoryManager;
  friend class TypeFactory;
  friend class base_internal::PersistentTypeHandle;

  explicit MapType(Persistent<const Type> key, Persistent<const Type> value);

  const Persistent<const Type> key_;
  const Persistent<const Type> value_;
};

CEL_INTERNAL_TYPE_DECL(MapType);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_MAP_TYPE_H_

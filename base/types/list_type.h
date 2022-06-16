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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPES_LIST_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPES_LIST_TYPE_H_

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

// ListType represents a list type. A list is a sequential container where each
// element is the same type.
class ListType final : public Type, public base_internal::HeapData {
  // I would have liked to make this class final, but we cannot instantiate
  // Persistent<const Type> or Transient<const Type> at this point. It must be
  // done after the post include below. Maybe we should separate out the post
  // includes on a per type basis so we can do that?
 public:
  static constexpr Kind kKind = Kind::kList;

  static bool Is(const Type& type) { return type.kind() == kKind; }

  Kind kind() const { return kKind; }

  absl::string_view name() const { return KindToString(kind()); }

  std::string DebugString() const;

  void HashValue(absl::HashState state) const;

  bool Equals(const Type& other) const;

  // Returns the type of the elements in the list.
  const Persistent<const Type>& element() const { return element_; }

 private:
  friend class MemoryManager;
  friend class TypeFactory;
  friend class base_internal::PersistentTypeHandle;

  explicit ListType(Persistent<const Type> element);

  const Persistent<const Type> element_;
};

CEL_INTERNAL_TYPE_DECL(ListType);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_LIST_TYPE_H_

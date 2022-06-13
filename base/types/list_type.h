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

#include <cstddef>
#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "base/kind.h"
#include "base/type.h"

namespace cel {

// ListType represents a list type. A list is a sequential container where each
// element is the same type.
class ListType : public Type {
  // I would have liked to make this class final, but we cannot instantiate
  // Persistent<const Type> or Transient<const Type> at this point. It must be
  // done after the post include below. Maybe we should separate out the post
  // includes on a per type basis so we can do that?
 public:
  Kind kind() const final { return Kind::kList; }

  absl::string_view name() const final { return "list"; }

  std::string DebugString() const final;

  // Returns the type of the elements in the list.
  virtual Persistent<const Type> element() const = 0;

 private:
  friend class TypeFactory;
  friend class base_internal::TypeHandleBase;
  friend class base_internal::ListTypeImpl;

  // Called by base_internal::TypeHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Type& type) { return type.kind() == Kind::kList; }

  ListType() = default;

  ListType(const ListType&) = delete;
  ListType(ListType&&) = delete;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by base_internal::TypeHandleBase.
  bool Equals(const Type& other) const final;

  // Called by base_internal::TypeHandleBase.
  void HashValue(absl::HashState state) const final;
};

CEL_INTERNAL_TYPE_DECL(ListType);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_LIST_TYPE_H_

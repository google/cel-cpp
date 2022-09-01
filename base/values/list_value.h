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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_LIST_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/list_type.h"
#include "base/value.h"
#include "internal/rtti.h"

namespace cel {

class ValueFactory;

// ListValue represents an instance of cel::ListType.
class ListValue : public Value, public base_internal::HeapData {
 public:
  static constexpr Kind kKind = ListType::kKind;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  // TODO(issues/5): implement iterators so we can have cheap concated lists

  const Persistent<const ListType> type() const { return type_; }

  constexpr Kind kind() const { return kKind; }

  virtual std::string DebugString() const = 0;

  virtual size_t size() const = 0;

  virtual bool empty() const { return size() == 0; }

  virtual absl::StatusOr<Persistent<const Value>> Get(
      ValueFactory& value_factory, size_t index) const = 0;

  virtual bool Equals(const Value& other) const = 0;

  virtual void HashValue(absl::HashState state) const = 0;

 protected:
  explicit ListValue(Persistent<const ListType> type);

 private:
  friend internal::TypeInfo base_internal::GetListValueTypeId(
      const ListValue& list_value);
  friend class base_internal::PersistentValueHandle;

  // Called by CEL_IMPLEMENT_LIST_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  const Persistent<const ListType> type_;
};

CEL_INTERNAL_VALUE_DECL(ListValue);

// CEL_DECLARE_LIST_VALUE declares `list_value` as an list value. It must
// be part of the class definition of `list_value`.
//
// class MyListValue : public cel::ListValue {
//  ...
// private:
//   CEL_DECLARE_LIST_VALUE(MyListValue);
// };
#define CEL_DECLARE_LIST_VALUE(list_value) \
  CEL_INTERNAL_DECLARE_VALUE(List, list_value)

// CEL_IMPLEMENT_LIST_VALUE implements `list_value` as an list
// value. It must be called after the class definition of `list_value`.
//
// class MyListValue : public cel::ListValue {
//  ...
// private:
//   CEL_DECLARE_LIST_VALUE(MyListValue);
// };
//
// CEL_IMPLEMENT_LIST_VALUE(MyListValue);
#define CEL_IMPLEMENT_LIST_VALUE(list_value) \
  CEL_INTERNAL_IMPLEMENT_VALUE(List, list_value)

namespace base_internal {

inline internal::TypeInfo GetListValueTypeId(const ListValue& list_value) {
  return list_value.TypeId();
}

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_LIST_VALUE_H_

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

#include "absl/base/attributes.h"
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
class ListValue : public Value {
 public:
  static constexpr Kind kKind = ListType::kKind;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  // TODO(issues/5): implement iterators so we can have cheap concated lists

  Handle<ListType> type() const;

  constexpr Kind kind() const { return kKind; }

  std::string DebugString() const;

  size_t size() const;

  bool empty() const;

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const;

  ABSL_DEPRECATED("Use Get(ValueFactory&, size_t) instead")
  absl::StatusOr<Handle<Value>> Get(MemoryManager& memory_manager,
                                    size_t index) const;

  bool Equals(const Value& other) const;

  void HashValue(absl::HashState state) const;

 private:
  friend class base_internal::LegacyListValue;
  friend class base_internal::AbstractListValue;
  friend internal::TypeInfo base_internal::GetListValueTypeId(
      const ListValue& list_value);
  friend class base_internal::ValueHandle;

  ListValue() = default;

  // Called by CEL_IMPLEMENT_LIST_VALUE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const;
};

namespace base_internal {

ABSL_ATTRIBUTE_WEAK absl::StatusOr<Handle<Value>> LegacyListValueGet(
    uintptr_t impl, ValueFactory& value_factory, size_t index);
ABSL_ATTRIBUTE_WEAK size_t LegacyListValueSize(uintptr_t impl);
ABSL_ATTRIBUTE_WEAK bool LegacyListValueEmpty(uintptr_t impl);

class LegacyListValue final : public ListValue, public InlineData {
 public:
  static bool Is(const Value& value) {
    return value.kind() == kKind &&
           static_cast<const ListValue&>(value).TypeId() ==
               internal::TypeId<LegacyListValue>();
  }

  Handle<ListType> type() const;

  std::string DebugString() const;

  size_t size() const;

  bool empty() const;

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const;

  bool Equals(const Value& other) const;

  void HashValue(absl::HashState state) const;

  constexpr uintptr_t value() const { return impl_; }

 private:
  friend class base_internal::ValueHandle;
  friend class cel::ListValue;
  template <size_t Size, size_t Align>
  friend class AnyData;

  static constexpr uintptr_t kMetadata =
      kStoredInline | kTrivial | (static_cast<uintptr_t>(kKind) << kKindShift);

  explicit LegacyListValue(uintptr_t impl)
      : ListValue(), InlineData(kMetadata), impl_(impl) {}

  // Called by CEL_IMPLEMENT_STRUCT_VALUE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const {
    return internal::TypeId<LegacyListValue>();
  }

  uintptr_t impl_;
};

class AbstractListValue : public ListValue, public HeapData {
 public:
  static bool Is(const Value& value) {
    return value.kind() == kKind &&
           static_cast<const ListValue&>(value).TypeId() !=
               internal::TypeId<LegacyListValue>();
  }

  Handle<ListType> type() const { return type_; }

  virtual std::string DebugString() const = 0;

  virtual size_t size() const = 0;

  virtual bool empty() const { return size() == 0; }

  virtual absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                            size_t index) const = 0;

  virtual bool Equals(const Value& other) const = 0;

  virtual void HashValue(absl::HashState state) const = 0;

 protected:
  explicit AbstractListValue(Handle<ListType> type);

 private:
  friend class cel::ListValue;
  friend class base_internal::LegacyListValue;
  friend class base_internal::AbstractListValue;
  friend internal::TypeInfo base_internal::GetListValueTypeId(
      const ListValue& list_value);
  friend class base_internal::ValueHandle;

  // Called by CEL_IMPLEMENT_LIST_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  const Handle<ListType> type_;
};

inline internal::TypeInfo GetListValueTypeId(const ListValue& list_value) {
  return list_value.TypeId();
}

}  // namespace base_internal

#define CEL_LIST_VALUE_CLASS ::cel::base_internal::AbstractListValue

CEL_INTERNAL_VALUE_DECL(ListValue);

// CEL_DECLARE_LIST_VALUE declares `list_value` as an list value. It must
// be part of the class definition of `list_value`.
//
// class MyListValue : public CEL_LIST_VALUE_CLASS {
//  ...
// private:
//   CEL_DECLARE_LIST_VALUE(MyListValue);
// };
#define CEL_DECLARE_LIST_VALUE(list_value) \
  CEL_INTERNAL_DECLARE_VALUE(List, list_value)

// CEL_IMPLEMENT_LIST_VALUE implements `list_value` as an list
// value. It must be called after the class definition of `list_value`.
//
// class MyListValue : public CEL_LIST_VALUE_CLASS {
//  ...
// private:
//   CEL_DECLARE_LIST_VALUE(MyListValue);
// };
//
// CEL_IMPLEMENT_LIST_VALUE(MyListValue);
#define CEL_IMPLEMENT_LIST_VALUE(list_value) \
  CEL_INTERNAL_IMPLEMENT_VALUE(List, list_value)

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_LIST_VALUE_H_

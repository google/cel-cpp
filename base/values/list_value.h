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
#include <vector>

#include "absl/base/attributes.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/owner.h"
#include "base/type.h"
#include "base/types/list_type.h"
#include "base/value.h"
#include "internal/rtti.h"

namespace cel {

class ValueFactory;
class ListValueBuilderInterface;
template <typename T>
class ListValueBuilder;

// ListValue represents an instance of cel::ListType.
class ListValue : public Value {
 public:
  using BuilderInterface = ListValueBuilderInterface;
  template <typename T>
  using Builder = ListValueBuilder<T>;

  static constexpr ValueKind kKind = ValueKind::kList;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  using Value::Is;

  static const ListValue& Cast(const Value& value) {
    ABSL_DCHECK(Is(value)) << "cannot cast " << value.type()->name()
                           << " to list";
    return static_cast<const ListValue&>(value);
  }

  // TODO(uncreated-issue/10): implement iterators so we can have cheap concat lists

  Handle<ListType> type() const;

  constexpr ValueKind kind() const { return kKind; }

  std::string DebugString() const;

  absl::StatusOr<Any> ConvertToAny(ValueFactory& value_factory) const;

  absl::StatusOr<Json> ConvertToJson(ValueFactory& value_factory) const;

  size_t size() const;

  bool empty() const;

  class GetContext final {
   public:
    explicit GetContext(ValueFactory& value_factory)
        : value_factory_(value_factory) {}

    ValueFactory& value_factory() const { return value_factory_; }

   private:
    ValueFactory& value_factory_;
  };

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const;

  struct Element final {
    Element(size_t index, Handle<Value> value)
        : index(index), value(std::move(value)) {}

    size_t index;
    Handle<Value> value;
  };

  class Iterator;

  absl::StatusOr<UniqueRef<Iterator>> NewIterator(
      MemoryManager& memory_manager) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

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

// Abstract class describes an iterator which can iterate over the elements in a
// list. A default implementation is provided by `ListValue::NewIterator`,
// however it is likely not as efficient as providing your own implementation.
class ListValue::Iterator {
 public:
  using Element = ListValue::Element;

  virtual ~Iterator() = default;

  ABSL_MUST_USE_RESULT virtual bool HasNext() = 0;

  virtual absl::StatusOr<Element> Next(
      const ListValue::GetContext& context) = 0;

  virtual absl::StatusOr<size_t> NextIndex(
      const ListValue::GetContext& context);

  virtual absl::StatusOr<Handle<Value>> NextValue(
      const ListValue::GetContext& context);
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

  using ListValue::Is;

  static const LegacyListValue& Cast(const Value& value) {
    ABSL_ASSERT(Is(value));
    return static_cast<const LegacyListValue&>(value);
  }

  Handle<ListType> type() const;

  std::string DebugString() const;

  absl::StatusOr<Any> ConvertToAny(ValueFactory& value_factory) const;

  absl::StatusOr<Json> ConvertToJson(ValueFactory& value_factory) const;

  size_t size() const;

  bool empty() const;

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const;

  constexpr uintptr_t value() const { return impl_; }

  absl::StatusOr<UniqueRef<Iterator>> NewIterator(
      MemoryManager& memory_manager) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 private:
  friend class ValueHandle;
  friend class cel::ListValue;
  template <size_t Size, size_t Align>
  friend struct AnyData;

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

class AbstractListValue : public ListValue,
                          public HeapData,
                          public EnableOwnerFromThis<AbstractListValue> {
 public:
  static bool Is(const Value& value) {
    return value.kind() == kKind &&
           static_cast<const ListValue&>(value).TypeId() !=
               internal::TypeId<LegacyListValue>();
  }

  using ListValue::Is;

  static const AbstractListValue& Cast(const Value& value) {
    ABSL_ASSERT(Is(value));
    return static_cast<const AbstractListValue&>(value);
  }

  const Handle<ListType>& type() const { return type_; }

  virtual std::string DebugString() const = 0;

  virtual absl::StatusOr<Any> ConvertToAny(ValueFactory& value_factory) const;

  virtual absl::StatusOr<Json> ConvertToJson(ValueFactory& value_factory) const;

  virtual size_t size() const = 0;

  virtual bool empty() const { return size() == 0; }

  virtual absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                            size_t index) const = 0;

  virtual absl::StatusOr<UniqueRef<Iterator>> NewIterator(
      MemoryManager& memory_manager) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 protected:
  explicit AbstractListValue(Handle<ListType> type);

 private:
  friend class cel::ListValue;
  friend class LegacyListValue;
  friend internal::TypeInfo GetListValueTypeId(const ListValue& list_value);
  friend class ValueHandle;

  // Called by CEL_IMPLEMENT_LIST_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  const Handle<ListType> type_;
};

inline internal::TypeInfo GetListValueTypeId(const ListValue& list_value) {
  return list_value.TypeId();
}

class DynamicListValue final : public AbstractListValue {
 public:
  DynamicListValue(Handle<ListType> type,
                   std::vector<Handle<Value>, Allocator<Handle<Value>>> storage)
      : AbstractListValue(std::move(type)), storage_(std::move(storage)) {}

  std::string DebugString() const override {
    size_t count = size();
    std::string out;
    out.push_back('[');
    if (count != 0) {
      out.append(storage_[0]->DebugString());
      for (size_t index = 1; index < count; index++) {
        out.append(", ");
        out.append(storage_[index]->DebugString());
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const override {
    static_cast<void>(context);
    return storage_[index];
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<DynamicListValue>();
  }

 private:
  std::vector<Handle<Value>, Allocator<Handle<Value>>> storage_;
};

template <typename T>
class StaticListValue final : public AbstractListValue {
 public:
  using value_traits = ValueTraits<T>;
  using underlying_type = typename value_traits::underlying_type;

  StaticListValue(
      Handle<ListType> type,
      std::vector<underlying_type, Allocator<underlying_type>> storage)
      : AbstractListValue(std::move(type)), storage_(std::move(storage)) {}

  std::string DebugString() const override {
    size_t count = size();
    std::string out;
    out.push_back('[');
    if (count != 0) {
      out.append(value_traits::DebugString(storage_[0]));
      for (size_t index = 1; index < count; index++) {
        out.append(", ");
        out.append(value_traits::DebugString(storage_[index]));
      }
    }
    out.push_back(']');
    return out;
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const override {
    return value_traits::Wrap(context.value_factory(), storage_[index]);
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<StaticListValue<T>>();
  }

 private:
  std::vector<underlying_type, Allocator<underlying_type>> storage_;
};

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

namespace base_internal {

template <>
struct ValueTraits<ListValue> {
  using type = ListValue;

  using type_type = ListType;

  using underlying_type = void;

  static std::string DebugString(const type& value) {
    return value.DebugString();
  }

  static Handle<type> Wrap(ValueFactory& value_factory, Handle<type> value) {
    static_cast<void>(value_factory);
    return value;
  }

  static Handle<type> Unwrap(Handle<type> value) { return value; }
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_LIST_VALUE_H_

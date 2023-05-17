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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_LIST_VALUE_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_LIST_VALUE_BUILDER_H_

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/memory.h"
#include "base/value_factory.h"
#include "base/values/list_value.h"

namespace cel {

// Abstract interface for building ListValue.
//
// ListValueBuilderInterface is not reusable, once Build() is called the state
// of ListValueBuilderInterface is undefined.
class ListValueBuilderInterface {
 public:
  virtual ~ListValueBuilderInterface() = default;

  virtual std::string DebugString() const = 0;

  virtual absl::Status Add(Handle<Value> value) = 0;

  virtual size_t size() const = 0;

  virtual bool empty() const { return size() == 0; }

  virtual void reserve(size_t size) = 0;

  virtual absl::StatusOr<Handle<ListValue>> Build() && = 0;

 protected:
  explicit ListValueBuilderInterface(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory)
      : value_factory_(value_factory) {}

  ValueFactory& value_factory() const { return value_factory_; }

 private:
  ValueFactory& value_factory_;
};

// ListValueBuilder implements ListValueBuilderInterface, but is specialized for
// some types which have underlying C++ representations. When T is Value,
// ListValueBuilder has exactly the same methods as ListValueBuilderInterface.
// When T is not Value itself, each function that accepts Handle<Value> above
// also accepts Handle<T> variants. When T has some underlying C++
// representation, each function that accepts Handle<Value> above also accepts
// the underlying C++ representation.
//
// For example, ListValueBuilder<IntValue>::Add accepts Handle<Value>,
// Handle<IntValue> and int64_t.
template <typename T>
class ListValueBuilder;

namespace base_internal {

// Implementation of ListValueBuilder. Specialized to store some value types as
// C++ primitives, avoiding Handle overhead. Anything that does not have a C++
// primitive is stored as Handle<Value>.
template <typename T, typename U = void>
class ListValueBuilderImpl;

// Specialization for when the element type is not Value itself and has no C++
// primitive types.
template <typename T>
class ListValueBuilderImpl<T, void> : public ListValueBuilderInterface {
 public:
  static_assert(std::is_base_of_v<Value, T>);

  ListValueBuilderImpl(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
      Handle<typename ValueTraits<T>::type_type> type)
      : ListValueBuilderInterface(value_factory),
        type_(std::move(type)),
        storage_(Allocator<Handle<Value>>{value_factory.memory_manager()}) {}

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

  absl::Status Add(Handle<Value> value) override {
    return Add(std::move(value).As<T>());
  }

  absl::Status Add(Handle<T> value) {
    storage_.push_back(std::move(value));
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         value_factory().type_factory().CreateListType(type_));
    return value_factory()
        .template CreateListValue<base_internal::DynamicListValue>(
            std::move(type), std::move(storage_));
  }

 private:
  Handle<typename ValueTraits<T>::type_type> type_;
  std::vector<Handle<Value>, Allocator<Handle<Value>>> storage_;
};

// Specialization for when the element type is Value itself and has no C++
// primitive types.
template <>
class ListValueBuilderImpl<Value, void> : public ListValueBuilderInterface {
 public:
  ListValueBuilderImpl(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
      Handle<Type> type)
      : ListValueBuilderInterface(value_factory),
        type_(std::move(type)),
        storage_(Allocator<Handle<Value>>{value_factory.memory_manager()}) {}

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

  absl::Status Add(Handle<Value> value) override {
    storage_.push_back(std::move(value));
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         value_factory().type_factory().CreateListType(type_));
    return value_factory()
        .template CreateListValue<base_internal::DynamicListValue>(
            std::move(type), std::move(storage_));
  }

 private:
  Handle<Type> type_;
  std::vector<Handle<Value>, Allocator<Handle<Value>>> storage_;
};

// Specialization used when the element type has some C++ primitive
// representation.
template <typename T, typename U>
class ListValueBuilderImpl : public ListValueBuilderInterface {
 public:
  ListValueBuilderImpl(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
      Handle<typename ValueTraits<T>::type_type> type)
      : ListValueBuilderInterface(value_factory),
        type_(std::move(type)),
        storage_(Allocator<U>{value_factory.memory_manager()}) {}

  std::string DebugString() const override {
    using value_traits = ValueTraits<T>;
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

  absl::Status Add(Handle<Value> value) override {
    return Add(std::move(value).As<T>());
  }

  absl::Status Add(const Handle<T>& value) { return Add(value->value()); }

  absl::Status Add(U value) {
    storage_.push_back(std::move(value));
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto type,
                         value_factory().type_factory().CreateListType(type_));
    return value_factory()
        .template CreateListValue<base_internal::StaticListValue<T>>(
            std::move(type), std::move(storage_));
  }

 private:
  Handle<typename ValueTraits<T>::type_type> type_;
  std::vector<U, Allocator<U>> storage_;
};

}  // namespace base_internal

template <typename T>
class ListValueBuilder final
    : public base_internal::ListValueBuilderImpl<
          T, typename base_internal::ValueTraits<T>::underlying_type> {
 private:
  using Impl = base_internal::ListValueBuilderImpl<
      T, typename base_internal::ValueTraits<T>::underlying_type>;

 public:
  using Impl::Impl;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_LIST_VALUE_BUILDER_H_

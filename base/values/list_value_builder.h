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
#include "base/allocator.h"
#include "base/value_factory.h"
#include "base/values/list_value.h"

namespace cel {

// Abstract interface for building ListValue.
//
// ListValueBuilderInterface is not re-usable, once Build() is called the state
// of ListValueBuilderInterface is undefined.
class ListValueBuilderInterface {
 public:
  virtual ~ListValueBuilderInterface() = default;

  virtual std::string DebugString() const = 0;

  virtual absl::Status Add(const Handle<Value>& value) = 0;

  virtual absl::Status Add(Handle<Value>&& value) = 0;

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

// Implementation of ListValueBuilderInterface for types which are not
// specialized below.
template <typename T>
class ListValueBuilder final : public ListValueBuilderInterface {
 public:
  static_assert(std::is_base_of_v<Value, T>);

  ListValueBuilder(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
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

  absl::Status Add(const Handle<Value>& value) override {
    return Add(value.As<T>());
  }

  absl::Status Add(Handle<Value>&& value) override {
    return Add(value.As<T>());
  }

  absl::Status Add(const Handle<T>& value) {
    storage_.push_back(value);
    return absl::OkStatus();
  }

  absl::Status Add(Handle<T>&& value) {
    storage_.push_back(std::move(value));
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override;

 private:
  Handle<Type> type_;
  std::vector<Handle<Value>, Allocator<Handle<Value>>> storage_;
};

// Specialization of ListValueBuilder for Value.
template <>
class ListValueBuilder<Value> final : public ListValueBuilderInterface {
 public:
  ListValueBuilder(ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory,
                   Handle<Type> type)
      : ListValueBuilderInterface(value_factory),
        type_(std::move(type)),
        storage_(Allocator<Handle<Value>>{value_factory.memory_manager()}) {}

  std::string DebugString() const override;

  absl::Status Add(const Handle<Value>& value) override {
    storage_.push_back(value);
    return absl::OkStatus();
  }

  absl::Status Add(Handle<Value>&& value) override {
    storage_.push_back(std::move(value));
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override;

 private:
  Handle<Type> type_;
  std::vector<Handle<Value>, Allocator<Handle<Value>>> storage_;
};

// Specialization of ListValueBuilder for BoolValue.
template <>
class ListValueBuilder<BoolValue> final : public ListValueBuilderInterface {
 public:
  explicit ListValueBuilder(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory)
      : ListValueBuilderInterface(value_factory),
        storage_(Allocator<bool>{value_factory.memory_manager()}) {}

  std::string DebugString() const override;

  absl::Status Add(const Handle<Value>& value) override {
    return Add(value.As<BoolValue>());
  }

  absl::Status Add(Handle<Value>&& value) override {
    return Add(value.As<BoolValue>());
  }

  absl::Status Add(const Handle<BoolValue>& value) {
    return Add(value->value());
  }

  absl::Status Add(Handle<BoolValue>&& value) { return Add(value->value()); }

  absl::Status Add(bool value) {
    storage_.push_back(value);
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override;

 private:
  std::vector<bool, Allocator<bool>> storage_;
};

// Specialization of ListValueBuilder for IntValue.
template <>
class ListValueBuilder<IntValue> final : public ListValueBuilderInterface {
 public:
  explicit ListValueBuilder(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory)
      : ListValueBuilderInterface(value_factory),
        storage_(Allocator<int64_t>{value_factory.memory_manager()}) {}

  std::string DebugString() const override;

  absl::Status Add(const Handle<Value>& value) override {
    return Add(value.As<IntValue>());
  }

  absl::Status Add(Handle<Value>&& value) override {
    return Add(value.As<IntValue>());
  }

  absl::Status Add(const Handle<IntValue>& value) {
    return Add(value->value());
  }

  absl::Status Add(Handle<IntValue>&& value) { return Add(value->value()); }

  absl::Status Add(int64_t value) {
    storage_.push_back(value);
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override;

 private:
  std::vector<int64_t, Allocator<int64_t>> storage_;
};

// Specialization of ListValueBuilder for UintValue.
template <>
class ListValueBuilder<UintValue> final : public ListValueBuilderInterface {
 public:
  explicit ListValueBuilder(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory)
      : ListValueBuilderInterface(value_factory),
        storage_(Allocator<uint64_t>{value_factory.memory_manager()}) {}

  std::string DebugString() const override;

  absl::Status Add(const Handle<Value>& value) override {
    return Add(value.As<UintValue>());
  }

  absl::Status Add(Handle<Value>&& value) override {
    return Add(value.As<UintValue>());
  }

  absl::Status Add(const Handle<UintValue>& value) {
    return Add(value->value());
  }

  absl::Status Add(Handle<UintValue>&& value) { return Add(value->value()); }

  absl::Status Add(uint64_t value) {
    storage_.push_back(value);
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override;

 private:
  std::vector<uint64_t, Allocator<uint64_t>> storage_;
};

// Specialization of ListValueBuilder for DoubleValue.
template <>
class ListValueBuilder<DoubleValue> final : public ListValueBuilderInterface {
 public:
  explicit ListValueBuilder(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory)
      : ListValueBuilderInterface(value_factory),
        storage_(Allocator<double>{value_factory.memory_manager()}) {}

  std::string DebugString() const override;

  absl::Status Add(const Handle<Value>& value) override {
    return Add(value.As<DoubleValue>());
  }

  absl::Status Add(Handle<Value>&& value) override {
    return Add(value.As<DoubleValue>());
  }

  absl::Status Add(const Handle<DoubleValue>& value) {
    return Add(value->value());
  }

  absl::Status Add(Handle<DoubleValue>&& value) { return Add(value->value()); }

  absl::Status Add(double value) {
    storage_.push_back(value);
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override;

 private:
  std::vector<double, Allocator<double>> storage_;
};

// Specialization of ListValueBuilder for DurationValue.
template <>
class ListValueBuilder<DurationValue> final : public ListValueBuilderInterface {
 public:
  explicit ListValueBuilder(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory)
      : ListValueBuilderInterface(value_factory),
        storage_(Allocator<absl::Duration>{value_factory.memory_manager()}) {}

  std::string DebugString() const override;

  absl::Status Add(const Handle<Value>& value) override {
    return Add(value.As<DurationValue>());
  }

  absl::Status Add(Handle<Value>&& value) override {
    return Add(value.As<DurationValue>());
  }

  absl::Status Add(const Handle<DurationValue>& value) {
    return Add(value->value());
  }

  absl::Status Add(Handle<DurationValue>&& value) {
    return Add(value->value());
  }

  absl::Status Add(absl::Duration value) {
    storage_.push_back(value);
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override;

 private:
  std::vector<absl::Duration, Allocator<absl::Duration>> storage_;
};

// Specialization of ListValueBuilder for TimestampValue.
template <>
class ListValueBuilder<TimestampValue> final
    : public ListValueBuilderInterface {
 public:
  explicit ListValueBuilder(
      ABSL_ATTRIBUTE_LIFETIME_BOUND ValueFactory& value_factory)
      : ListValueBuilderInterface(value_factory),
        storage_(Allocator<absl::Time>{value_factory.memory_manager()}) {}

  std::string DebugString() const override;

  absl::Status Add(const Handle<Value>& value) override {
    return Add(value.As<TimestampValue>());
  }

  absl::Status Add(Handle<Value>&& value) override {
    return Add(value.As<TimestampValue>());
  }

  absl::Status Add(const Handle<TimestampValue>& value) {
    return Add(value->value());
  }

  absl::Status Add(Handle<TimestampValue>&& value) {
    return Add(value->value());
  }

  absl::Status Add(absl::Time value) {
    storage_.push_back(value);
    return absl::OkStatus();
  }

  size_t size() const override { return storage_.size(); }

  bool empty() const override { return storage_.empty(); }

  void reserve(size_t size) override { storage_.reserve(size); }

  absl::StatusOr<Handle<ListValue>> Build() && override;

 private:
  std::vector<absl::Time, Allocator<absl::Time>> storage_;
};

namespace base_internal {

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

  absl::StatusOr<Handle<Value>> Get(ValueFactory& value_factory,
                                    size_t index) const override {
    static_cast<void>(value_factory);
    return storage_[index];
  }

  internal::TypeInfo TypeId() const override {
    return internal::TypeId<DynamicListValue>();
  }

 private:
  std::vector<Handle<Value>, Allocator<Handle<Value>>> storage_;
};

}  // namespace base_internal

template <typename T>
absl::StatusOr<Handle<ListValue>> ListValueBuilder<T>::Build() && {
  CEL_ASSIGN_OR_RETURN(auto type, value_factory().type_factory().CreateListType(
                                      std::move(type_)));
  return value_factory()
      .template CreateListValue<base_internal::DynamicListValue>(
          std::move(type), std::move(storage_));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_LIST_VALUE_BUILDER_H_

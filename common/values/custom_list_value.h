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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

// `CustomListValue` represents values of the primitive `list` type.
// `CustomListValueView` is a non-owning view of `CustomListValue`.
// `CustomListValueInterface` is the abstract base class of implementations.
// `CustomListValue` and `CustomListValueView` act as smart pointers to
// `CustomListValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/value_kind.h"
#include "common/values/custom_value_interface.h"
#include "common/values/values.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class CustomListValueInterface;
class CustomListValueInterfaceIterator;
class CustomListValue;
class ValueManager;

// `Is` checks whether `lhs` and `rhs` have the same identity.
bool Is(const CustomListValue& lhs, const CustomListValue& rhs);

class CustomListValueInterface : public CustomValueInterface {
 public:
  using alternative_type = CustomListValue;

  static constexpr ValueKind kKind = ValueKind::kList;

  ValueKind kind() const final { return kKind; }

  absl::string_view GetTypeName() const final { return "list"; }

  using ForEachCallback = absl::FunctionRef<absl::StatusOr<bool>(const Value&)>;

  using ForEachWithIndexCallback =
      absl::FunctionRef<absl::StatusOr<bool>(size_t, const Value&)>;

  absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Cord& value) const override;

  virtual absl::Status Equal(ValueManager& value_manager, const Value& other,
                             Value& result) const;

  bool IsZeroValue() const { return IsEmpty(); }

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  // Returns a view of the element at index `index`. If the underlying
  // implementation cannot directly return a view of a value, the value will be
  // stored in `scratch`, and the returned view will be that of `scratch`.
  absl::Status Get(ValueManager& value_manager, size_t index,
                   Value& result) const;

  virtual absl::Status ForEach(ValueManager& value_manager,
                               ForEachCallback callback) const;

  virtual absl::Status ForEach(ValueManager& value_manager,
                               ForEachWithIndexCallback callback) const;

  virtual absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  virtual absl::Status Contains(ValueManager& value_manager, const Value& other,
                                Value& result) const;

  virtual CustomListValue Clone(ArenaAllocator<> allocator) const = 0;

 protected:
  friend class CustomListValueInterfaceIterator;

  virtual absl::Status GetImpl(ValueManager& value_manager, size_t index,
                               Value& result) const = 0;
};

class CustomListValue {
 public:
  using interface_type = CustomListValueInterface;

  static constexpr ValueKind kKind = CustomListValueInterface::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  CustomListValue(Shared<const CustomListValueInterface> interface)
      : interface_(std::move(interface)) {}

  // By default, this creates an empty list whose type is `list(dyn)`. Unless
  // you can help it, you should use a more specific typed list value.
  CustomListValue();
  CustomListValue(const CustomListValue&) = default;
  CustomListValue(CustomListValue&&) = default;
  CustomListValue& operator=(const CustomListValue&) = default;
  CustomListValue& operator=(CustomListValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return interface_->GetTypeName(); }

  std::string DebugString() const { return interface_->DebugString(); }

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Cord& value) const {
    return interface_->SerializeTo(descriptor_pool, message_factory, value);
  }

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const {
    return interface_->ConvertToJson(descriptor_pool, message_factory, json);
  }

  // See Value::ConvertToJsonArray().
  absl::Status ConvertToJsonArray(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const {
    return interface_->ConvertToJsonArray(descriptor_pool, message_factory,
                                          json);
  }

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;

  bool IsZeroValue() const { return interface_->IsZeroValue(); }

  CustomListValue Clone(Allocator<> allocator) const;

  bool IsEmpty() const { return interface_->IsEmpty(); }

  size_t Size() const { return interface_->Size(); }

  // See ListValueInterface::Get for documentation.
  absl::Status Get(ValueManager& value_manager, size_t index,
                   Value& result) const;

  using ForEachCallback = typename CustomListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename CustomListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachWithIndexCallback callback) const;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  absl::Status Contains(ValueManager& value_manager, const Value& other,
                        Value& result) const;

  void swap(CustomListValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  explicit operator bool() const { return static_cast<bool>(interface_); }

 private:
  friend struct NativeTypeTraits<CustomListValue>;
  friend bool Is(const CustomListValue& lhs, const CustomListValue& rhs);

  Shared<const CustomListValueInterface> interface_;
};

inline void swap(CustomListValue& lhs, CustomListValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const CustomListValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<CustomListValue> final {
  static NativeTypeId Id(const CustomListValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const CustomListValue& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<CustomListValue, T>>,
                               std::is_base_of<CustomListValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<CustomListValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<CustomListValue>::SkipDestructor(type);
  }
};

inline bool Is(const CustomListValue& lhs, const CustomListValue& rhs) {
  return lhs.interface_.operator->() == rhs.interface_.operator->();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_

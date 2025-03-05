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

// `CustomMapValue` represents values of the primitive `map` type.
// `CustomMapValueView` is a non-owning view of `CustomMapValue`.
// `CustomMapValueInterface` is the abstract base class of implementations.
// `CustomMapValue` and `CustomMapValueView` act as smart pointers to
// `CustomMapValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/value_kind.h"
#include "common/values/custom_value_interface.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class ListValue;
class CustomMapValueInterface;
class CustomMapValue;

class CustomMapValueInterface : public CustomValueInterface {
 public:
  using alternative_type = CustomMapValue;

  static constexpr ValueKind kKind = ValueKind::kMap;

  ValueKind kind() const final { return kKind; }

  absl::string_view GetTypeName() const final { return "map"; }

  using ForEachCallback =
      absl::FunctionRef<absl::StatusOr<bool>(const Value&, const Value&)>;

  absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<absl::Cord*> value) const override;

  absl::Status Equal(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<Value*> result) const override;

  bool IsZeroValue() const { return IsEmpty(); }

  // Returns `true` if this map contains no entries, `false` otherwise.
  virtual bool IsEmpty() const { return Size() == 0; }

  // Returns the number of entries in this map.
  virtual size_t Size() const = 0;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Get(const Value& key,
                   absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
                   absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
                   absl::Nonnull<google::protobuf::Arena*> arena,
                   absl::Nonnull<Value*> result) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<bool> Find(
      const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Has(const Value& key,
                   absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
                   absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
                   absl::Nonnull<google::protobuf::Arena*> arena,
                   absl::Nonnull<Value*> result) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  virtual absl::Status ListKeys(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<ListValue*> result) const = 0;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  virtual absl::Status ForEach(
      ForEachCallback callback,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  // By default, implementations do not guarantee any iteration order. Unless
  // specified otherwise, assume the iteration order is random.
  virtual absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator()
      const = 0;

  virtual CustomMapValue Clone(absl::Nonnull<google::protobuf::Arena*> arena) const = 0;

 protected:
  // Called by `Find` after performing various argument checks.
  virtual absl::StatusOr<bool> FindImpl(
      const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<Value*> result) const = 0;

  // Called by `Has` after performing various argument checks.
  virtual absl::StatusOr<bool> HasImpl(
      const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const = 0;
};

class CustomMapValue : private common_internal::MapValueMixin<CustomMapValue> {
 public:
  using interface_type = CustomMapValueInterface;

  static constexpr ValueKind kKind = CustomMapValueInterface::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  CustomMapValue(Shared<const CustomMapValueInterface> interface)
      : interface_(std::move(interface)) {}

  // By default, this creates an empty map whose type is `map(dyn, dyn)`. Unless
  // you can help it, you should use a more specific typed map value.
  CustomMapValue();
  CustomMapValue(const CustomMapValue&) = default;
  CustomMapValue(CustomMapValue&&) = default;
  CustomMapValue& operator=(const CustomMapValue&) = default;
  CustomMapValue& operator=(CustomMapValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return interface_->GetTypeName(); }

  std::string DebugString() const { return interface_->DebugString(); }

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<absl::Cord*> value) const {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(value != nullptr);

    return interface_->SerializeTo(descriptor_pool, message_factory, value);
  }

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(json != nullptr);

    return interface_->ConvertToJson(descriptor_pool, message_factory, json);
  }

  // See Value::ConvertToJsonObject().
  absl::Status ConvertToJsonObject(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(json != nullptr);

    return interface_->ConvertToJsonObject(descriptor_pool, message_factory,
                                           json);
  }

  absl::Status Equal(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(result != nullptr);

    return interface_->Equal(other, descriptor_pool, message_factory, arena,
                             result);
  }
  using MapValueMixin::Equal;

  bool IsZeroValue() const { return interface_->IsZeroValue(); }

  CustomMapValue Clone(absl::Nonnull<google::protobuf::Arena*> arena) const;

  bool IsEmpty() const { return interface_->IsEmpty(); }

  size_t Size() const { return interface_->Size(); }

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Get(const Value& key,
                   absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
                   absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
                   absl::Nonnull<google::protobuf::Arena*> arena,
                   absl::Nonnull<Value*> result) const {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(result != nullptr);

    return interface_->Get(key, descriptor_pool, message_factory, arena,
                           result);
  }
  using MapValueMixin::Get;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<bool> Find(
      const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(result != nullptr);

    return interface_->Find(key, descriptor_pool, message_factory, arena,
                            result);
  }
  using MapValueMixin::Find;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Has(const Value& key,
                   absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
                   absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
                   absl::Nonnull<google::protobuf::Arena*> arena,
                   absl::Nonnull<Value*> result) const {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(result != nullptr);

    return interface_->Has(key, descriptor_pool, message_factory, arena,
                           result);
  }
  using MapValueMixin::Has;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ListKeys(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<ListValue*> result) const {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(result != nullptr);

    return interface_->ListKeys(descriptor_pool, message_factory, arena,
                                result);
  }
  using MapValueMixin::ListKeys;

  // See the corresponding type declaration of `MapValueInterface` for
  // documentation.
  using ForEachCallback = typename CustomMapValueInterface::ForEachCallback;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ForEach(
      ForEachCallback callback,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);

    return interface_->ForEach(callback, descriptor_pool, message_factory,
                               arena);
  }

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator() const {
    return interface_->NewIterator();
  }

  void swap(CustomMapValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  explicit operator bool() const { return static_cast<bool>(interface_); }

 private:
  friend struct NativeTypeTraits<CustomMapValue>;
  friend class common_internal::ValueMixin<CustomMapValue>;
  friend class common_internal::MapValueMixin<CustomMapValue>;

  Shared<const CustomMapValueInterface> interface_;
};

inline void swap(CustomMapValue& lhs, CustomMapValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, const CustomMapValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<CustomMapValue> final {
  static NativeTypeId Id(const CustomMapValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const CustomMapValue& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<CustomMapValue, T>>,
                               std::is_base_of<CustomMapValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<CustomMapValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<CustomMapValue>::SkipDestructor(type);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_VALUE_H_

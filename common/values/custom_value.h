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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_CUSTOM_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_CUSTOM_VALUE_H_

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/native_type.h"
#include "common/value_kind.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;

// CustomValueContent is an opaque 16-byte trivially copyable value. The format
// of the data stored within is unknown to everything except the the caller
// which creates it. Do not try to interpret it otherwise.
class CustomValueContent final {
 public:
  static CustomValueContent Zero() {
    CustomValueContent content;
    std::memset(&content, 0, sizeof(content));
    return content;
  }

  template <typename T>
  static CustomValueContent From(T value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable");
    static_assert(sizeof(T) <= 16, "sizeof(T) must be no greater than 16");

    CustomValueContent content;
    std::memcpy(content.raw_, std::addressof(value), sizeof(T));
    return content;
  }

  template <typename T, size_t N>
  static CustomValueContent From(const T (&array)[N]) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable");
    static_assert((sizeof(T) * N) <= 16,
                  "sizeof(T[N]) must be no greater than 16");

    CustomValueContent content;
    std::memcpy(content.raw_, array, sizeof(T) * N);
    return content;
  }

  template <typename T>
  T To() const {
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable");
    static_assert(sizeof(T) <= 16, "sizeof(T) must be no greater than 16");

    T value;
    std::memcpy(std::addressof(value), raw_, sizeof(T));
    return value;
  }

 private:
  alignas(void*) std::byte raw_[16];
};

class CustomValueInterface {
 public:
  CustomValueInterface(const CustomValueInterface&) = delete;
  CustomValueInterface(CustomValueInterface&&) = delete;

  virtual ~CustomValueInterface() = default;

  CustomValueInterface& operator=(const CustomValueInterface&) = delete;
  CustomValueInterface& operator=(CustomValueInterface&&) = delete;

  virtual ValueKind kind() const = 0;

  virtual absl::string_view GetTypeName() const = 0;

  virtual std::string DebugString() const = 0;

  // See Value::SerializeTo().
  virtual absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<absl::Cord*> value) const;

  // See Value::ConvertToJson().
  virtual absl::Status ConvertToJson(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const;

  // See Value::ConvertToJsonArray().
  virtual absl::Status ConvertToJsonArray(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const;

  // See Value::ConvertToJsonObject().
  virtual absl::Status ConvertToJsonObject(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const;

  virtual absl::Status Equal(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<Value*> result) const = 0;

 protected:
  CustomValueInterface() = default;

 private:
  friend struct NativeTypeTraits<CustomValueInterface>;

  virtual NativeTypeId GetNativeTypeId() const = 0;
};

template <>
struct NativeTypeTraits<CustomValueInterface> final {
  static NativeTypeId Id(const CustomValueInterface& custom_value_interface) {
    return custom_value_interface.GetNativeTypeId();
  }
};

template <typename T>
struct NativeTypeTraits<
    T, std::enable_if_t<std::conjunction_v<
           std::is_base_of<CustomValueInterface, T>,
           std::negation<std::is_same<T, CustomValueInterface>>>>>
    final {
  static NativeTypeId Id(const CustomValueInterface& custom_value_interface) {
    return NativeTypeTraits<CustomValueInterface>::Id(custom_value_interface);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_CUSTOM_VALUE_H_

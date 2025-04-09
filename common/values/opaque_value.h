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
// IWYU pragma: friend "common/values/optional_value.h"

// `OpaqueValue` represents values of the `opaque` type. `OpaqueValueView`
// is a non-owning view of `OpaqueValue`. `OpaqueValueInterface` is the abstract
// base class of implementations. `OpaqueValue` and `OpaqueValueView` act as
// smart pointers to `OpaqueValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class OpaqueValueInterface;
class OpaqueValueInterfaceIterator;
class OpaqueValue;
class TypeFactory;
using OpaqueValueContent = CustomValueContent;

struct OpaqueValueDispatcher {
  using GetTypeId =
      NativeTypeId (*)(const OpaqueValueDispatcher* ABSL_NONNULL dispatcher,
                       OpaqueValueContent content);

  using GetArena = google::protobuf::Arena* ABSL_NULLABLE (*)(
      const OpaqueValueDispatcher* ABSL_NONNULL dispatcher,
      OpaqueValueContent content);

  using GetTypeName = absl::string_view (*)(
      const OpaqueValueDispatcher* ABSL_NONNULL dispatcher,
      OpaqueValueContent content);

  using DebugString =
      std::string (*)(const OpaqueValueDispatcher* ABSL_NONNULL dispatcher,
                      OpaqueValueContent content);

  using GetRuntimeType =
      OpaqueType (*)(const OpaqueValueDispatcher* ABSL_NONNULL dispatcher,
                     OpaqueValueContent content);

  using Equal = absl::Status (*)(
      const OpaqueValueDispatcher* ABSL_NONNULL dispatcher,
      OpaqueValueContent content, const OpaqueValue& other,
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::Arena* ABSL_NONNULL arena, Value* ABSL_NONNULL result);

  using Clone = OpaqueValue (*)(
      const OpaqueValueDispatcher* ABSL_NONNULL dispatcher,
      OpaqueValueContent content, google::protobuf::Arena* ABSL_NONNULL arena);

  ABSL_NONNULL GetTypeId get_type_id;

  ABSL_NONNULL GetArena get_arena;

  ABSL_NONNULL GetTypeName get_type_name;

  ABSL_NONNULL DebugString debug_string;

  ABSL_NONNULL GetRuntimeType get_runtime_type;

  ABSL_NONNULL Equal equal;

  ABSL_NONNULL Clone clone;
};

class OpaqueValueInterface {
 public:
  OpaqueValueInterface() = default;
  OpaqueValueInterface(const OpaqueValueInterface&) = delete;
  OpaqueValueInterface(OpaqueValueInterface&&) = delete;

  virtual ~OpaqueValueInterface() = default;

  OpaqueValueInterface& operator=(const OpaqueValueInterface&) = delete;
  OpaqueValueInterface& operator=(OpaqueValueInterface&&) = delete;

 private:
  friend class OpaqueValue;

  virtual std::string DebugString() const = 0;

  virtual absl::string_view GetTypeName() const = 0;

  virtual OpaqueType GetRuntimeType() const = 0;

  virtual absl::Status Equal(
      const OpaqueValue& other,
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::Arena* ABSL_NONNULL arena, Value* ABSL_NONNULL result) const = 0;

  virtual OpaqueValue Clone(google::protobuf::Arena* ABSL_NONNULL arena) const = 0;

  virtual NativeTypeId GetNativeTypeId() const = 0;

  struct Content {
    const OpaqueValueInterface* ABSL_NONNULL interface;
    google::protobuf::Arena* ABSL_NONNULL arena;
  };
};

// Creates an opaque value from a manual dispatch table `dispatcher` and
// opaque data `content` whose format is only know to functions in the manual
// dispatch table. The dispatch table should probably be valid for the lifetime
// of the process, but at a minimum must outlive all instances of the resulting
// value.
//
// IMPORTANT: This approach to implementing OpaqueValue should only be
// used when you know exactly what you are doing. When in doubt, just implement
// OpaqueValueInterface.
OpaqueValue UnsafeOpaqueValue(const OpaqueValueDispatcher* ABSL_NONNULL
                              dispatcher ABSL_ATTRIBUTE_LIFETIME_BOUND,
                              OpaqueValueContent content);

class OpaqueValue : private common_internal::OpaqueValueMixin<OpaqueValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kOpaque;

  // Constructs an opaque value from an implementation of
  // `OpaqueValueInterface` `interface` whose lifetime is tied to that of
  // the arena `arena`.
  OpaqueValue(const OpaqueValueInterface* ABSL_NONNULL
              interface ABSL_ATTRIBUTE_LIFETIME_BOUND,
              google::protobuf::Arena* ABSL_NONNULL arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    ABSL_DCHECK(interface != nullptr);
    ABSL_DCHECK(arena != nullptr);
    content_ = OpaqueValueContent::From(
        OpaqueValueInterface::Content{.interface = interface, .arena = arena});
  }

  OpaqueValue() = default;
  OpaqueValue(const OpaqueValue&) = default;
  OpaqueValue(OpaqueValue&&) = default;
  OpaqueValue& operator=(const OpaqueValue&) = default;
  OpaqueValue& operator=(OpaqueValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  NativeTypeId GetTypeId() const;

  OpaqueType GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::io::ZeroCopyOutputStream* ABSL_NONNULL output) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::Message* ABSL_NONNULL json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
                     google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
                     google::protobuf::Arena* ABSL_NONNULL arena,
                     Value* ABSL_NONNULL result) const;
  using OpaqueValueMixin::Equal;

  bool IsZeroValue() const { return false; }

  OpaqueValue Clone(google::protobuf::Arena* ABSL_NONNULL arena) const;

  // Returns `true` if this opaque value is an instance of an optional value.
  bool IsOptional() const;

  // Convenience method for use with template metaprogramming. See
  // `IsOptional()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, bool> Is() const {
    return IsOptional();
  }

  // Performs a checked cast from an opaque value to an optional value,
  // returning a non-empty optional with either a value or reference to the
  // optional value. Otherwise an empty optional is returned.
  optional_ref<const OptionalValue> AsOptional() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const OptionalValue> AsOptional()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<OptionalValue> AsOptional() &&;
  absl::optional<OptionalValue> AsOptional() const&&;

  // Convenience method for use with template metaprogramming. See
  // `AsOptional()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OptionalValue, T>,
                       optional_ref<const OptionalValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   optional_ref<const OptionalValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() &&;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() const&&;

  // Performs an unchecked cast from an opaque value to an optional value. In
  // debug builds a best effort is made to crash. If `IsOptional()` would return
  // false, calling this method is undefined behavior.
  const OptionalValue& GetOptional() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const OptionalValue& GetOptional() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  OptionalValue GetOptional() &&;
  OptionalValue GetOptional() const&&;

  // Convenience method for use with template metaprogramming. See
  // `Optional()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue> Get() &&;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue> Get()
      const&&;

  const OpaqueValueDispatcher* ABSL_NULLABLE dispatcher() const {
    return dispatcher_;
  }

  OpaqueValueContent content() const {
    ABSL_DCHECK(dispatcher_ != nullptr);
    return content_;
  }

  const OpaqueValueInterface* ABSL_NULLABLE interface() const {
    if (dispatcher_ == nullptr) {
      return content_.To<OpaqueValueInterface::Content>().interface;
    }
    return nullptr;
  }

  friend void swap(OpaqueValue& lhs, OpaqueValue& rhs) noexcept {
    using std::swap;
    swap(lhs.dispatcher_, rhs.dispatcher_);
    swap(lhs.content_, rhs.content_);
  }

  explicit operator bool() const {
    if (dispatcher_ == nullptr) {
      return content_.To<OpaqueValueInterface::Content>().interface != nullptr;
    }
    return true;
  }

 protected:
  OpaqueValue(const OpaqueValueDispatcher* ABSL_NONNULL dispatcher
                  ABSL_ATTRIBUTE_LIFETIME_BOUND,
              OpaqueValueContent content)
      : dispatcher_(dispatcher), content_(content) {
    ABSL_DCHECK(dispatcher != nullptr);
    ABSL_DCHECK(dispatcher->get_type_id != nullptr);
    ABSL_DCHECK(dispatcher->get_type_name != nullptr);
    ABSL_DCHECK(dispatcher->clone != nullptr);
  }

 private:
  friend class common_internal::ValueMixin<OpaqueValue>;
  friend class common_internal::OpaqueValueMixin<OpaqueValue>;
  friend OpaqueValue UnsafeOpaqueValue(const OpaqueValueDispatcher* ABSL_NONNULL
                                       dispatcher ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                       OpaqueValueContent content);

  const OpaqueValueDispatcher* ABSL_NULLABLE dispatcher_ = nullptr;
  OpaqueValueContent content_ = OpaqueValueContent::Zero();
};

inline std::ostream& operator<<(std::ostream& out, const OpaqueValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<OpaqueValue> final {
  static NativeTypeId Id(const OpaqueValue& type) { return type.GetTypeId(); }
};

inline OpaqueValue UnsafeOpaqueValue(const OpaqueValueDispatcher* ABSL_NONNULL
                                     dispatcher ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                     OpaqueValueContent content) {
  return OpaqueValue(dispatcher, content);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_

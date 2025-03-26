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
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/native_type.h"
#include "common/value_kind.h"
#include "common/values/custom_value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class ListValue;
class CustomMapValueInterface;
class CustomMapValueInterfaceKeysIterator;
class CustomMapValue;
using CustomMapValueContent = CustomValueContent;

struct CustomMapValueDispatcher {
  using GetTypeId = NativeTypeId (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content);

  using GetArena = absl::Nullable<google::protobuf::Arena*> (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content);

  using DebugString =
      std::string (*)(absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
                      CustomMapValueContent content);

  using SerializeTo = absl::Status (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::io::ZeroCopyOutputStream*> output);

  using ConvertToJsonObject = absl::Status (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json);

  using Equal = absl::Status (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content, const MapValue& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result);

  using IsZeroValue =
      bool (*)(absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
               CustomMapValueContent content);

  using IsEmpty =
      bool (*)(absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
               CustomMapValueContent content);

  using Size =
      size_t (*)(absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
                 CustomMapValueContent content);

  using Find = absl::StatusOr<bool> (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content, const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result);

  using Has = absl::StatusOr<bool> (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content, const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena);

  using ListKeys = absl::Status (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<ListValue*> result);

  using ForEach = absl::Status (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content,
      absl::FunctionRef<absl::StatusOr<bool>(const Value&, const Value&)>
          callback,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena);

  using NewIterator = absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content);

  using Clone = CustomMapValue (*)(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
      CustomMapValueContent content, absl::Nonnull<google::protobuf::Arena*> arena);

  absl::Nonnull<GetTypeId> get_type_id;

  absl::Nonnull<GetArena> get_arena;

  // If null, simply returns "map".
  absl::Nullable<DebugString> debug_string = nullptr;

  // If null, attempts to serialize results in an UNIMPLEMENTED error.
  absl::Nullable<SerializeTo> serialize_to = nullptr;

  // If null, attempts to convert to JSON results in an UNIMPLEMENTED error.
  absl::Nullable<ConvertToJsonObject> convert_to_json_object = nullptr;

  // If null, an nonoptimal fallback implementation for equality is used.
  absl::Nullable<Equal> equal = nullptr;

  absl::Nonnull<IsZeroValue> is_zero_value;

  // If null, `size(...) == 0` is used.
  absl::Nullable<IsEmpty> is_empty = nullptr;

  absl::Nonnull<Size> size;

  absl::Nonnull<Find> find;

  absl::Nonnull<Has> has;

  absl::Nonnull<ListKeys> list_keys;

  // If null, a fallback implementation based on `list_keys` is used.
  absl::Nullable<ForEach> for_each = nullptr;

  // If null, a fallback implementation based on `list_keys` is used.
  absl::Nullable<NewIterator> new_iterator = nullptr;

  absl::Nonnull<Clone> clone;
};

class CustomMapValueInterface {
 public:
  CustomMapValueInterface() = default;
  CustomMapValueInterface(const CustomMapValueInterface&) = delete;
  CustomMapValueInterface(CustomMapValueInterface&&) = delete;

  virtual ~CustomMapValueInterface() = default;

  CustomMapValueInterface& operator=(const CustomMapValueInterface&) = delete;
  CustomMapValueInterface& operator=(CustomMapValueInterface&&) = delete;

  using ForEachCallback =
      absl::FunctionRef<absl::StatusOr<bool>(const Value&, const Value&)>;

 private:
  friend class CustomMapValueInterfaceIterator;
  friend class CustomMapValue;
  friend absl::Status common_internal::MapValueEqual(
      const CustomMapValueInterface& lhs, const MapValue& rhs,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result);

  virtual std::string DebugString() const = 0;

  virtual absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::io::ZeroCopyOutputStream*> output) const;

  virtual absl::Status ConvertToJsonObject(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const = 0;

  virtual absl::Status Equal(
      const MapValue& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;

  virtual bool IsZeroValue() const { return IsEmpty(); }

  // Returns `true` if this map contains no entries, `false` otherwise.
  virtual bool IsEmpty() const { return Size() == 0; }

  // Returns the number of entries in this map.
  virtual size_t Size() const = 0;

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
  virtual absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator() const;

  virtual CustomMapValue Clone(absl::Nonnull<google::protobuf::Arena*> arena) const = 0;

  virtual absl::StatusOr<bool> Find(
      const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<Value*> result) const = 0;

  virtual absl::StatusOr<bool> Has(
      const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const = 0;

  virtual NativeTypeId GetNativeTypeId() const = 0;

  struct Content {
    absl::Nonnull<const CustomMapValueInterface*> interface;
    absl::Nonnull<google::protobuf::Arena*> arena;
  };
};

// Creates a custom map value from a manual dispatch table `dispatcher` and
// opaque data `content` whose format is only know to functions in the manual
// dispatch table. The dispatch table should probably be valid for the lifetime
// of the process, but at a minimum must outlive all instances of the resulting
// value.
//
// IMPORTANT: This approach to implementing CustomMapValue should only be
// used when you know exactly what you are doing. When in doubt, just implement
// CustomMapValueInterface.
CustomMapValue UnsafeCustomMapValue(
    absl::Nonnull<const CustomMapValueDispatcher*> dispatcher
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    CustomMapValueContent content);

class CustomMapValue final
    : private common_internal::MapValueMixin<CustomMapValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kMap;

  // Constructs a custom map value from an implementation of
  // `CustomMapValueInterface` `interface` whose lifetime is tied to that of
  // the arena `arena`.
  CustomMapValue(absl::Nonnull<const CustomMapValueInterface*>
                     interface ABSL_ATTRIBUTE_LIFETIME_BOUND,
                 absl::Nonnull<google::protobuf::Arena*> arena
                     ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    ABSL_DCHECK(interface != nullptr);
    ABSL_DCHECK(arena != nullptr);
    content_ = CustomMapValueContent::From(CustomMapValueInterface::Content{
        .interface = interface, .arena = arena});
  }

  // By default, this creates an empty map whose type is `map(dyn, dyn)`. Unless
  // you can help it, you should use a more specific typed map value.
  CustomMapValue();
  CustomMapValue(const CustomMapValue&) = default;
  CustomMapValue(CustomMapValue&&) = default;
  CustomMapValue& operator=(const CustomMapValue&) = default;
  CustomMapValue& operator=(CustomMapValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  NativeTypeId GetTypeId() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::io::ZeroCopyOutputStream*> output) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const;

  // See Value::ConvertToJsonObject().
  absl::Status ConvertToJsonObject(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const;

  absl::Status Equal(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;
  using MapValueMixin::Equal;

  bool IsZeroValue() const;

  CustomMapValue Clone(absl::Nonnull<google::protobuf::Arena*> arena) const;

  bool IsEmpty() const;

  size_t Size() const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Get(const Value& key,
                   absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
                   absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
                   absl::Nonnull<google::protobuf::Arena*> arena,
                   absl::Nonnull<Value*> result) const;
  using MapValueMixin::Get;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<bool> Find(
      const Value& key,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;
  using MapValueMixin::Find;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Has(const Value& key,
                   absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
                   absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
                   absl::Nonnull<google::protobuf::Arena*> arena,
                   absl::Nonnull<Value*> result) const;
  using MapValueMixin::Has;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ListKeys(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<ListValue*> result) const;
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
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator() const;

  absl::Nullable<const CustomMapValueDispatcher*> dispatcher() const {
    return dispatcher_;
  }

  CustomMapValueContent content() const {
    ABSL_DCHECK(dispatcher_ != nullptr);
    return content_;
  }

  absl::Nullable<const CustomMapValueInterface*> interface() const {
    if (dispatcher_ == nullptr) {
      return content_.To<CustomMapValueInterface::Content>().interface;
    }
    return nullptr;
  }

  friend void swap(CustomMapValue& lhs, CustomMapValue& rhs) noexcept {
    using std::swap;
    swap(lhs.dispatcher_, rhs.dispatcher_);
    swap(lhs.content_, rhs.content_);
  }

 private:
  friend class common_internal::ValueMixin<CustomMapValue>;
  friend class common_internal::MapValueMixin<CustomMapValue>;
  friend CustomMapValue UnsafeCustomMapValue(
      absl::Nonnull<const CustomMapValueDispatcher*> dispatcher
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      CustomMapValueContent content);

  CustomMapValue(absl::Nonnull<const CustomMapValueDispatcher*> dispatcher,
                 CustomMapValueContent content)
      : dispatcher_(dispatcher), content_(content) {
    ABSL_DCHECK(dispatcher != nullptr);
    ABSL_DCHECK(dispatcher->get_type_id != nullptr);
    ABSL_DCHECK(dispatcher->get_arena != nullptr);
    ABSL_DCHECK(dispatcher->is_zero_value != nullptr);
    ABSL_DCHECK(dispatcher->size != nullptr);
    ABSL_DCHECK(dispatcher->find != nullptr);
    ABSL_DCHECK(dispatcher->has != nullptr);
    ABSL_DCHECK(dispatcher->list_keys != nullptr);
    ABSL_DCHECK(dispatcher->clone != nullptr);
  }

  absl::Nullable<const CustomMapValueDispatcher*> dispatcher_ = nullptr;
  CustomMapValueContent content_ = CustomMapValueContent::Zero();
};

inline std::ostream& operator<<(std::ostream& out, const CustomMapValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<CustomMapValue> final {
  static NativeTypeId Id(const CustomMapValue& type) {
    return type.GetTypeId();
  }
};

inline CustomMapValue UnsafeCustomMapValue(
    absl::Nonnull<const CustomMapValueDispatcher*> dispatcher
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    CustomMapValueContent content) {
  return CustomMapValue(dispatcher, content);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_VALUE_H_

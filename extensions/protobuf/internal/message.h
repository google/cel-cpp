// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_MESSAGE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_MESSAGE_H_

#include <cstddef>
#include <memory>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

template <typename T>
inline constexpr bool IsProtoMessage =
    std::conjunction_v<std::is_base_of<google::protobuf::Message, std::decay_t<T>>,
                       std::negation<std::is_same<google::protobuf::Message, T>>>;

template <typename T>
struct ProtoMessageTraits {
  static_assert(IsProtoMessage<T>);

  static absl::Nonnull<google::protobuf::Message*> ArenaCopyConstruct(
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<const google::protobuf::Message*> from) {
    if constexpr (google::protobuf::Arena::is_arena_constructable<T>::value) {
      return google::protobuf::Arena::Create<T>(arena,
                                      *google::protobuf::DynamicCastToGenerated<T>(from));
    } else {
      auto* to = google::protobuf::Arena::Create<T>(arena);
      *to = *google::protobuf::DynamicCastToGenerated<T>(from);
      return to;
    }
  }

  static absl::Nonnull<google::protobuf::Message*> CopyConstruct(
      absl::Nonnull<void*> address,
      absl::Nonnull<const google::protobuf::Message*> from) {
    return ::new (address) T(*google::protobuf::DynamicCastToGenerated<T>(from));
  }

  static absl::Nonnull<google::protobuf::Message*> ArenaMoveConstruct(
      absl::Nonnull<google::protobuf::Arena*> arena,
      absl::Nonnull<google::protobuf::Message*> from) {
    if constexpr (google::protobuf::Arena::is_arena_constructable<T>::value) {
      return google::protobuf::Arena::Create<T>(
          arena, std::move(*google::protobuf::DynamicCastToGenerated<T>(from)));
    } else {
      auto* to = google::protobuf::Arena::Create<T>(arena);
      *to = std::move(*google::protobuf::DynamicCastToGenerated<T>(from));
      return to;
    }
  }

  static absl::Nonnull<google::protobuf::Message*> MoveConstruct(
      absl::Nonnull<void*> address, absl::Nonnull<google::protobuf::Message*> from) {
    return ::new (address)
        T(std::move(*google::protobuf::DynamicCastToGenerated<T>(from)));
  }
};

// Get the `google::protobuf::Descriptor` from `google::protobuf::Message`, or return an error if it
// is `nullptr`. This should be extremely rare.
absl::StatusOr<absl::Nonnull<const google::protobuf::Descriptor*>> GetDescriptor(
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND);

// Get the `google::protobuf::Reflection` from `google::protobuf::Message`, or return an error if it
// is `nullptr`. This should be extremely rare.
absl::StatusOr<absl::Nonnull<const google::protobuf::Reflection*>> GetReflection(
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND);

// Get the `google::protobuf::Reflection` from `google::protobuf::Message`, or abort.
// Should only be used when it is guaranteed `google::protobuf::Message` has reflection.
absl::Nonnull<const google::protobuf::Reflection*> GetReflectionOrDie(
    const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND);

using ProtoMessageArenaCopyConstructor = absl::Nonnull<google::protobuf::Message*> (*)(
    absl::Nonnull<google::protobuf::Arena*>, absl::Nonnull<const google::protobuf::Message*>);

using ProtoMessageCopyConstructor = absl::Nonnull<google::protobuf::Message*> (*)(
    absl::Nonnull<void*>, absl::Nonnull<const google::protobuf::Message*>);

using ProtoMessageArenaMoveConstructor = absl::Nonnull<google::protobuf::Message*> (*)(
    absl::Nonnull<google::protobuf::Arena*>, absl::Nonnull<google::protobuf::Message*>);

using ProtoMessageMoveConstructor = absl::Nonnull<google::protobuf::Message*> (*)(
    absl::Nonnull<void*>, absl::Nonnull<google::protobuf::Message*>);

// Adapts a protocol buffer message to a value, copying it.
absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueManager& value_manager, absl::Nonnull<const google::protobuf::Message*> message,
    size_t size, size_t align,
    absl::Nonnull<ProtoMessageArenaCopyConstructor> arena_copy_construct,
    absl::Nonnull<ProtoMessageCopyConstructor> copy_construct);

// Adapts a protocol buffer message to a value, moving it if possible.
absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueManager& value_manager, absl::Nonnull<google::protobuf::Message*> message,
    size_t size, size_t align,
    absl::Nonnull<ProtoMessageArenaMoveConstructor> arena_move_construct,
    absl::Nonnull<ProtoMessageMoveConstructor> move_construct);

// Aliasing conversion. Assumes `aliased` is the owner of `message`.
absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueManager& value_manager, Shared<const void> aliased,
    absl::Nonnull<const google::protobuf::Message*> message);

// Adapts a protocol buffer message to a struct value, taking ownership of the
// message. `message` must not be a well known type. If the `MemoryManager` used
// by `value_factory` is backed by `google::protobuf::Arena`, `message` must have been
// created on the arena.
StructValue ProtoMessageAsStructValueImpl(
    ValueFactory& value_factory, absl::Nonnull<google::protobuf::Message*> message);

// Adapts a serialized protocol buffer message to a value. `prototype` should be
// the prototype message returned from the message factory.
absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueFactory& value_factory, const TypeReflector& type_reflector,
    absl::Nonnull<const google::protobuf::Message*> prototype,
    const absl::Cord& serialized);

// Converts a value to a protocol buffer message.
absl::StatusOr<absl::Nonnull<google::protobuf::Message*>> ProtoMessageFromValueImpl(
    const Value& value, absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory, google::protobuf::Arena* arena);
inline absl::StatusOr<absl::Nonnull<google::protobuf::Message*>>
ProtoMessageFromValueImpl(const Value& value, google::protobuf::Arena* arena) {
  return ProtoMessageFromValueImpl(
      value, google::protobuf::DescriptorPool::generated_pool(),
      google::protobuf::MessageFactory::generated_factory(), arena);
}

// Converts a value to a specific protocol buffer message.
absl::Status ProtoMessageFromValueImpl(
    const Value& value, absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory,
    absl::Nonnull<google::protobuf::Message*> message);
inline absl::Status ProtoMessageFromValueImpl(
    const Value& value, absl::Nonnull<google::protobuf::Message*> message) {
  return ProtoMessageFromValueImpl(
      value, google::protobuf::DescriptorPool::generated_pool(),
      google::protobuf::MessageFactory::generated_factory(), message);
}

// Converts a value to a specific protocol buffer map key.
using ProtoMapKeyFromValueConverter = absl::Status (*)(const Value&,
                                                       google::protobuf::MapKey&);

// Gets the converter for converting from values to protocol buffer map key.
absl::StatusOr<ProtoMapKeyFromValueConverter> GetProtoMapKeyFromValueConverter(
    google::protobuf::FieldDescriptor::CppType cpp_type);

// Converts a value to a specific protocol buffer map value.
using ProtoMapValueFromValueConverter = absl::Status (*)(
    const Value&, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef&);

// Gets the converter for converting from values to protocol buffer map value.
absl::StatusOr<ProtoMapValueFromValueConverter>
GetProtoMapValueFromValueConverter(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field);

struct DefaultArenaDeleter {
  template <typename T>
  void operator()(T* message) const {
    if (arena == nullptr) {
      delete message;
    }
  }

  google::protobuf::Arena* arena = nullptr;
};

// Smart pointer for a protocol buffer message that may or mot not be allocated
// on an arena.
template <typename T>
using ArenaUniquePtr = std::unique_ptr<T, DefaultArenaDeleter>;

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_MESSAGE_H_

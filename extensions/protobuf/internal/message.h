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
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "common/memory.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
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
ABSL_ATTRIBUTE_PURE_FUNCTION absl::Nonnull<const google::protobuf::Reflection*>
GetReflectionOrDie(
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

// Adapts a serialized protocol buffer message to a value. `prototype` should be
// the prototype message returned from the message factory.
absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueFactory& value_factory, const TypeReflector& type_reflector,
    absl::Nonnull<const google::protobuf::Message*> prototype,
    const absl::Cord& serialized);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_MESSAGE_H_

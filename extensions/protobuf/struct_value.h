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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_STRUCT_VALUE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/log/die_if_null.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/owner.h"
#include "base/type.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/struct_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/struct_type.h"
#include "internal/casts.h"
#include "internal/rtti.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

class ProtoValue;

// ProtoStructValue is an implementation of StructValue using protocol buffer
// messages. ProtoStructValue can represented parsed or
// serialized protocol buffer messages. Currently only parsed protocol buffer
// messages are implemented, but support for serialized protocol buffer messages
// will be added in the future.
class ProtoStructValue : public CEL_STRUCT_VALUE_CLASS {
 private:
  template <typename T, typename R>
  using EnableIfDerivedMessage =
      std::enable_if_t<(!std::is_same_v<google::protobuf::Message, T> &&
                        std::is_base_of_v<google::protobuf::Message, T>),
                       R>;

 public:
  static bool Is(const Value& value) {
    return value.kind() == Kind::kStruct &&
           cel::base_internal::GetStructValueTypeId(
               static_cast<const StructValue&>(value)) ==
               cel::internal::TypeId<ProtoStructValue>();
  }

  using CEL_STRUCT_VALUE_CLASS::Is;

  static const ProtoStructValue& Cast(const Value& value) {
    ABSL_ASSERT(Is(value));
    return static_cast<const ProtoStructValue&>(value);
  }

  using CEL_STRUCT_VALUE_CLASS::DebugString;

  const Handle<ProtoStructType>& type() const {
    return CEL_STRUCT_VALUE_CLASS::type().As<ProtoStructType>();
  }

  // Gets a reference to the concrete protocol buffer message. If the
  // encapsulated protocol buffer message is not the same as T, an empty
  // optional is returned. Otherwise a constant lvalue reference is returned.
  // The lvalue reference may be referencing scratch or some underlying storage.
  // It is primarily used when converting from a serialized protocol buffer
  // message.
  //
  // ```
  // Handle<ProtoStructValue> value /* = ... */;
  // MyProtoMessage scratch;
  // if (auto message = value->value(scratch); message) {
  //   /* success */
  // }
  // ```
  template <typename T>
  EnableIfDerivedMessage<T, const T*> value(
      ABSL_ATTRIBUTE_LIFETIME_BOUND T& scratch) const {
    auto maybe_value =
        ValueReference(scratch, *ABSL_DIE_IF_NULL(T::descriptor()),  // Crash OK
                       internal::TypeId<T>());
    if (ABSL_PREDICT_FALSE(!maybe_value.has_value())) {
      return nullptr;
    }
    const auto* value_ptr = *maybe_value;
    return value_ptr != nullptr ? cel::internal::down_cast<const T*>(value_ptr)
                                : &scratch;
  }

  // Gets a copy of the encapsulated protocol buffer message. The caller owns
  // the returned copy. In the event that deserialization is needed and fails,
  // nullptr is returned.
  std::unique_ptr<google::protobuf::Message> value() const;
  std::unique_ptr<google::protobuf::Message> value(
      ABSL_ATTRIBUTE_LIFETIME_BOUND google::protobuf::MessageFactory& message_factory)
      const;

  // Gets a copy of the encapsulated protocol buffer message. The arena owns the
  // returned copy. In the event that deserialization is needed and fails,
  // nullptr is returned.
  google::protobuf::Message* value(ABSL_ATTRIBUTE_LIFETIME_BOUND google::protobuf::Arena& arena,
                         ABSL_ATTRIBUTE_LIFETIME_BOUND google::protobuf::MessageFactory&
                             message_factory) const;
  google::protobuf::Message* value(
      ABSL_ATTRIBUTE_LIFETIME_BOUND google::protobuf::Arena& arena) const;

 protected:
  explicit ProtoStructValue(Handle<StructType> type)
      : CEL_STRUCT_VALUE_CLASS(std::move(type)) {}

  // Returns an empty optional if we are unable to deserialize the message or
  // there is a type mismatch. Returns a nullptr if scratch was used otherwise a
  // pointer to the parsed protocol buffer message.
  virtual absl::optional<const google::protobuf::Message*> ValueReference(
      google::protobuf::Message& scratch, const google::protobuf::Descriptor& desc,
      internal::TypeInfo type) const = 0;

  virtual google::protobuf::Message* ValuePointer(google::protobuf::MessageFactory& message_factory,
                                        google::protobuf::Arena* arena) const = 0;

 private:
  friend class ProtoValue;

  template <typename T>
  static EnableIfDerivedMessage<T, absl::StatusOr<Handle<ProtoStructValue>>>
  Create(ValueFactory& value_factory, T&& value);

  template <typename T>
  static EnableIfDerivedMessage<T, absl::StatusOr<Handle<ProtoStructValue>>>
  CreateBorrowed(Owner<Value> owner, ValueFactory& value_factory,
                 const T& value ABSL_ATTRIBUTE_LIFETIME_BOUND);

  static absl::StatusOr<Handle<ProtoStructValue>> Create(
      ValueFactory& value_factory, const google::protobuf::Message& message);

  static absl::StatusOr<Handle<ProtoStructValue>> CreateBorrowed(
      Owner<Value> owner, ValueFactory& value_factory,
      const google::protobuf::Message& message ABSL_ATTRIBUTE_LIFETIME_BOUND);

  static absl::StatusOr<Handle<ProtoStructValue>> Create(
      ValueFactory& value_factory, google::protobuf::Message&& message);

  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ProtoStructValue>();
  }
};

// -----------------------------------------------------------------------------
// Implementation details

namespace protobuf_internal {

// Declare here but implemented in value.cc to give ProtoStructValue access to
// the conversion logic in value.cc. Creates a borrowed `ListValue` over
// `google.protobuf.ListValue`.
//
// Borrowing here means we are borrowing some native representation owned by
// `owner` and creating a new value which references that native representation,
// but does not own it.
absl::StatusOr<Handle<Value>> CreateBorrowedListValue(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::Message& value ABSL_ATTRIBUTE_LIFETIME_BOUND);

// Declare here but implemented in value.cc to give ProtoStructValue access to
// the conversion logic in value.cc. Creates a borrowed `MapValue` over
// `google.protobuf.Struct`.
//
// Borrowing here means we are borrowing some native representation owned by
// `owner` and creating a new value which references that native representation,
// but does not own it.
absl::StatusOr<Handle<Value>> CreateBorrowedStruct(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::Message& value ABSL_ATTRIBUTE_LIFETIME_BOUND);

// Declare here but implemented in value.cc to give ProtoStructValue access to
// the conversion logic in value.cc. Creates a borrowed `Value` over
// `google.protobuf.Value`.
//
// Borrowing here means we are borrowing some native representation owned by
// `owner` and creating a new value which references that native representation,
// but does not own it.
absl::StatusOr<Handle<Value>> CreateBorrowedValue(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::Message& value ABSL_ATTRIBUTE_LIFETIME_BOUND);

absl::StatusOr<Handle<Value>> CreateListValue(
    ValueFactory& value_factory, std::unique_ptr<google::protobuf::Message> value);

absl::StatusOr<Handle<Value>> CreateStruct(
    ValueFactory& value_factory, std::unique_ptr<google::protobuf::Message> value);

absl::StatusOr<Handle<Value>> CreateValue(
    ValueFactory& value_factory, std::unique_ptr<google::protobuf::Message> value);

// Base class of all implementations of `ProtoStructValue` that operate on
// parsed protocol buffer messages.
class ParsedProtoStructValue : public ProtoStructValue {
 public:
  static bool Is(const Value& value) {
    // Right now all ProtoStructValue are ParsedProtoStructValue. We need to
    // update this if anything changes.
    return ProtoStructValue::Is(value);
  }

  using ProtoStructValue::Is;

  static const ParsedProtoStructValue& Cast(const Value& value) {
    ABSL_ASSERT(Is(value));
    return static_cast<const ParsedProtoStructValue&>(value);
  }

  static std::string DebugString(const google::protobuf::Message& message);

  std::string DebugString() const final;

  absl::StatusOr<Handle<Value>> GetFieldByName(
      const GetFieldContext& context, absl::string_view name) const final;

  absl::StatusOr<Handle<Value>> GetFieldByNumber(const GetFieldContext& context,
                                                 int64_t number) const final;

  absl::StatusOr<bool> HasFieldByName(const HasFieldContext& context,
                                      absl::string_view name) const final;

  absl::StatusOr<bool> HasFieldByNumber(const HasFieldContext& context,
                                        int64_t number) const final;

  using ProtoStructValue::value;

  virtual const google::protobuf::Message& value() const = 0;

 protected:
  explicit ParsedProtoStructValue(Handle<StructType> type)
      : ProtoStructValue(std::move(type)) {}

  google::protobuf::Message* ValuePointer(google::protobuf::MessageFactory& message_factory,
                                google::protobuf::Arena* arena) const final;

  absl::StatusOr<Handle<Value>> GetField(const GetFieldContext& context,
                                         const StructType::Field& field) const;

  absl::StatusOr<Handle<Value>> GetMapField(
      const GetFieldContext& context, const StructType::Field& field,
      const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc) const;

  absl::StatusOr<Handle<Value>> GetRepeatedField(
      const GetFieldContext& context, const StructType::Field& field,
      const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc) const;

  absl::StatusOr<Handle<Value>> GetSingularField(
      const GetFieldContext& context, const StructType::Field& field,
      const google::protobuf::Reflection& reflect,
      const google::protobuf::FieldDescriptor& field_desc) const;

  absl::StatusOr<bool> HasField(TypeManager& type_manager,
                                const StructType::Field& field) const;
};

// Implementation of `ParsedProtoStructValue` which knows the concrete type of
// the protocol buffer message. The protocol buffer message is stored by value.
template <typename T>
class StaticParsedProtoStructValue final : public ParsedProtoStructValue {
 public:
  StaticParsedProtoStructValue(Handle<StructType> type, T&& value)
      : ParsedProtoStructValue(std::move(type)),
        value_(std::forward<T>(value)) {}

  const google::protobuf::Message& value() const override { return value_; }

 protected:
  absl::optional<const google::protobuf::Message*> ValueReference(
      google::protobuf::Message& scratch, const google::protobuf::Descriptor& desc,
      internal::TypeInfo type) const override {
    static_cast<void>(scratch);
    static_cast<void>(desc);
    if (ABSL_PREDICT_FALSE(type != internal::TypeId<T>())) {
      return absl::nullopt;
    }
    ABSL_ASSERT(value().GetDescriptor() == &desc);
    return &value();
  }

 private:
  const T value_;
};

template <typename T>
class HeapStaticParsedProtoStructValue : public ParsedProtoStructValue {
 public:
  HeapStaticParsedProtoStructValue(Handle<StructType> type, const T* value)
      : ParsedProtoStructValue(std::move(type)), value_(value) {}

  const google::protobuf::Message& value() const final { return *value_; }

 protected:
  absl::optional<const google::protobuf::Message*> ValueReference(
      google::protobuf::Message& scratch, const google::protobuf::Descriptor& desc,
      internal::TypeInfo type) const final {
    static_cast<void>(scratch);
    static_cast<void>(desc);
    if (ABSL_PREDICT_FALSE(type != internal::TypeId<T>())) {
      return absl::nullopt;
    }
    ABSL_ASSERT(value().GetDescriptor() == &desc);
    return &value();
  }

 private:
  const T* const value_;
};

// Base implementation of `ParsedProtoStructValue` which does not know the
// concrete type of the protocol buffer message. The protocol buffer message is
// referenced by pointer and is allocated with the same memory manager that
// allocated this.
class DynamicParsedProtoStructValue : public ParsedProtoStructValue {
 public:
  const google::protobuf::Message& value() const final { return *value_; }

 protected:
  DynamicParsedProtoStructValue(Handle<StructType> type,
                                const google::protobuf::Message* value)
      : ParsedProtoStructValue(std::move(type)),
        value_(ABSL_DIE_IF_NULL(value)) {}  // Crash OK

  absl::optional<const google::protobuf::Message*> ValueReference(
      google::protobuf::Message& scratch, const google::protobuf::Descriptor& desc,
      internal::TypeInfo type) const final {
    if (ABSL_PREDICT_FALSE(&desc != scratch.GetDescriptor())) {
      return absl::nullopt;
    }
    return &value();
  }

  const google::protobuf::Message* value_ptr() const { return value_; }

 private:
  const google::protobuf::Message* const value_;
};

// Implementation of `DynamicParsedProtoStructValue` for Arena-based memory
// managers.
class ArenaDynamicParsedProtoStructValue
    : public DynamicParsedProtoStructValue {
 public:
  ArenaDynamicParsedProtoStructValue(Handle<StructType> type,
                                     const google::protobuf::Message* value)
      : DynamicParsedProtoStructValue(std::move(type), value) {
    ABSL_ASSERT(value->GetArena() != nullptr);
  }
};

}  // namespace protobuf_internal

template <typename T>
inline ProtoStructValue::EnableIfDerivedMessage<
    T, absl::StatusOr<Handle<ProtoStructValue>>>
ProtoStructValue::Create(ValueFactory& value_factory, T&& value) {
  CEL_ASSIGN_OR_RETURN(
      auto type, ProtoStructType::Resolve<T>(value_factory.type_manager()));
  if (google::protobuf::Arena::is_arena_constructable<T>::value &&
      ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (ABSL_PREDICT_TRUE(arena != nullptr)) {
      auto* arena_value = google::protobuf::Arena::CreateMessage<T>(arena);
      *arena_value = std::forward<T>(value);
      return value_factory.CreateStructValue<
          protobuf_internal::ArenaDynamicParsedProtoStructValue>(
          std::move(type), arena_value);
    }
  }
  return value_factory
      .CreateStructValue<protobuf_internal::StaticParsedProtoStructValue<T>>(
          std::move(type), std::forward<T>(value));
}

template <typename T>
inline ProtoStructValue::EnableIfDerivedMessage<
    T, absl::StatusOr<Handle<ProtoStructValue>>>
ProtoStructValue::CreateBorrowed(Owner<Value> owner,
                                 ValueFactory& value_factory, const T& value) {
  CEL_ASSIGN_OR_RETURN(
      auto type, ProtoStructType::Resolve<T>(value_factory.type_manager()));
  return value_factory.CreateBorrowedStructValue<
      protobuf_internal::HeapStaticParsedProtoStructValue<T>>(
      std::move(owner), std::move(type), &value);
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_STRUCT_VALUE_H_

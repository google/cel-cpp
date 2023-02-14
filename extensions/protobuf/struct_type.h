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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_STRUCT_TYPE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_STRUCT_TYPE_H_

#include <type_traits>

#include "absl/log/die_if_null.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/type_factory.h"
#include "base/types/struct_type.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

class ProtoStructType : public CEL_STRUCT_TYPE_CLASS {
 private:
  template <typename T, typename R>
  using EnableIfDerivedMessage =
      std::enable_if_t<(!std::is_same_v<google::protobuf::Message, T> &&
                        std::is_base_of_v<google::protobuf::Message, T>),
                       R>;

 public:
  template <typename T>
  static EnableIfDerivedMessage<T, absl::StatusOr<Handle<ProtoStructType>>>
  Create(TypeFactory& type_factory);

  static absl::StatusOr<Handle<ProtoStructType>> Create(
      TypeFactory& type_factory, const google::protobuf::Descriptor* descriptor);

  absl::string_view name() const final { return descriptor().full_name(); }

  virtual const google::protobuf::Descriptor& descriptor() const = 0;

 protected:
  // Called by FindField.
  absl::StatusOr<absl::optional<Field>> FindFieldByName(
      TypeManager& type_manager, absl::string_view name) const final;

  // Called by FindField.
  absl::StatusOr<absl::optional<Field>> FindFieldByNumber(
      TypeManager& type_manager, int64_t number) const final;

 private:
  // Called by CEL_IMPLEMENT_STRUCT_TYPE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ProtoStructType>();
  }
};

namespace proto_internal {

template <typename T>
class StaticProtoStructType final : public ProtoStructType {
 public:
  static_assert(std::is_base_of_v<google::protobuf::Message, T>);

  StaticProtoStructType() = default;

  const google::protobuf::Descriptor& descriptor() const override {
    return *T::descriptor();
  }
};

class DynamicProtoStructType final : public ProtoStructType {
 public:
  explicit DynamicProtoStructType(const google::protobuf::Descriptor* descriptor)
      : descriptor_(ABSL_DIE_IF_NULL(descriptor)) {}  // Crash OK

  const google::protobuf::Descriptor& descriptor() const override { return *descriptor_; }

 private:
  const google::protobuf::Descriptor* const descriptor_;
};

}  // namespace proto_internal

template <typename T>
inline ProtoStructType::EnableIfDerivedMessage<
    T, absl::StatusOr<Handle<ProtoStructType>>>
ProtoStructType::Create(TypeFactory& type_factory) {
  return type_factory
      .CreateStructType<proto_internal::StaticProtoStructType<T>>();
}

inline absl::StatusOr<Handle<ProtoStructType>> ProtoStructType::Create(
    TypeFactory& type_factory, const google::protobuf::Descriptor* descriptor) {
  return type_factory.CreateStructType<proto_internal::DynamicProtoStructType>(
      descriptor);
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_STRUCT_TYPE_H_

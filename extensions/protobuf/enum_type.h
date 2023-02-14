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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_ENUM_TYPE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_ENUM_TYPE_H_

#include "base/type_factory.h"
#include "base/types/enum_type.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/generated_enum_util.h"

namespace cel::extensions {

class ProtoEnumType : public EnumType {
 private:
  template <typename T, typename R>
  using EnableIfEnum = std::enable_if_t<google::protobuf::is_proto_enum<T>::value, R>;

 public:
  template <typename T>
  static EnableIfEnum<T, absl::StatusOr<Handle<ProtoEnumType>>> Create(
      TypeFactory& type_factory);

  static absl::StatusOr<Handle<ProtoEnumType>> Create(
      TypeFactory& type_factory, const google::protobuf::EnumDescriptor* descriptor);

  absl::string_view name() const final { return descriptor().full_name(); }

  virtual const google::protobuf::EnumDescriptor& descriptor() const = 0;

 protected:
  // Called by FindField.
  absl::StatusOr<absl::optional<Constant>> FindConstantByName(
      absl::string_view name) const final;

  // Called by FindField.
  absl::StatusOr<absl::optional<Constant>> FindConstantByNumber(
      int64_t number) const final;

 private:
  // Called by CEL_IMPLEMENT_STRUCT_TYPE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ProtoEnumType>();
  }
};

namespace proto_internal {

template <typename T>
class StaticProtoEnumType final : public ProtoEnumType {
 public:
  static_assert(google::protobuf::is_proto_enum<T>::value);

  StaticProtoEnumType() = default;

  const google::protobuf::EnumDescriptor& descriptor() const override {
    return *google::protobuf::GetEnumDescriptor<T>();
  }
};

class DynamicProtoEnumType final : public ProtoEnumType {
 public:
  explicit DynamicProtoEnumType(const google::protobuf::EnumDescriptor* descriptor)
      : descriptor_(ABSL_DIE_IF_NULL(descriptor)) {}  // Crash OK

  const google::protobuf::EnumDescriptor& descriptor() const override {
    return *descriptor_;
  }

 private:
  const google::protobuf::EnumDescriptor* const descriptor_;
};

}  // namespace proto_internal

template <typename T>
inline ProtoEnumType::EnableIfEnum<T, absl::StatusOr<Handle<ProtoEnumType>>>
ProtoEnumType::Create(TypeFactory& type_factory) {
  return type_factory.CreateEnumType<proto_internal::StaticProtoEnumType<T>>();
}

inline absl::StatusOr<Handle<ProtoEnumType>> ProtoEnumType::Create(
    TypeFactory& type_factory, const google::protobuf::EnumDescriptor* descriptor) {
  return type_factory.CreateEnumType<proto_internal::DynamicProtoEnumType>(
      descriptor);
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_ENUM_TYPE_H_

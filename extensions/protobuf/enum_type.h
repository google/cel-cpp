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

#include "absl/base/attributes.h"
#include "absl/log/die_if_null.h"
#include "base/type.h"
#include "base/type_manager.h"
#include "base/types/enum_type.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "google/protobuf/generated_enum_util.h"

namespace cel::extensions {

class ProtoType;
class ProtoTypeProvider;

class ProtoEnumType final : public EnumType {
 public:
  static bool Is(const Type& type) {
    return type.kind() == Kind::kEnum &&
           cel::base_internal::GetEnumTypeTypeId(static_cast<const EnumType&>(
               type)) == cel::internal::TypeId<ProtoEnumType>();
  }

  using EnumType::Is;

  static const ProtoEnumType& Cast(const Type& type) {
    ABSL_ASSERT(Is(type));
    return static_cast<const ProtoEnumType&>(type);
  }

  absl::string_view name() const override { return descriptor().full_name(); }

  const google::protobuf::EnumDescriptor& descriptor() const { return *descriptor_; }

 protected:
  // Called by FindField.
  absl::StatusOr<absl::optional<Constant>> FindConstantByName(
      absl::string_view name) const override;

  // Called by FindField.
  absl::StatusOr<absl::optional<Constant>> FindConstantByNumber(
      int64_t number) const override;

 private:
  friend class ProtoType;
  friend class ProtoTypeProvider;
  friend class cel::MemoryManager;

  // Called by Arena-based memory managers to determine whether we actually need
  // our destructor called.
  static bool IsDestructorSkippable(
      const ProtoEnumType& type ABSL_ATTRIBUTE_UNUSED) noexcept {
    // Our destructor is useless, we only hold pointers to protobuf-owned data.
    return true;
  }

  template <typename T>
  static std::enable_if_t<google::protobuf::is_proto_enum<T>::value,
                          absl::StatusOr<Handle<ProtoEnumType>>>
  Resolve(TypeManager& type_manager) {
    return Resolve(type_manager, *google::protobuf::GetEnumDescriptor<T>());
  }

  static absl::StatusOr<Handle<ProtoEnumType>> Resolve(
      TypeManager& type_manager, const google::protobuf::EnumDescriptor& descriptor);

  explicit ProtoEnumType(const google::protobuf::EnumDescriptor* descriptor)
      : descriptor_(ABSL_DIE_IF_NULL(descriptor)) {}  // Crash OK.

  // Called by CEL_IMPLEMENT_STRUCT_TYPE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const override {
    return internal::TypeId<ProtoEnumType>();
  }

  const google::protobuf::EnumDescriptor* const descriptor_;
};

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_ENUM_TYPE_H_
